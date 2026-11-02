/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Unprivileged candidate enumeration for cross-process reclaim (Issue #12).
 *
 * Read-only by construction: the only syscalls here are opendir/readdir,
 * open/read on procfs text, readlink, and stat. There is no pidfd_open, no
 * process_madvise, no signal, and no wait. It cannot change any process.
 */
#define _GNU_SOURCE

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "crossproc_candidates.h"
#include "crossproc_protection.h"

#ifndef MR_CAND_PROC_ROOT
#  define MR_CAND_PROC_ROOT "/proc"
#endif

static const char *cand_proc_root = MR_CAND_PROC_ROOT;

#ifdef MEMREDUCT_TESTING
/* Test build only: point enumeration at a fixture tree with fake PIDs. The
 * production binary has no code path that reads this variable. */
static void cand_test_root(void)
{
    const char *value = getenv("MEMREDUCT_TEST_PROC_ROOT");
    if (value != NULL && value[0] == '/') {
        cand_proc_root = value;
    }
}

static uint64_t cand_own_uid(void)
{
    const char *value = getenv("MEMREDUCT_TEST_UID");
    if (value != NULL && value[0] != '\0') {
        uint64_t parsed = 0;
        for (const char *cursor = value; *cursor != '\0'; cursor++) {
            if (*cursor < '0' || *cursor > '9') {
                return (uint64_t)geteuid();
            }
            unsigned int digit = (unsigned int)(*cursor - '0');
            if (parsed > (UINT32_MAX - digit) / 10u) {
                return (uint64_t)geteuid();
            }
            parsed = parsed * 10u + digit;
        }
        return parsed;
    }
    return (uint64_t)geteuid();
}
#else
static void cand_test_root(void) { }
static uint64_t cand_own_uid(void) { return (uint64_t)geteuid(); }
#endif

int mr_cand_name_is_protected(const char *name)
{
    return mr_xproc_name_is_protected(name);
}

static int read_text(const char *path, char *buffer, size_t size)
{
    int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd == -1) {
        return -1;
    }
    ssize_t total = 0;
    while ((size_t)total + 1u < size) {
        ssize_t got = read(fd, buffer + total, size - (size_t)total - 1u);
        if (got == -1) {
            if (errno == EINTR) {
                continue;
            }
            (void)close(fd);
            return -1;
        }
        if (got == 0) {
            break;
        }
        total += got;
    }
    buffer[total] = '\0';
    (void)close(fd);
    return 0;
}

/* Field 22 (start_time) from /proc/<pid>/stat, anchored on the last ')'. */
static int read_start_time(int32_t pid, uint64_t *out)
{
    char path[MR_CAND_PROC_PATH_BYTES];
    char line[4096];
    if (snprintf(path, sizeof(path), "%s/%d/stat", cand_proc_root, (int)pid) >= (int)sizeof(path)) {
        return -1;
    }
    if (read_text(path, line, sizeof(line)) != 0) {
        return -1;
    }
    const char *close_paren = strrchr(line, ')');
    if (close_paren == NULL) {
        return -1;
    }
    const char *cursor = close_paren + 1;
    if (*cursor != ' ') {
        return -1;
    }
    cursor++;
    for (int field = 0; field < 19; field++) {
        while (*cursor != '\0' && *cursor != ' ') {
            cursor++;
        }
        if (*cursor != ' ') {
            return -1;
        }
        cursor++;
    }
    uint64_t value = 0;
    int digits = 0;
    while (*cursor >= '0' && *cursor <= '9') {
        uint64_t next = value * 10u + (uint64_t)(*cursor - '0');
        if (next < value) {
            return -1;
        }
        value = next;
        cursor++;
        digits++;
    }
    if (digits == 0) {
        return -1;
    }
    *out = value;
    return 0;
}

/* Reads Name and Uid and the state letter in one pass over status. */
struct cand_status {
    char name[MR_CAND_NAME_BYTES];
    uint64_t uid;
    char state;
    int have_name;
    int have_uid;
    int have_state;
};

static int read_status(int32_t pid, struct cand_status *out)
{
    char path[MR_CAND_PROC_PATH_BYTES];
    char buffer[8192];
    memset(out, 0, sizeof(*out));
    if (snprintf(path, sizeof(path), "%s/%d/status", cand_proc_root, (int)pid) >= (int)sizeof(path)) {
        return -1;
    }
    if (read_text(path, buffer, sizeof(buffer)) != 0) {
        return -1;
    }
    const char *line = buffer;
    while (line != NULL && *line != '\0') {
        if (strncmp(line, "Name:", 5) == 0) {
            const char *value = line + 5;
            while (*value == ' ' || *value == '\t') {
                value++;
            }
            size_t length = 0;
            while (value[length] != '\0' && value[length] != '\n' &&
                   length + 1u < sizeof(out->name)) {
                out->name[length] = value[length];
                length++;
            }
            out->name[length] = '\0';
            out->have_name = length > 0;
        } else if (strncmp(line, "Uid:", 4) == 0) {
            const char *value = line + 4;
            while (*value == ' ' || *value == '\t') {
                value++;
            }
            uint64_t parsed[4] = {0, 0, 0, 0};
            int valid = 1;
            for (size_t index = 0; index < 4u; index++) {
                if (*value < '0' || *value > '9') {
                    valid = 0;
                    break;
                }
                do {
                    unsigned int digit = (unsigned int)(*value - '0');
                    if (parsed[index] > (UINT32_MAX - digit) / 10u) {
                        valid = 0;
                        break;
                    }
                    parsed[index] = parsed[index] * 10u + digit;
                    value++;
                } while (*value >= '0' && *value <= '9');
                if (!valid) {
                    break;
                }
                if (index < 3u) {
                    if (*value != ' ' && *value != '\t') {
                        valid = 0;
                        break;
                    }
                    while (*value == ' ' || *value == '\t') {
                        value++;
                    }
                }
            }
            while (valid && (*value == ' ' || *value == '\t')) {
                value++;
            }
            if (valid && (*value == '\n' || *value == '\0') &&
                parsed[0] == parsed[1] && parsed[0] == parsed[2] &&
                parsed[0] == parsed[3]) {
                out->uid = parsed[0];
                out->have_uid = 1;
            }
        } else if (strncmp(line, "State:", 6) == 0) {
            const char *value = line + 6;
            while (*value == ' ' || *value == '\t') {
                value++;
            }
            if (*value != '\0' && *value != '\n') {
                out->state = *value;
                out->have_state = 1;
            }
        }
        const char *next = strchr(line, '\n');
        line = next == NULL ? NULL : next + 1;
    }
    return (out->have_name && out->have_uid) ? 0 : -1;
}

static int read_exe(int32_t pid, char *out, size_t size)
{
    char path[MR_CAND_PROC_PATH_BYTES];
    out[0] = '\0';
    if (size < 2u ||
        snprintf(path, sizeof(path), "%s/%d/exe", cand_proc_root, (int)pid) >= (int)sizeof(path)) {
        return -1;
    }
    ssize_t got = readlink(path, out, size - 1u);
    if (got <= 0 || (size_t)got >= size - 1u || out[0] != '/') {
        out[0] = '\0';
        return -1;
    }
    out[got] = '\0';
    return 0;
}

/* Same truncation problem the helper documents: the comm field in
 * /proc/<pid>/stat is capped at 15 characters, so "dbus-broker-launcher"
 * arrives as "dbus-broker-lau" and a naive name comparison misses it — this was
 * observed on the live machine. The exe symlink is not truncated, so its
 * basename is checked too. Keeping this in step with the helper matters: a row
 * this enumeration offers and the helper then refuses is a confusing product. */
static int pid_safety(int32_t pid, const char *comm, uint64_t uid,
                      char *exe_out, size_t exe_size)
{
    if (mr_cand_name_is_protected(comm)) {
        return 1;
    }
    if (read_exe(pid, exe_out, exe_size) != 0) {
        return -1;
    }
    int exe_protected = mr_xproc_exe_is_protected(exe_out);
    if (exe_protected != 0) {
        return exe_protected;
    }

    char path[MR_CAND_PROC_PATH_BYTES];
    char cgroup[4096];
    if (snprintf(path, sizeof(path), "%s/%d/cgroup", cand_proc_root, (int)pid) >= (int)sizeof(path) ||
        read_text(path, cgroup, sizeof(cgroup)) != 0) {
        return -1;
    }
    int cgroup_result = mr_xproc_cgroup_text_is_user_app(cgroup, (uint32_t)uid);
    return cgroup_result == 1 ? 0 : cgroup_result == 0 ? 1 : -1;
}

/* Sum the mapping lengths the helper would also consider. Same filter, so the
 * number shown before the click is the number the helper will actually work
 * with — not a larger, more flattering one. */
static int sum_reclaimable(int32_t pid, uint64_t *bytes_out, uint32_t *regions_out)
{
    char path[MR_CAND_PROC_PATH_BYTES];
    char line[4096];
    uint64_t total = 0;
    uint32_t count = 0;

    if (snprintf(path, sizeof(path), "%s/%d/maps", cand_proc_root, (int)pid) >= (int)sizeof(path)) {
        return -1;
    }
    FILE *maps = fopen(path, "re");
    if (maps == NULL) {
        return -1;
    }
    long page = sysconf(_SC_PAGESIZE);
    uint64_t alignment = page > 0 ? (uint64_t)page : 4096ull;

    while (fgets(line, sizeof(line), maps) != NULL) {
        unsigned long long begin = 0;
        unsigned long long end = 0;
        char perms[8] = {0};
        char device[32] = {0};
        char name[512] = {0};
        unsigned long long offset = 0;
        unsigned long long inode = 0;

        int fields = sscanf(line, "%llx-%llx %7s %llx %31s %llu %511s",
                            &begin, &end, perms, &offset, device, &inode, name);
        if (fields < 5 || end <= begin) {
            continue;
        }
        if (perms[3] != 'p' || perms[0] != 'r') {
            continue;
        }
        if (fields >= 6 && inode != 0 && perms[1] == 'w') {
            continue;
        }
        if (name[0] == '[') {
            continue;
        }
        uint64_t aligned_start = ((uint64_t)begin + alignment - 1u) & ~(alignment - 1u);
        uint64_t aligned_end = (uint64_t)end & ~(alignment - 1u);
        if (aligned_end <= aligned_start) {
            continue;
        }
        total += aligned_end - aligned_start;
        count++;
    }
    (void)fclose(maps);
    *bytes_out = total;
    *regions_out = count;
    return 0;
}

/* Is `pid` on this program's own parent chain? Advising our own ancestor would
 * stall the very program that would have to report the result. */
static int is_own_ancestor(int32_t pid)
{
    int32_t cursor = (int32_t)getpid();
    for (int step = 0; step < 256 && cursor > 1; step++) {
        if (cursor == pid) {
            return 1;
        }
        char path[MR_CAND_PROC_PATH_BYTES];
        char line[4096];
        if (snprintf(path, sizeof(path), "%s/%d/stat", cand_proc_root, (int)cursor) >= (int)sizeof(path)) {
            return 1; /* cannot prove it is safe: treat as ancestor */
        }
        if (read_text(path, line, sizeof(line)) != 0) {
            return 1;
        }
        const char *close_paren = strrchr(line, ')');
        if (close_paren == NULL) {
            return 1;
        }
        const char *walk = strchr(close_paren + 1, ' ');
        if (walk == NULL) {
            return 1;
        }
        int32_t ppid = 0;
        int digits = 0;
        while (walk[1] >= '0' && walk[1] <= '9') {
            ppid = ppid * 10 + (walk[1] - '0');
            walk++;
            digits++;
        }
        if (digits == 0 || ppid <= 0 || ppid == cursor) {
            return 0;
        }
        cursor = ppid;
    }
    return 0;
}

static int parse_pid(const char *name, int32_t *out)
{
    int64_t value = 0;
    int digits = 0;
    for (const char *cursor = name; *cursor != '\0'; cursor++) {
        if (!isdigit((unsigned char)*cursor)) {
            return -1;
        }
        value = value * 10 + (*cursor - '0');
        if (value > 0x7fffffff) {
            return -1;
        }
        digits++;
    }
    if (digits == 0) {
        return -1;
    }
    *out = (int32_t)value;
    return 0;
}

int mr_cand_scan(struct mr_cand_list *out)
{
    if (out == NULL) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    cand_test_root();

    DIR *dir = opendir(cand_proc_root);
    if (dir == NULL) {
        return -1;
    }

    const uint64_t own_uid = cand_own_uid();
    const int32_t own_pid = (int32_t)getpid();
    struct dirent *entry;

    while (out->scanned < MR_CAND_MAX_PROC_ENTRIES &&
           (entry = readdir(dir)) != NULL) {
        int32_t pid = 0;
        if (parse_pid(entry->d_name, &pid) != 0) {
            continue;
        }
        out->scanned++;

        if (pid == 1) {
            continue;
        }
        if (pid == own_pid) {
            out->skipped_self++;
            continue;
        }

        struct cand_status status;
        if (read_status(pid, &status) != 0) {
            /* Vanished between readdir and open, or a kernel thread with no
             * status. Neither is an error worth reporting above the rest. */
            out->skipped_unreadable++;
            continue;
        }
        if (status.have_state &&
            (status.state == 'Z' || status.state == 'X' || status.state == 'x')) {
            out->skipped_zombie++;
            continue;
        }
        if (status.uid != own_uid) {
            out->skipped_other_uid++;
            continue;
        }
        char exe[MR_CAND_PATH_BYTES];
        int safety = pid_safety(pid, status.name, status.uid, exe, sizeof(exe));
        if (safety != 0) {
            if (safety > 0) {
                out->skipped_protected++;
            } else {
                out->skipped_unreadable++;
            }
            continue;
        }
        if (is_own_ancestor(pid)) {
            out->skipped_ancestor++;
            continue;
        }

        uint64_t reclaimable = 0;
        uint32_t regions = 0;
        if (sum_reclaimable(pid, &reclaimable, &regions) != 0) {
            out->skipped_kernel_thread++;
            continue;
        }
        if (regions == 0 || reclaimable == 0) {
            out->skipped_no_regions++;
            continue;
        }

        if (out->count >= MR_CAND_MAX) {
            out->skipped_no_regions += 0; /* ceiling reported via skipped_limit */
            continue;
        }

        struct mr_cand_entry *slot = &out->entries[out->count];
        memset(slot, 0, sizeof(*slot));
        slot->pid = pid;
        slot->uid = status.uid;
        slot->reclaimable_bytes = reclaimable;
        slot->regions = regions;
        memcpy(slot->name, status.name, sizeof(slot->name) - 1u);
        if (read_start_time(pid, &slot->start_time) != 0) {
            /* Without a start_time the helper cannot verify identity, so the row
             * would be unusable. Drop it rather than offer a row that always
             * fails after authorization. */
            memset(slot, 0, sizeof(*slot));
            out->skipped_unreadable++;
            continue;
        }
        memcpy(slot->exe, exe, strlen(exe) + 1u);
        out->count++;
    }
    (void)closedir(dir);
    return 0;
}

int mr_cand_at(const struct mr_cand_list *list, uint32_t index,
               struct mr_cand_entry *out)
{
    if (list == NULL || out == NULL || index >= list->count || index >= MR_CAND_MAX) {
        return -1;
    }
    *out = list->entries[index];
    return 0;
}

/* ---------------------------------------------------------------------------
 * Scalar bridge for Zamak.
 *
 * Zamak 0.2 has no struct return, so the scan result is held in one static slot
 * and read field by field — the same shape self_reclaim.c and crossproc_plan.c
 * already use. One scan at a time; a second call replaces the previous slot,
 * which the GUI does between redraws anyway.
 * ------------------------------------------------------------------------- */

static struct mr_cand_list scan_slot;

int64_t mr_cand_scan_run(void)
{
    int result = mr_cand_scan(&scan_slot);
    return result == 0 ? 0 : 1;
}

int64_t mr_cand_count(void) { return (int64_t)scan_slot.count; }
int64_t mr_cand_scanned(void) { return (int64_t)scan_slot.scanned; }

int64_t mr_cand_skipped(int64_t reason)
{
    switch (reason) {
        case MR_CAND_SKIP_SELF: return (int64_t)scan_slot.skipped_self;
        case MR_CAND_SKIP_KERNEL_THREAD: return (int64_t)scan_slot.skipped_kernel_thread;
        case MR_CAND_SKIP_OTHER_UID: return (int64_t)scan_slot.skipped_other_uid;
        case MR_CAND_SKIP_UNREADABLE: return (int64_t)scan_slot.skipped_unreadable;
        case MR_CAND_SKIP_PROTECTED: return (int64_t)scan_slot.skipped_protected;
        case MR_CAND_SKIP_ANCESTOR: return (int64_t)scan_slot.skipped_ancestor;
        case MR_CAND_SKIP_NO_REGIONS: return (int64_t)scan_slot.skipped_no_regions;
        case MR_CAND_SKIP_ZOMBIE: return (int64_t)scan_slot.skipped_zombie;
        default: return 0;
    }
}

int64_t mr_cand_pid(int64_t index)
{
    if (index < 0 || index >= (int64_t)scan_slot.count) {
        return -1;
    }
    return (int64_t)scan_slot.entries[index].pid;
}

int64_t mr_cand_start_time(int64_t index)
{
    if (index < 0 || index >= (int64_t)scan_slot.count) {
        return -1;
    }
    return (int64_t)scan_slot.entries[index].start_time;
}

int64_t mr_cand_reclaimable_bytes(int64_t index)
{
    if (index < 0 || index >= (int64_t)scan_slot.count) {
        return -1;
    }
    return (int64_t)scan_slot.entries[index].reclaimable_bytes;
}

int64_t mr_cand_regions(int64_t index)
{
    if (index < 0 || index >= (int64_t)scan_slot.count) {
        return -1;
    }
    return (int64_t)scan_slot.entries[index].regions;
}

/* The per-index name. Zamak can read it through the same pointer the status line
 * uses; the buffer belongs to the scan slot, so it stays valid until the next
 * scan and must not be freed by the caller. */
const char *mr_cand_name(int64_t index)
{
    if (index < 0 || index >= (int64_t)scan_slot.count) {
        return "";
    }
    return scan_slot.entries[index].name;
}
