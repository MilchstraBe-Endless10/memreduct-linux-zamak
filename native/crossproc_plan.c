/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Read-only planning/discovery for the cross-process workingset advisory
 * prototype (Issue #12, ADR-0007).
 *
 * Nothing in this file writes to procfs, to the kernel, or to any process. It
 * reads a procfs root, chooses targets and ranges under fixed ceilings, pins
 * each chosen process with pidfd_open, and hands the result to a caller that may
 * or may not be allowed to act on it. The writable half lives in
 * crossproc_advise.c behind two independent gates.
 *
 * Why the shape is what it is:
 *   - process_madvise() takes a pidfd, so a target's identity is pinned before
 *     any decision is made about it. A PID number alone is not an identity.
 *   - The returned byte count is the length the kernel covered, not the amount
 *     reclaimed. This module therefore never produces a "freed" figure.
 *   - The scan is bounded in every dimension, so the cost of a hostile procfs
 *     (thousands of entries, huge mappings) cannot be used to stall the caller.
 */
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include "crossproc_plan.h"

#ifndef __NR_pidfd_open
#  define __NR_pidfd_open 434
#endif

#ifndef __NR_process_madvise
#  if defined(__x86_64__) || defined(__aarch64__)
#    define __NR_process_madvise 440
#  else
#    define __NR_process_madvise (-1)
#  endif
#endif

/* Advice values cached here so old headers still compile. */
#ifndef MADV_COLD
#  define MADV_COLD 20
#endif
#ifndef MADV_PAGEOUT
#  define MADV_PAGEOUT 21
#endif

/* Production reads the real procfs. Only a MEMREDUCT_TESTING build may point
 * the scan at a fixture tree, and even then the root must be absolute. */
static const char *proc_root(void)
{
#ifdef MEMREDUCT_TESTING
    const char *override = getenv("MEMREDUCT_TEST_PROC_ROOT");
    if (override != NULL && override[0] == '/') {
        return override;
    }
#endif
    return "/proc";
}

/* Re-entrancy guard. The plan builder is not reentrant: it holds one static
 * slot for the scalar bridge, and a second concurrent build would overwrite the
 * first caller's result. */
static int build_in_progress;

/* Test-only pidfd_open replacement. A fixture tree names PIDs that do not exist,
 * so the real syscall would answer ESRCH for every one of them and the planning
 * logic could never be exercised. */
#ifdef MEMREDUCT_TESTING
static mr_crossproc_pidfd_hook pidfd_hook;
static int32_t pidfd_open_hook_calls;

void mr_crossproc_set_pidfd_hook(mr_crossproc_pidfd_hook hook)
{
    pidfd_hook = hook;
    pidfd_open_hook_calls = 0;
}

int32_t mr_crossproc_test_pidfd_calls(void)
{
    return pidfd_open_hook_calls;
}
#endif

static int32_t open_pidfd(int32_t pid)
{
#ifdef MEMREDUCT_TESTING
    if (pidfd_hook != NULL) {
        pidfd_open_hook_calls++;
        return pidfd_hook(pid);
    }
#endif
    return (int32_t)syscall(__NR_pidfd_open, (pid_t)pid, 0u);
}

static struct mr_crossproc_plan last_plan;

static uint64_t monotonic_ms(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0;
    }
    return (uint64_t)now.tv_sec * 1000ull + (uint64_t)now.tv_nsec / 1000000ull;
}

static void clamp_limits(const struct mr_crossproc_limits *requested,
                         struct mr_crossproc_limits *effective)
{
    memset(effective, 0, sizeof(*effective));
    if (requested == NULL) {
        effective->max_targets = MR_CROSSPROC_MAX_TARGETS;
        effective->max_regions = MR_CROSSPROC_MAX_REGIONS;
        effective->max_total_bytes = MR_CROSSPROC_MAX_TOTAL_BYTES;
        effective->budget_ms = MR_CROSSPROC_TIME_BUDGET_MS;
        effective->advice = MR_CROSSPROC_ADVICE_COLD;
        return;
    }
    effective->max_targets = requested->max_targets;
    if (effective->max_targets == 0 || effective->max_targets > MR_CROSSPROC_MAX_TARGETS) {
        effective->max_targets = MR_CROSSPROC_MAX_TARGETS;
    }
    effective->max_regions = requested->max_regions;
    if (effective->max_regions == 0 || effective->max_regions > MR_CROSSPROC_MAX_REGIONS) {
        effective->max_regions = MR_CROSSPROC_MAX_REGIONS;
    }
    effective->max_total_bytes = requested->max_total_bytes;
    if (effective->max_total_bytes == 0 ||
        effective->max_total_bytes > MR_CROSSPROC_MAX_TOTAL_BYTES) {
        effective->max_total_bytes = MR_CROSSPROC_MAX_TOTAL_BYTES;
    }
    effective->budget_ms = requested->budget_ms;
    if (effective->budget_ms == 0 || effective->budget_ms > MR_CROSSPROC_TIME_BUDGET_MS) {
        effective->budget_ms = MR_CROSSPROC_TIME_BUDGET_MS;
    }
    effective->advice = requested->advice;
}

static int advice_is_valid(int32_t advice)
{
    return advice == MR_CROSSPROC_ADVICE_COLD || advice == MR_CROSSPROC_ADVICE_PAGEOUT;
}


/* Build "<root>/<pid><suffix>" and refuse anything that does not fit.
 * snprintf() truncates silently and returns the length it wanted, so a long
 * proc root would otherwise read a path that names a different file. The
 * caller treats a short buffer as a hard failure, never as "process gone". */
static int proc_path(char *out, size_t out_size, int32_t pid, const char *suffix)
{
    int written = snprintf(out, out_size, "%s/%d%s", proc_root(), (int)pid, suffix);
    return written > 0 && (size_t)written < out_size;
}

/* /proc/<pid>/stat field 22 is the process start time in clock ticks. It only
 * changes when a different process is created, which is exactly the condition
 * that makes an old pidfd unusable. Parsing it avoids depending on a synced
 * /proc/<pid>/stat and is stable across kernels. */
static int read_start_time(int32_t pid, uint64_t *out)
{
    char path[MR_CROSSPROC_PATH_BYTES];
    char buffer[1024];
    if (!proc_path(path, sizeof(path), pid, "/stat")) {
        return -1;
    }
    FILE *file = fopen(path, "re");
    if (file == NULL) {
        return errno == ENOENT ? 1 : -1; /* 1 = gone */
    }
    size_t read = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    buffer[read] = '\0';

    /* The comm field may contain spaces and parentheses, so counting spaces is
     * only safe after the closing ')'. Field 3 begins right after it. */
    char *cursor = strrchr(buffer, ')');
    if (cursor == NULL) {
        return -1;
    }
    /* Skip fields 3..21 to reach field 22. Each iteration must consume one
     * whole field: strchr() from the *start* of a field finds the separator
     * that terminates it, so advancing one past it both consumes that field and
     * lands on the next. Advancing twice per iteration (as an earlier version
     * did) skipped two fields at a time and ran off the end of the buffer, which
     * made every process look unreadable and emptied the whole plan. */
    for (int field = 3; field < 22; field++) {
        while (*cursor == ' ') {
            cursor++;
        }
        if (*cursor == '\0') {
            return -1;
        }
        cursor = strchr(cursor, ' ');
        if (cursor == NULL) {
            return -1;
        }
        cursor++;
    }
    char *end = NULL;
    errno = 0;
    unsigned long long value = strtoull(cursor, &end, 10);
    if (errno != 0 || end == cursor) {
        return -1;
    }
    *out = (uint64_t)value;
    return 0;
}

/* Kernel threads have no address space, so no maps file. That is the cheapest
 * and most reliable kernel-thread test available without privileges. */
static int is_kernel_thread(int32_t pid)
{
    char path[MR_CROSSPROC_PATH_BYTES];
    if (!proc_path(path, sizeof(path), pid, "/maps")) {
        return 0;
    }
    return access(path, R_OK) != 0;
}

#ifdef MEMREDUCT_TESTING
/* Test-only injection point for the ownership check. A fixture tree cannot
 * chown its directories to another user without privileges, and a test that
 * needs root to prove "other users are skipped" would be a bad trade. The hook
 * answers the same question the stat() below answers, so the production
 * decision path stays exactly one stat(). Never compiled into a release build.
 * Returns the owner uid for that pid, or -1 if it cannot be determined. */
typedef int32_t (*mr_crossproc_uid_hook)(int32_t pid);
static mr_crossproc_uid_hook test_uid_hook;

void mr_crossproc_set_uid_hook(mr_crossproc_uid_hook hook)
{
    test_uid_hook = hook;
}
#endif

/* 0 = same uid, 1 = other uid, -1 = unreadable. Unreadable is treated as
 * "other": the safe answer when the caller cannot prove ownership. */
static int uid_relation(int32_t pid)
{
    char path[MR_CROSSPROC_PATH_BYTES];
    struct stat info;
#ifdef MEMREDUCT_TESTING
    if (test_uid_hook != NULL) {
        int32_t owner = test_uid_hook(pid);
        if (owner < 0) {
            return -1;
        }
        return owner == (int32_t)geteuid() ? 0 : 1;
    }
#endif
    if (!proc_path(path, sizeof(path), pid, "")) {
        return -1;
    }
    if (stat(path, &info) != 0) {
        return -1;
    }
    return info.st_uid == geteuid() ? 0 : 1;
}

static int mapping_is_eligible(const char *line, uint64_t *start, uint64_t *end,
                               int32_t *reason)
{
    unsigned long long begin = 0, finish = 0;
    char perms[8] = {0};
    char offset[32] = {0};
    char device[32] = {0};
    unsigned long long inode = 0;
    char path[512] = {0};

    int fields = sscanf(line, "%llx-%llx %7s %31s %31s %llu %511s",
                        &begin, &finish, perms, offset, device, &inode, path);
    if (fields < 5) {
        /* Blank line, or not a maps line at all. */
        *reason = MR_CROSSPROC_SKIP_UNREADABLE;
        return 0;
    }
    if (finish <= begin) {
        *reason = MR_CROSSPROC_SKIP_UNREADABLE;
        return 0;
    }

    /* Private anonymous or private file-backed only. A shared mapping is also
     * mapped elsewhere; pushing its pages out would affect another owner. */
    if (perms[0] != 'r' || perms[3] != 'p') {
        *reason = MR_CROSSPROC_SKIP_SHARED;
        return 0;
    }
    /* A writable file-backed mapping may hold dirty data; reclaiming it forces
     * writeback from this application's decision, which nobody asked for. */
    if (fields >= 6 && inode != 0 && perms[1] == 'w') {
        *reason = MR_CROSSPROC_SKIP_WRITABLE;
        return 0;
    }

    if (fields >= 7 && path[0] != '\0') {
        static const char *const special[] = {
            "[vdso]", "[vvar]", "[vsyscall]", "[stack]", "[vectors]", "[sigpage]", NULL
        };
        for (size_t index = 0; special[index] != NULL; index++) {
            if (strcmp(path, special[index]) == 0) {
                *reason = MR_CROSSPROC_SKIP_SPECIAL;
                return 0;
            }
        }
    }

    *start = (uint64_t)begin;
    *end = (uint64_t)finish;
    *reason = MR_CROSSPROC_SKIP_NONE;
    return 1;
}

/* Fill regions for one pinned target. Returns the number of regions kept and
 * writes the byte total. On a partial read the target keeps what it has. */
/* Declared ahead of its first use: collect_regions() reports the oversized
 * case through the same counter that the scan loop feeds. */
static void count_skip(struct mr_crossproc_plan *plan, int32_t reason);

static uint32_t collect_regions(int32_t pid, const struct mr_crossproc_limits *limits,
                                uint64_t remaining_bytes, uint64_t *bytes_out,
                                struct mr_crossproc_region *regions,
                                struct mr_crossproc_plan *plan)
{
    char path[MR_CROSSPROC_PATH_BYTES];
    char line[1024];
    uint32_t kept = 0;
    uint64_t total = 0;

    if (!proc_path(path, sizeof(path), pid, "/maps")) {
        return 0;
    }
    FILE *maps = fopen(path, "re");
    if (maps == NULL) {
        return 0;
    }

    while (kept < limits->max_regions && fgets(line, sizeof(line), maps) != NULL) {
        uint64_t start = 0;
        uint64_t end = 0;
        int32_t reason = MR_CROSSPROC_SKIP_NONE;
        if (!mapping_is_eligible(line, &start, &end, &reason)) {
            /* A blank or unparseable line is not a shared mapping. Counting it
             * as one made skipped_shared report 7 for a six-line maps file and
             * turned a diagnosis of "what did we skip" into noise. */
            switch (reason) {
                case MR_CROSSPROC_SKIP_WRITABLE: plan->skipped_writable++; break;
                case MR_CROSSPROC_SKIP_SPECIAL: plan->skipped_special++; break;
                case MR_CROSSPROC_SKIP_SHARED: plan->skipped_shared++; break;
                default: break;
            }
            continue;
        }
        uint64_t length = end - start;
        if (length > MR_CROSSPROC_MAX_REGION_BYTES) {
            /* Over the single-region ceiling: leave it alone, but say so. A
             * silent skip would make an oversized process look ineligible. */
            count_skip(plan, MR_CROSSPROC_SKIP_TOO_LARGE);
            continue;
        }
        if (length > remaining_bytes - total) {
            break; /* the remaining budget cannot cover this range */
        }
        regions[kept].start = start;
        regions[kept].end = end;
        regions[kept].advice = limits->advice;
        regions[kept].reserved = 0;
        kept++;
        total += length;
    }
    fclose(maps);

    *bytes_out = total;
    return kept;
}

static void count_skip(struct mr_crossproc_plan *plan, int32_t reason)
{
    switch (reason) {
        case MR_CROSSPROC_SKIP_PID1: plan->skipped_pid1++; break;
        case MR_CROSSPROC_SKIP_SELF: plan->skipped_self++; break;
        case MR_CROSSPROC_SKIP_KERNEL_THREAD: plan->skipped_kernel_thread++; break;
        case MR_CROSSPROC_SKIP_OTHER_UID: plan->skipped_other_uid++; break;
        case MR_CROSSPROC_SKIP_EXITED: plan->skipped_exited++; break;
        case MR_CROSSPROC_SKIP_PID_REUSED: plan->skipped_pid_reused++; break;
        case MR_CROSSPROC_SKIP_TOO_LARGE: plan->skipped_too_large++; break;
        case MR_CROSSPROC_SKIP_BUDGET: plan->skipped_budget++; break;
        case MR_CROSSPROC_SKIP_UNREADABLE: plan->skipped_unreadable++; break;
        default: break;
    }
}

/* Decide whether one PID can become a target, without touching it. */
static int32_t classify(int32_t pid, const char *proc_root_path)
{
    (void)proc_root_path;
    if (pid == 1) {
        return MR_CROSSPROC_SKIP_PID1;
    }
    if (pid == (int32_t)getpid()) {
        return MR_CROSSPROC_SKIP_SELF;
    }
    if (is_kernel_thread(pid)) {
        return MR_CROSSPROC_SKIP_KERNEL_THREAD;
    }
    int relation = uid_relation(pid);
    if (relation != 0) {
        return MR_CROSSPROC_SKIP_OTHER_UID; /* unreadable counts as "not ours" */
    }
    return MR_CROSSPROC_SKIP_NONE;
}

static int parse_pid(const char *name, int32_t *out)
{
    char *end = NULL;
    errno = 0;
    long value = strtol(name, &end, 10);
    if (errno != 0 || end == name || *end != '\0' || value <= 0 || value > 2147483647L) {
        return 0;
    }
    *out = (int32_t)value;
    return 1;
}

int mr_crossproc_plan_build(const struct mr_crossproc_limits *limits,
                            struct mr_crossproc_plan *plan)
{
    if (plan == NULL) {
        return -1;
    }
    memset(plan, 0, sizeof(*plan));
    plan->status = MR_CROSSPROC_OK;

    if (build_in_progress) {
        plan->status = MR_CROSSPROC_REENTRANT;
        return 0;
    }
    build_in_progress = 1;

    struct mr_crossproc_limits effective;
    clamp_limits(limits, &effective);
    plan->advice = effective.advice;

    if (!advice_is_valid(effective.advice)) {
        plan->status = MR_CROSSPROC_INVALID_ARGUMENT;
        build_in_progress = 0;
        return 0;
    }
#if __NR_process_madvise < 0
    plan->status = MR_CROSSPROC_UNSUPPORTED;
    build_in_progress = 0;
    return 0;
#endif

    DIR *directory = opendir(proc_root());
    if (directory == NULL) {
        plan->status = MR_CROSSPROC_PROC_UNAVAILABLE;
        plan->error_code = errno;
        build_in_progress = 0;
        return 0;
    }

    struct dirent *entry = NULL;
    uint64_t deadline = monotonic_ms() + effective.budget_ms;
    int stopped_early = 0;
    /* Separate cursor: plan->regions is the total of kept regions and must not
     * also serve as an array index. */
    uint32_t region_cursor = 0;

    /* Bounded scan: at most MR_CROSSPROC_MAX_PROC_ENTRIES entries are examined,
     * and the loop stops as soon as the byte or target budget is spent. There is
     * deliberately no way to ask for a full sweep. */
    while (plan->scanned < MR_CROSSPROC_MAX_PROC_ENTRIES && (entry = readdir(directory)) != NULL) {
        int32_t pid = 0;
        if (!parse_pid(entry->d_name, &pid)) {
            continue;
        }
        plan->scanned++;
        if (plan->targets >= effective.max_targets ||
            plan->bytes_planned >= effective.max_total_bytes) {
            stopped_early = 1;
            break;
        }
        if (monotonic_ms() > deadline) {
            stopped_early = 1;
            break;
        }

        int32_t reason = classify(pid, proc_root());
        if (reason != MR_CROSSPROC_SKIP_NONE) {
            count_skip(plan, reason);
            continue;
        }

        int32_t pidfd = open_pidfd(pid);
        if (pidfd < 0) {
            if (errno == ESRCH) {
                count_skip(plan, MR_CROSSPROC_SKIP_EXITED);
            } else if (errno == EPERM || errno == EACCES) {
                /* The kernel refuses to hand us a handle for this process.
                 * Report it; never fall back to a raw PID. */
                count_skip(plan, MR_CROSSPROC_SKIP_OTHER_UID);
            } else {
                count_skip(plan, MR_CROSSPROC_SKIP_UNREADABLE);
            }
            continue;
        }

        uint64_t start_before = 0;
        int started = read_start_time(pid, &start_before);
        if (started == 1) {
            count_skip(plan, MR_CROSSPROC_SKIP_EXITED);
            close(pidfd);
            continue;
        }
        if (started < 0) {
            count_skip(plan, MR_CROSSPROC_SKIP_UNREADABLE);
            close(pidfd);
            continue;
        }

        uint64_t bytes = 0;
        uint32_t kept = collect_regions(pid, &effective, effective.max_total_bytes - plan->bytes_planned,
                                        &bytes, &plan->region_store[region_cursor],
                                        plan);
        if (kept == 0) {
            /* Nothing adviseable in this process; its pidfd is not needed. */
            close(pidfd);
            continue;
        }

        /* Re-read the identity after the map scan. A process that exited and
         * whose number was reused between the two reads must not be advised. */
        uint64_t start_after = 0;
        if (read_start_time(pid, &start_after) != 0 || start_after != start_before) {
            count_skip(plan, MR_CROSSPROC_SKIP_PID_REUSED);
            close(pidfd);
            memset(&plan->region_store[region_cursor], 0,
                   kept * sizeof(struct mr_crossproc_region));
            continue;
        }

        struct mr_crossproc_target *target = &plan->entries[plan->targets];
        target->pid = pid;
        target->pidfd = pidfd;
        target->start_time = start_before;
        target->region_offset = region_cursor;
        target->region_count = kept;
        target->bytes = bytes;
        plan->targets++;
        region_cursor += kept;
        plan->regions = region_cursor;
        plan->bytes_planned += bytes;
    }
    closedir(directory);

    if (plan->targets == 0) {
        plan->status = MR_CROSSPROC_NO_CANDIDATES;
    } else if (stopped_early) {
        plan->status = MR_CROSSPROC_BUDGET_EXHAUSTED;
    }

    build_in_progress = 0;
    return 0;
}

void mr_crossproc_plan_release(struct mr_crossproc_plan *plan)
{
    if (plan == NULL) {
        return;
    }
    for (uint32_t index = 0; index < plan->targets && index < MR_CROSSPROC_MAX_TARGETS; index++) {
        if (plan->entries[index].pidfd >= 0) {
            close(plan->entries[index].pidfd);
            plan->entries[index].pidfd = -1;
        }
    }
    plan->targets = 0;
    plan->regions = 0;
    plan->bytes_planned = 0;
}

uint32_t mr_crossproc_plan_region_count(const struct mr_crossproc_plan *plan)
{
    return plan == NULL ? 0u : plan->regions;
}

int mr_crossproc_plan_region_at(const struct mr_crossproc_plan *plan,
                                uint32_t target_index, uint32_t region_index,
                                struct mr_crossproc_region *out)
{
    if (plan == NULL || out == NULL) {
        return -1;
    }
    if (target_index >= plan->targets || target_index >= MR_CROSSPROC_MAX_TARGETS) {
        return -1;
    }
    const struct mr_crossproc_target *target = &plan->entries[target_index];
    if (region_index >= target->region_count) {
        return -1;
    }
    uint32_t absolute = target->region_offset + region_index;
    if (absolute >= plan->regions) {
        return -1;
    }
    *out = plan->region_store[absolute];
    return 0;
}

#ifdef MEMREDUCT_TESTING
/* Direct inspection of an arbitrary plan, so a test does not have to go through
 * the single static bridge slot. Test builds only. */
int64_t mr_crossproc_plan_status_of(const struct mr_crossproc_plan *plan)
{
    return plan == NULL ? -1 : plan->status;
}

int64_t mr_crossproc_plan_targets_of(const struct mr_crossproc_plan *plan)
{
    return plan == NULL ? -1 : (int64_t)plan->targets;
}

int64_t mr_crossproc_plan_regions_of(const struct mr_crossproc_plan *plan)
{
    return plan == NULL ? -1 : (int64_t)plan->regions;
}

int64_t mr_crossproc_plan_bytes_of(const struct mr_crossproc_plan *plan)
{
    return plan == NULL ? -1 : (int64_t)plan->bytes_planned;
}

int64_t mr_crossproc_plan_scanned_of(const struct mr_crossproc_plan *plan)
{
    return plan == NULL ? -1 : (int64_t)plan->scanned;
}

/* Count for one enum mr_crossproc_skip value, so a test names the reason
 * instead of depending on member order. */
int64_t mr_crossproc_plan_skipped_of(const struct mr_crossproc_plan *plan, int64_t reason)
{
    if (plan == NULL) {
        return -1;
    }
    switch (reason) {
        case MR_CROSSPROC_SKIP_PID1: return (int64_t)plan->skipped_pid1;
        case MR_CROSSPROC_SKIP_SELF: return (int64_t)plan->skipped_self;
        case MR_CROSSPROC_SKIP_KERNEL_THREAD: return (int64_t)plan->skipped_kernel_thread;
        case MR_CROSSPROC_SKIP_OTHER_UID: return (int64_t)plan->skipped_other_uid;
        case MR_CROSSPROC_SKIP_EXITED: return (int64_t)plan->skipped_exited;
        case MR_CROSSPROC_SKIP_PID_REUSED: return (int64_t)plan->skipped_pid_reused;
        case MR_CROSSPROC_SKIP_SHARED: return (int64_t)plan->skipped_shared;
        case MR_CROSSPROC_SKIP_WRITABLE: return (int64_t)plan->skipped_writable;
        case MR_CROSSPROC_SKIP_SPECIAL: return (int64_t)plan->skipped_special;
        case MR_CROSSPROC_SKIP_BUDGET: return (int64_t)plan->skipped_budget;
        case MR_CROSSPROC_SKIP_UNREADABLE: return (int64_t)plan->skipped_unreadable;
        default: return 0;
    }
}

/* Region i of target t, flattened for ctypes. */
int64_t mr_crossproc_plan_region_start(const struct mr_crossproc_plan *plan,
                                       uint32_t target_index, uint32_t region_index)
{
    struct mr_crossproc_region region;
    if (mr_crossproc_plan_region_at(plan, target_index, region_index, &region) != 0) {
        return -1;
    }
    return (int64_t)region.start;
}

int64_t mr_crossproc_plan_region_end(const struct mr_crossproc_plan *plan,
                                     uint32_t target_index, uint32_t region_index)
{
    struct mr_crossproc_region region;
    if (mr_crossproc_plan_region_at(plan, target_index, region_index, &region) != 0) {
        return -1;
    }
    return (int64_t)region.end;
}

int64_t mr_crossproc_plan_target_pid(const struct mr_crossproc_plan *plan, uint32_t index)
{
    if (plan == NULL || index >= plan->targets) {
        return -1;
    }
    return (int64_t)plan->entries[index].pid;
}
#endif /* MEMREDUCT_TESTING */

/* Scalar bridge: one run, then fields of that same run. */
int64_t mr_crossproc_plan_status(void)
{
    const struct mr_crossproc_limits limits = {
        MR_CROSSPROC_MAX_TARGETS, MR_CROSSPROC_MAX_REGIONS,
        MR_CROSSPROC_MAX_TOTAL_BYTES, MR_CROSSPROC_TIME_BUDGET_MS,
        MR_CROSSPROC_ADVICE_COLD
    };
    if (build_in_progress) {
        return MR_CROSSPROC_REENTRANT;
    }
    mr_crossproc_plan_release(&last_plan);
    (void)mr_crossproc_plan_build(&limits, &last_plan);
    return last_plan.status;
}

int64_t mr_crossproc_plan_targets(void)
{
    return (int64_t)last_plan.targets;
}

int64_t mr_crossproc_plan_regions(void)
{
    return (int64_t)last_plan.regions;
}

int64_t mr_crossproc_plan_bytes(void)
{
    return (int64_t)last_plan.bytes_planned;
}

int64_t mr_crossproc_plan_error(void)
{
    return (int64_t)last_plan.error_code;
}

/* Why a candidate was dropped, counted over the same run. Kept on the default
 * build so the result status is explainable, not just a number. */
int64_t mr_crossproc_plan_skipped(int64_t reason)
{
    const struct mr_crossproc_plan *plan = &last_plan;
    switch (reason) {
        case MR_CROSSPROC_SKIP_PID1: return (int64_t)plan->skipped_pid1;
        case MR_CROSSPROC_SKIP_SELF: return (int64_t)plan->skipped_self;
        case MR_CROSSPROC_SKIP_KERNEL_THREAD: return (int64_t)plan->skipped_kernel_thread;
        case MR_CROSSPROC_SKIP_OTHER_UID: return (int64_t)plan->skipped_other_uid;
        case MR_CROSSPROC_SKIP_EXITED: return (int64_t)plan->skipped_exited;
        case MR_CROSSPROC_SKIP_PID_REUSED: return (int64_t)plan->skipped_pid_reused;
        case MR_CROSSPROC_SKIP_SHARED: return (int64_t)plan->skipped_shared;
        case MR_CROSSPROC_SKIP_WRITABLE: return (int64_t)plan->skipped_writable;
        case MR_CROSSPROC_SKIP_SPECIAL: return (int64_t)plan->skipped_special;
        case MR_CROSSPROC_SKIP_TOO_LARGE: return (int64_t)plan->skipped_too_large;
        case MR_CROSSPROC_SKIP_BUDGET: return (int64_t)plan->skipped_budget;
        case MR_CROSSPROC_SKIP_UNREADABLE: return (int64_t)plan->skipped_unreadable;
        default: return -1;
    }
}
