/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Mem Reduct Zamak — cross-process reclaim helper (Issue #12, M1.1).
 *
 * This is a SEPARATE privileged helper. It shares no code, no path and no
 * Polkit action with the page-cache helper (native/memreduct_helper.c). The
 * page-cache helper must never grow into a generic "run this for root"
 * executor, and this one must never be reachable through the page-cache
 * action.
 *
 * Threat model in one paragraph: the GUI is unprivileged and the PID it sends
 * is untrusted. Everything that decides whether memory is touched is re-derived
 * from the kernel here: the requester's UID, the target's real UID, its
 * start_time, its liveness, its ancestry, and its actual mappings. Nothing is
 * accepted from argv except three decimal numbers and one fixed verb. There is
 * no path argument, no shell, no address range, no signal, and no retry.
 *
 * The kernel only accepts an advice request. The number it returns is a length
 * it covered, never an amount of memory that stayed out. Every output line that
 * could be misread as "freed" says what it actually is.
 */
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <linux/capability.h>

#include "crossproc_helper.h"
#include "crossproc_protection.h"

/* Install path. Fixed; never derived from input. The Polkit action pins this
 * exact path with org.freedesktop.policykit.exec.path. */
#define MR_XPROC_HELPER_NAME "memreduct-zamak-crossproc-helper"

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

#ifndef MADV_PAGEOUT
#  define MADV_PAGEOUT 21
#endif

#ifndef CAP_SYS_NICE
#  define CAP_SYS_NICE 23
#endif

#define MR_XPROC_PATH_BYTES 4096u
#define MR_XPROC_LINE_BYTES 4096u
#define MR_XPROC_COMM_BYTES 64u
/* One syscall, one target, one vector array. These are ceilings, not defaults:
 * a caller may ask for less, never more. */
#define MR_XPROC_MAX_IOVECS 32u
#define MR_XPROC_MAX_BYTES (64ull * 1024ull * 1024ull)
/* Room for the walk up the parent chain before giving up on a malformed tree. */
#define MR_XPROC_MAX_ANCESTRY 256

/* ---------------------------------------------------------------------------
 * Test-only injection points.
 *
 * A MEMREDUCT_TESTING build may run as a normal user against a fixture procfs
 * tree and may replace the syscall so no real kernel state is touched. The
 * production build script must not define MEMREDUCT_TESTING for this file; a
 * regression test asserts that.
 * ------------------------------------------------------------------------- */
#ifdef MEMREDUCT_TESTING
static const char *test_proc_root;

static const char *proc_root(void)
{
    if (test_proc_root == NULL) {
        const char *value = getenv("MEMREDUCT_TEST_PROC_ROOT");
        test_proc_root = (value != NULL && value[0] == '/') ? value : "/proc";
    }
    return test_proc_root;
}

/* Every test hook below is opt-in and explicit. None of them can be reached in
 * a production build because the whole block is compiled out. */
static int test_skip_privilege(void)
{
    const char *value = getenv("MEMREDUCT_TEST_SKIP_PRIVILEGE");
    return value != NULL && strcmp(value, "1") == 0;
}

static int test_skip_requester(void)
{
    const char *value = getenv("MEMREDUCT_TEST_SKIP_REQUESTER");
    return value != NULL && strcmp(value, "1") == 0;
}

static int test_skip_capability(void)
{
    const char *value = getenv("MEMREDUCT_TEST_SKIP_CAPABILITY");
    return value != NULL && strcmp(value, "1") == 0;
}

static int test_backend_stub(void)
{
    const char *value = getenv("MEMREDUCT_TEST_BACKEND");
    return value != NULL && strcmp(value, "stub") == 0;
}

/* The ancestry walk starts at this process's own real PID, which a fixture tree
 * cannot contain (it is only known at run time). Skipping it lets every other
 * check be driven by a fixture; the walk itself has its own case below. */
static int test_skip_ancestry(void)
{
    const char *value = getenv("MEMREDUCT_TEST_SKIP_ANCESTRY");
    return value != NULL && strcmp(value, "1") == 0;
}

/* pidfd_open() cannot succeed for a PID that exists only in a fixture tree, so
 * the test build may substitute a descriptor opened on a private file. The
 * production path calls the syscall directly, with no branch around it. */
static int test_pidfd_open(int32_t pid)
{
    const char *path = getenv("MEMREDUCT_TEST_PIDFD_FILE");
    if (path == NULL || path[0] != '/') {
        return -1;
    }
    (void)pid;
    return open(path, O_RDONLY | O_CLOEXEC);
}
#else
static const char *proc_root(void) { return "/proc"; }
static int test_skip_privilege(void) { return 0; }
static int test_skip_requester(void) { return 0; }
static int test_skip_capability(void) { return 0; }
static int test_backend_stub(void) { return 0; }
static int test_skip_ancestry(void) { return 0; }
#endif

/* ---------------------------------------------------------------------------
 * Small helpers. Fixed buffers, no allocation on the decision path, no
 * format string that comes from input.
 * ------------------------------------------------------------------------- */

static void emit(FILE *stream, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stream, format, args);
    va_end(args);
}

/* Strict unsigned decimal parse: digits only, no sign, no whitespace, no
 * trailing garbage, and no overflow past `max`. */
static int parse_u64(const char *text, uint64_t max, uint64_t *out)
{
    if (text == NULL || text[0] == '\0') {
        return -1;
    }
    uint64_t value = 0;
    for (const char *cursor = text; *cursor != '\0'; cursor++) {
        if (!isdigit((unsigned char)*cursor)) {
            return -1;
        }
        unsigned int digit = (unsigned int)(*cursor - '0');
        if (value > (max - digit) / 10u) {
            return -1;
        }
        value = value * 10u + digit;
    }
    *out = value;
    return 0;
}

static int proc_path(char *buffer, size_t size, int32_t pid, const char *leaf)
{
    int written = snprintf(buffer, size, "%s/%d/%s", proc_root(), (int)pid, leaf);
    if (written < 0 || (size_t)written >= size) {
        return -1;
    }
    return 0;
}

/* Read one small procfs file into a fixed buffer. Returns the byte count, or
 * -1. Deliberately refuses a file that fills the buffer: a truncated status or
 * stat line is not evidence. */
static ssize_t read_proc_file(const char *path, char *buffer, size_t size)
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
            int saved = errno;
            (void)close(fd);
            errno = saved;
            return -1;
        }
        if (got == 0) {
            break;
        }
        total += got;
    }
    if ((size_t)total + 1u >= size) {
        (void)close(fd);
        errno = EOVERFLOW;
        return -1;
    }
    buffer[total] = '\0';
    (void)close(fd);
    return total;
}

/* Pull a "Key:\tvalue" line out of /proc/<pid>/status. */
static int status_uid_values(const char *status, uint64_t out[4])
{
    int found = 0;
    const char *line = status;
    while (line != NULL && *line != '\0') {
        if (strncmp(line, "Uid:", 4) == 0) {
            if ((line[4] != ' ' && line[4] != '\t') || found) {
                return -1;
            }
            found = 1;
            const char *value = line + 4;
            while (*value == ' ' || *value == '\t') {
                value++;
            }
            for (size_t index = 0; index < 4u; index++) {
                if (*value < '0' || *value > '9') {
                    return -1;
                }
                uint64_t parsed = 0;
                do {
                    unsigned int digit = (unsigned int)(*value - '0');
                    if (parsed > (UINT32_MAX - digit) / 10u) {
                        return -1;
                    }
                    parsed = parsed * 10u + digit;
                    value++;
                } while (*value >= '0' && *value <= '9');
                out[index] = parsed;
                if (index < 3u) {
                    if (*value != ' ' && *value != '\t') {
                        return -1;
                    }
                    while (*value == ' ' || *value == '\t') {
                        value++;
                    }
                }
            }
            while (*value == ' ' || *value == '\t') {
                value++;
            }
            if (*value != '\n' && *value != '\0') {
                return -1;
            }
        }
        const char *next = strchr(line, '\n');
        line = next == NULL ? NULL : next + 1;
    }
    return found ? 0 : -1;
}

/* Copy the process name from /proc/<pid>/status (the "Name:" line). */
static int status_name(const char *status, char *out, size_t size)
{
    const char *line = status;
    while (line != NULL && *line != '\0') {
        if (strncmp(line, "Name:", 5) == 0) {
            const char *value = line + 5;
            while (*value == ' ' || *value == '\t') {
                value++;
            }
            size_t length = 0;
            while (value[length] != '\0' && value[length] != '\n' && length + 1u < size) {
                out[length] = value[length];
                length++;
            }
            out[length] = '\0';
            return length > 0 ? 0 : -1;
        }
        const char *next = strchr(line, '\n');
        line = next == NULL ? NULL : next + 1;
    }
    return -1;
}

/* ---------------------------------------------------------------------------
 * /proc/<pid>/stat parsing.
 *
 * The comm field can contain spaces and parentheses, so the only safe anchor is
 * the LAST ')' on the line. Everything after it is a fixed, space-separated
 * field list starting at field 3 (state). start_time is field 22, which is
 * index 19 after that anchor.
 * ------------------------------------------------------------------------- */
#define MR_STAT_STARTTIME_INDEX 19

struct mr_proc_stat {
    char state;
    int32_t ppid;
    uint64_t start_time;
};

static int parse_stat(const char *stat_line, struct mr_proc_stat *out)
{
    const char *open_paren = strchr(stat_line, '(');
    const char *close_paren = strrchr(stat_line, ')');
    if (open_paren == NULL || close_paren == NULL || close_paren < open_paren) {
        return -1;
    }
    const char *cursor = close_paren + 1;
    if (*cursor != ' ') {
        return -1;
    }
    cursor++;
    if (*cursor == '\0' || *cursor == ' ') {
        return -1;
    }
    out->state = *cursor;

    /* field 3 is the state character; walk to the start_time index. */
    int index = 0;
    while (index < MR_STAT_STARTTIME_INDEX) {
        while (*cursor != '\0' && *cursor != ' ') {
            cursor++;
        }
        if (*cursor != ' ') {
            return -1;
        }
        cursor++;
        index++;
    }

    /* start_time, field 22. */
    uint64_t start_time = 0;
    int digits = 0;
    while (*cursor >= '0' && *cursor <= '9') {
        uint64_t next = start_time * 10u + (uint64_t)(*cursor - '0');
        if (next < start_time) {
            return -1;
        }
        start_time = next;
        cursor++;
        digits++;
    }
    if (digits == 0) {
        return -1;
    }
    /* A field boundary must follow, otherwise this is not a number we parsed. */
    if (*cursor != ' ' && *cursor != '\0' && *cursor != '\n') {
        return -1;
    }
    out->start_time = start_time;

    /* ppid, field 4: re-walk from the anchor. */
    const char *walk = close_paren + 2; /* skip "state" */
    walk = strchr(walk, ' ');           /* end of state */
    if (walk == NULL) {
        return -1;
    }
    walk++;
    int64_t ppid = 0;
    int ppid_digits = 0;
    while (*walk >= '0' && *walk <= '9') {
        ppid = ppid * 10 + (*walk - '0');
        walk++;
        ppid_digits++;
    }
    if (ppid_digits == 0) {
        return -1;
    }
    out->ppid = (int32_t)ppid;
    return 0;
}

/* ---------------------------------------------------------------------------
 * Identity checks.
 * ------------------------------------------------------------------------- */

static int read_status(int32_t pid, char *buffer, size_t size)
{
    char path[MR_XPROC_PATH_BYTES];
    if (proc_path(path, sizeof(path), pid, "status") != 0) {
        return -1;
    }
    if (read_proc_file(path, buffer, size) < 0) {
        return -1;
    }
    return 0;
}

static int read_stat(int32_t pid, struct mr_proc_stat *out)
{
    char path[MR_XPROC_PATH_BYTES];
    char line[MR_XPROC_LINE_BYTES];
    if (proc_path(path, sizeof(path), pid, "stat") != 0) {
        return -1;
    }
    if (read_proc_file(path, line, sizeof(line)) < 0) {
        return -1;
    }
    return parse_stat(line, out);
}

/* Walk from pid up through PPid to 1. Returns 1 when `ancestor` is found. A
 * malformed or unreadable chain is reported as -1, which callers treat as a
 * refusal: an unprovable ancestry is not a safe ancestry. */
static int is_ancestor_of(int32_t ancestor, int32_t pid)
{
    int32_t cursor = pid;
    for (int step = 0; step < MR_XPROC_MAX_ANCESTRY; step++) {
        if (cursor == ancestor) {
            return 1;
        }
        if (cursor <= 1) {
            return 0;
        }
        struct mr_proc_stat info;
        if (read_stat(cursor, &info) != 0) {
            return -1;
        }
        if (info.ppid <= 0 || info.ppid == cursor) {
            return 0;
        }
        cursor = info.ppid;
    }
    return -1;
}

static int read_target_exe(int32_t pid, char *target, size_t size)
{
    char path[MR_XPROC_PATH_BYTES];
    if (target == NULL || size < 2u || proc_path(path, sizeof(path), pid, "exe") != 0) {
        return -1;
    }
    ssize_t got = readlink(path, target, size - 1u);
    if (got <= 0 || (size_t)got >= size - 1u) {
        target[0] = '\0';
        return -1;
    }
    target[got] = '\0';
    return target[0] == '/' ? 0 : -1;
}

static int read_target_cgroup(int32_t pid, char *buffer, size_t size)
{
    char path[MR_XPROC_PATH_BYTES];
    if (proc_path(path, sizeof(path), pid, "cgroup") != 0 ||
        read_proc_file(path, buffer, size) < 0) {
        return -1;
    }
    return 0;
}

/* Fail closed unless the process is a same-UID user application whose kernel
 * identity can be re-derived from status, exe, and cgroup. `enforce_requester`
 * is false only in fixture tests; even there all four target UIDs must agree. */
static const char *inspect_target(int32_t pid, uint64_t requester_uid,
                                  int enforce_requester, uint64_t *target_uid_out)
{
    char status[MR_XPROC_LINE_BYTES];
    if (read_status(pid, status, sizeof(status)) != 0) {
        return "target_not_readable";
    }

    uint64_t uids[4] = {0, 0, 0, 0};
    if (status_uid_values(status, uids) != 0) {
        return "target_uid_unreadable";
    }
    if (uids[0] != uids[1] || uids[0] != uids[2] || uids[0] != uids[3]) {
        return "target_uid_credentials_mismatch";
    }
    if (target_uid_out != NULL) {
        *target_uid_out = uids[0];
    }
    if (uids[0] == 0) {
        return "target_is_root_owned";
    }
    if (enforce_requester && uids[0] != requester_uid) {
        return "target_other_uid";
    }

    char name[MR_XPROC_COMM_BYTES];
    if (status_name(status, name, sizeof(name)) != 0) {
        return "target_name_unreadable";
    }
    if (mr_xproc_name_is_protected(name)) {
        return "target_is_protected_process";
    }

    char exe[MR_XPROC_PATH_BYTES];
    if (read_target_exe(pid, exe, sizeof(exe)) != 0) {
        return "target_exe_unverifiable";
    }
    int exe_protected = mr_xproc_exe_is_protected(exe);
    if (exe_protected < 0) {
        return "target_exe_unverifiable";
    }
    if (exe_protected != 0) {
        return "target_is_protected_process";
    }

    char cgroup[MR_XPROC_LINE_BYTES];
    if (read_target_cgroup(pid, cgroup, sizeof(cgroup)) != 0) {
        return "target_cgroup_unverifiable";
    }
    uint64_t cgroup_uid = enforce_requester ? requester_uid : uids[0];
    int user_app = mr_xproc_cgroup_text_is_user_app(cgroup, (uint32_t)cgroup_uid);
    if (user_app < 0) {
        return "target_cgroup_unverifiable";
    }
    if (user_app == 0) {
        return "target_in_protected_cgroup";
    }
    return NULL;
}

/* Effective capability set, read from the kernel. Not from argv, not from the
 * environment. */
static int has_effective_cap(int capability)
{
    struct __user_cap_header_struct header;
    struct __user_cap_data_struct data[2];
    memset(&header, 0, sizeof(header));
    memset(data, 0, sizeof(data));
    header.version = _LINUX_CAPABILITY_VERSION_3;
    header.pid = 0;
    if (syscall(__NR_capget, &header, data) != 0) {
        return -1;
    }
    if (capability < 0 || capability > 63) {
        return -1;
    }
    unsigned int word = (unsigned int)capability / 32u;
    unsigned int bit = (unsigned int)capability % 32u;
    return (data[word].effective >> bit) & 1u;
}

/* pkexec executes a root target after setreuid(0, 0), so the correct helper
 * credentials are ruid=0/euid=0. PKEXEC_UID is only a caller declaration; the
 * target's kernel-reported UIDs are independently checked below. */
static const char *validate_helper_credentials(uint64_t real_uid,
                                               uint64_t effective_uid,
                                               int cap_sys_nice,
                                               const char *pkexec_uid,
                                               uint64_t *requester_uid)
{
    if (effective_uid != 0) {
        return "not_root";
    }
    if (real_uid != 0) {
        return "invalid_pkexec_root_credentials";
    }
    if (cap_sys_nice < 0) {
        return "capget_failed";
    }
    if (cap_sys_nice != 1) {
        return "missing_cap_sys_nice";
    }
    if (pkexec_uid == NULL || pkexec_uid[0] == '\0') {
        return "not_launched_via_pkexec";
    }
    uint64_t parsed = 0;
    if (parse_u64(pkexec_uid, UINT32_MAX, &parsed) != 0) {
        return "malformed_requester_uid";
    }
    if (parsed == 0) {
        return "invalid_requester_uid";
    }
    if (requester_uid != NULL) {
        *requester_uid = parsed;
    }
    return NULL;
}

#ifdef MEMREDUCT_TESTING
/* Pure credential-model probe. It is compiled out of the installed helper. */
const char *mr_xproc_test_validate_helper_credentials(uint64_t real_uid,
                                                       uint64_t effective_uid,
                                                       int cap_sys_nice,
                                                       const char *pkexec_uid,
                                                       uint64_t *requester_uid)
{
    return validate_helper_credentials(real_uid, effective_uid, cap_sys_nice,
                                       pkexec_uid, requester_uid);
}
#endif

/* ---------------------------------------------------------------------------
 * Region selection.
 *
 * Only ranges the kernel still reports for this exact pidfd target are used,
 * and only ranges whose reclaim is a reasonable request:
 *   - private mappings only ('p'), because a shared mapping belongs to more
 *     than this process and is not this action's business;
 *   - readable;
 *   - not a writable file-backed mapping (its pages may be dirty: reclaiming
 *     them forces writeback and can stall the target's own writes);
 *   - not one of the special kernel-provided mappings.
 * ------------------------------------------------------------------------- */

enum mr_region_skip {
    MR_REGION_KEPT = 0,
    MR_REGION_SHARED,
    MR_REGION_UNREADABLE,
    MR_REGION_WRITABLE_FILE,
    MR_REGION_SPECIAL,
    MR_REGION_TOO_LARGE,
    MR_REGION_BUDGET,
    MR_REGION_MALFORMED
};

static int is_special_mapping(const char *path)
{
    if (path == NULL || path[0] != '[') {
        return 0;
    }
    return strcmp(path, "[vdso]") == 0 || strcmp(path, "[vvar]") == 0 ||
           strcmp(path, "[vsyscall]") == 0 || strcmp(path, "[stack]") == 0 ||
           strcmp(path, "[vvar_vclock]") == 0 || strcmp(path, "[uprobes]") == 0;
}

static uint64_t page_size(void)
{
    long value = sysconf(_SC_PAGESIZE);
    return value > 0 ? (uint64_t)value : 4096ull;
}

struct mr_region_plan {
    struct iovec vectors[MR_XPROC_MAX_IOVECS];
    size_t count;
    uint64_t bytes;
    uint32_t skipped_shared;
    uint32_t skipped_unreadable;
    uint32_t skipped_writable_file;
    uint32_t skipped_special;
    uint32_t skipped_too_large;
    uint32_t skipped_budget;
    uint32_t skipped_malformed;
};

/* Read the target's maps and choose at most `max_iovecs` ranges totalling no
 * more than `max_bytes`. Never returns more than the ceilings allow. */
static int plan_regions(int32_t pid, uint64_t max_bytes, struct mr_region_plan *plan)
{
    memset(plan, 0, sizeof(*plan));

    char path[MR_XPROC_PATH_BYTES];
    if (proc_path(path, sizeof(path), pid, "maps") != 0) {
        return -1;
    }
    FILE *maps = fopen(path, "re");
    if (maps == NULL) {
        return -1;
    }

    const uint64_t alignment = page_size();
    char line[MR_XPROC_LINE_BYTES];

    while (plan->count < MR_XPROC_MAX_IOVECS && fgets(line, sizeof(line), maps) != NULL) {
        unsigned long long begin = 0;
        unsigned long long end = 0;
        char perms[8] = {0};
        char device[32] = {0};
        char name[512] = {0};
        unsigned long long offset = 0;
        unsigned long long inode = 0;

        int fields = sscanf(line, "%llx-%llx %7s %llx %31s %llu %511s",
                            &begin, &end, perms, &offset, device, &inode, name);
        if (fields < 5) {
            plan->skipped_malformed++;
            continue;
        }
        if (end <= begin) {
            plan->skipped_malformed++;
            continue;
        }

        uint64_t length = (uint64_t)(end - begin);

        /* Private only. */
        if (perms[3] != 'p') {
            plan->skipped_shared++;
            continue;
        }
        /* Readable: an unreadable range has no reclaimable user pages. */
        if (perms[0] != 'r') {
            plan->skipped_unreadable++;
            continue;
        }
        /* Writable file-backed: possibly dirty, reclaim forces writeback. */
        if (fields >= 6 && inode != 0 && perms[1] == 'w') {
            plan->skipped_writable_file++;
            continue;
        }
        if (is_special_mapping(name)) {
            plan->skipped_special++;
            continue;
        }
        if (length > max_bytes) {
            plan->skipped_too_large++;
            continue;
        }
        if (plan->bytes + length > max_bytes) {
            plan->skipped_budget++;
            continue;
        }

        /* Kernel advice takes page-aligned ranges; round inward so the range is
         * entirely inside the mapping. */
        uint64_t aligned_start = ((uint64_t)begin + alignment - 1u) & ~(alignment - 1u);
        uint64_t aligned_end = (uint64_t)end & ~(alignment - 1u);
        if (aligned_end <= aligned_start) {
            plan->skipped_malformed++;
            continue;
        }

        plan->vectors[plan->count].iov_base = (void *)(uintptr_t)aligned_start;
        plan->vectors[plan->count].iov_len = (size_t)(aligned_end - aligned_start);
        plan->count++;
        plan->bytes += aligned_end - aligned_start;
    }
    int read_error = ferror(maps);
    (void)fclose(maps);
    if (read_error) {
        return -1;
    }
    if (plan->count == 0) {
        return 0;
    }
    return 1;
}

/* ---------------------------------------------------------------------------
 * The syscall. Exactly one call, never retried, never split into batches.
 * ------------------------------------------------------------------------- */

static long call_process_madvise(int pidfd, struct iovec *vectors, size_t count)
{
    if (test_backend_stub()) {
        /* Test build only: report full coverage without touching any process. */
        uint64_t total = 0;
        for (size_t index = 0; index < count; index++) {
            total += vectors[index].iov_len;
        }
        return (long)total;
    }
#if __NR_process_madvise < 0
    errno = ENOSYS;
    return -1;
#else
    return syscall(__NR_process_madvise, pidfd, vectors, count, MADV_PAGEOUT, 0u);
#endif
}

static const char *classify_status(int64_t status)
{
    switch (status) {
        case MR_XPROC_STATUS_ACCEPTED: return "accepted";
        case MR_XPROC_STATUS_PARTIAL: return "partial";
        case MR_XPROC_STATUS_REFUSED: return "refused";
        case MR_XPROC_STATUS_NO_PERMISSION: return "no_permission";
        case MR_XPROC_STATUS_TARGET_GONE: return "target_gone";
        case MR_XPROC_STATUS_BAD_REQUEST: return "bad_request";
        case MR_XPROC_STATUS_UNSUPPORTED: return "unsupported";
        case MR_XPROC_STATUS_UNKNOWN: return "unknown";
        case MR_XPROC_STATUS_NO_REGIONS: return "no_regions";
        default: return "unknown";
    }
}

/* ---------------------------------------------------------------------------
 * Result block. Machine-readable, one key per line. The GUI parses this; it is
 * the only channel through which the privileged side speaks.
 * ------------------------------------------------------------------------- */

struct mr_outcome {
    int64_t status;
    const char *reason;
    int32_t pid;
    /* The caller's claim, echoed back so the GUI can prove the helper answered
     * about the target it asked for. This is what the caller said, not what the
     * helper verified; `start_time_verified` is the separate answer. */
    uint64_t start_time;
    int start_time_verified;
    /* The target's real UID as read from the kernel, -1 while unknown. A
     * refusal that happens before the target is inspected must not print a
     * number that looks like an owner. */
    int64_t uid;
    uint32_t regions;
    uint64_t requested_bytes;
    uint64_t advised_bytes;
    int32_t error_code;
    int32_t call_count;
    uint32_t skipped_shared;
    uint32_t skipped_unreadable;
    uint32_t skipped_writable_file;
    uint32_t skipped_special;
    uint32_t skipped_too_large;
    uint32_t skipped_budget;
    uint32_t skipped_malformed;
};

static void print_outcome(const struct mr_outcome *outcome)
{
    emit(stdout, "MR_CROSSPROC_RESULT\n");
    emit(stdout, "status=%s\n", classify_status(outcome->status));
    emit(stdout, "reason=%s\n", outcome->reason == NULL ? "none" : outcome->reason);
    emit(stdout, "pid=%d\n", (int)outcome->pid);
    emit(stdout, "start_time=%llu\n", (unsigned long long)outcome->start_time);
    emit(stdout, "start_time_verified=%s\n", outcome->start_time_verified ? "true" : "false");
    if (outcome->uid < 0) {
        emit(stdout, "uid=unknown\n");
    } else {
        emit(stdout, "uid=%lld\n", (long long)outcome->uid);
    }
    emit(stdout, "regions=%u\n", outcome->regions);
    emit(stdout, "requested_bytes=%llu\n", (unsigned long long)outcome->requested_bytes);
    emit(stdout, "advised_bytes=%llu\n", (unsigned long long)outcome->advised_bytes);
    emit(stdout, "error_code=%d\n", (int)outcome->error_code);
    emit(stdout, "call_count=%d\n", (int)outcome->call_count);
    emit(stdout, "skipped_shared=%u\n", outcome->skipped_shared);
    emit(stdout, "skipped_unreadable=%u\n", outcome->skipped_unreadable);
    emit(stdout, "skipped_writable_file=%u\n", outcome->skipped_writable_file);
    emit(stdout, "skipped_special=%u\n", outcome->skipped_special);
    emit(stdout, "skipped_too_large=%u\n", outcome->skipped_too_large);
    emit(stdout, "skipped_budget=%u\n", outcome->skipped_budget);
    emit(stdout, "skipped_malformed=%u\n", outcome->skipped_malformed);
    /* This line exists because the single most likely misreading of this tool
     * is that advised_bytes is memory that came back. It is not. */
    emit(stdout, "bytes_advised_is_not_bytes_freed=true\n");
}

static int status_exit_code(int64_t status)
{
    switch (status) {
        case MR_XPROC_STATUS_ACCEPTED:
        case MR_XPROC_STATUS_PARTIAL:
            return 0;
        case MR_XPROC_STATUS_UNSUPPORTED:
            return 3;
        case MR_XPROC_STATUS_UNKNOWN:
            return 4;
        case MR_XPROC_STATUS_REFUSED:
        case MR_XPROC_STATUS_NO_REGIONS:
        case MR_XPROC_STATUS_NO_PERMISSION:
        case MR_XPROC_STATUS_TARGET_GONE:
        case MR_XPROC_STATUS_BAD_REQUEST:
        default:
            return 1;
    }
}

static int refuse(struct mr_outcome *outcome, const char *reason)
{
    outcome->status = MR_XPROC_STATUS_REFUSED;
    outcome->reason = reason;
    return 1;
}

/* ---------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */

static int usage(void)
{
    emit(stderr,
         "usage: %s reclaim <pid> <start_time> <max_bytes>\n"
         "All three values are decimal and are treated as untrusted candidates;\n"
         "this helper re-verifies identity, ownership and mappings from the kernel.\n",
         MR_XPROC_HELPER_NAME);
    return 2;
}

int main(int argc, char **argv)
{
    struct mr_outcome outcome;
    memset(&outcome, 0, sizeof(outcome));

    /* Exactly four arguments, one fixed verb. No option parsing, no "--", no
     * free-form text, so nothing here can be turned into an argument-injection
     * or path-injection surface. */
    if (argc != 5) {
        return usage();
    }
    if (strcmp(argv[1], "reclaim") != 0) {
        return usage();
    }

    uint64_t raw_pid = 0;
    uint64_t raw_start_time = 0;
    uint64_t raw_max_bytes = 0;
    if (parse_u64(argv[2], 0x7fffffffull, &raw_pid) != 0 ||
        parse_u64(argv[3], UINT64_MAX, &raw_start_time) != 0 ||
        parse_u64(argv[4], MR_XPROC_MAX_BYTES, &raw_max_bytes) != 0) {
        return usage();
    }
    if (raw_pid < 2 || raw_max_bytes < page_size()) {
        /* PID 1 is never a target, and a budget below one page cannot describe
         * a page-aligned range. */
        return usage();
    }
    outcome.pid = (int32_t)raw_pid;
    outcome.start_time = raw_start_time;
    /* Unknown until the kernel tells us it matches. Nothing is verified yet. */
    outcome.start_time_verified = 0;
    outcome.uid = -1;

    /* ---- 1. Validate the root target credentials used by pkexec. ---- */
    uint64_t requester_uid = 0;
    uint64_t real_uid = test_skip_privilege() ? 0u : (uint64_t)getuid();
    uint64_t effective_uid = test_skip_privilege() ? 0u : (uint64_t)geteuid();
    int capability = test_skip_capability() ? 1 : -2;
    if (!test_skip_capability() && real_uid == 0 && effective_uid == 0) {
        capability = has_effective_cap(CAP_SYS_NICE);
    }
    const char *pkexec_uid = test_skip_requester() ? "1" : getenv("PKEXEC_UID");
    const char *credential_error = validate_helper_credentials(
        real_uid, effective_uid, capability, pkexec_uid, &requester_uid);
    if (credential_error != NULL) {
        refuse(&outcome, credential_error);
        print_outcome(&outcome);
        return status_exit_code(outcome.status);
    }

    /* ---- 2. Target identity, re-derived from the kernel. ---- */
    if (outcome.pid == 1) {
        refuse(&outcome, "target_is_pid1");
        print_outcome(&outcome);
        return status_exit_code(outcome.status);
    }
    if (outcome.pid == (int32_t)getpid()) {
        refuse(&outcome, "target_is_self");
        print_outcome(&outcome);
        return status_exit_code(outcome.status);
    }

    struct mr_proc_stat stat_before;
    if (read_stat(outcome.pid, &stat_before) != 0) {
        refuse(&outcome, "target_not_readable");
        print_outcome(&outcome);
        return status_exit_code(outcome.status);
    }
    if (stat_before.start_time != outcome.start_time) {
        /* The number the caller saw now belongs to a different process. */
        refuse(&outcome, "start_time_mismatch");
        print_outcome(&outcome);
        return status_exit_code(outcome.status);
    }
    if (stat_before.state == 'Z' || stat_before.state == 'X' || stat_before.state == 'x') {
        refuse(&outcome, "target_not_running");
        print_outcome(&outcome);
        return status_exit_code(outcome.status);
    }
    uint64_t target_uid = 0;
    const char *target_error = inspect_target(outcome.pid, requester_uid,
                                              !test_skip_requester(), &target_uid);
    if (target_error != NULL) {
        refuse(&outcome, target_error);
        print_outcome(&outcome);
        return status_exit_code(outcome.status);
    }
    outcome.uid = (int64_t)target_uid;
    /* From here on the start_time in this report is the value the kernel
     * confirmed, not merely the value the caller supplied. */
    outcome.start_time_verified = 1;

    /* Ancestry: the helper's own chain runs helper -> pkexec -> GUI. Advising
     * any process on it would stall the very operation that asked for it. */
    if (!test_skip_ancestry()) {
        int ancestry = is_ancestor_of(outcome.pid, (int32_t)getpid());
        if (ancestry != 0) {
            /* 1 means the target is an ancestor; -1 means the chain could not be
             * proven. Both are refusals: an unprovable chain is not a safe one. */
            refuse(&outcome, ancestry == 1 ? "target_is_ancestor" : "ancestry_unverifiable");
            print_outcome(&outcome);
            return status_exit_code(outcome.status);
        }
    }

    /* Kernel threads have no address space. Asking for their maps is how we
     * find out, and the answer is a refusal rather than an empty plan. */
    char maps_path[MR_XPROC_PATH_BYTES];
    if (proc_path(maps_path, sizeof(maps_path), outcome.pid, "maps") != 0) {
        refuse(&outcome, "target_path_invalid");
        print_outcome(&outcome);
        return status_exit_code(outcome.status);
    }
    struct stat maps_stat;
    if (stat(maps_path, &maps_stat) != 0) {
        refuse(&outcome, "target_has_no_address_space");
        print_outcome(&outcome);
        return status_exit_code(outcome.status);
    }

    /* ---- 3. Pin the identity. A pidfd cannot be re-pointed at a reused PID. ---- */
#ifdef MEMREDUCT_TESTING
    int pidfd = test_pidfd_open(outcome.pid);
    if (pidfd < 0) {
        pidfd = (int)syscall(__NR_pidfd_open, (pid_t)outcome.pid, 0u);
    }
#else
    int pidfd = (int)syscall(__NR_pidfd_open, (pid_t)outcome.pid, 0u);
#endif
    if (pidfd < 0) {
        refuse(&outcome, errno == ENOSYS ? "pidfd_open_unsupported" : "pidfd_open_failed");
        outcome.error_code = errno;
        print_outcome(&outcome);
        return status_exit_code(outcome.status);
    }

    /* ---- 6. Re-read the identity through the pinned target. ---- */
    struct mr_proc_stat stat_pinned;
    if (read_stat(outcome.pid, &stat_pinned) != 0 ||
        stat_pinned.start_time != outcome.start_time ||
        stat_pinned.state == 'Z' || stat_pinned.state == 'X' || stat_pinned.state == 'x') {
        (void)close(pidfd);
        refuse(&outcome, "target_identity_changed");
        print_outcome(&outcome);
        return status_exit_code(outcome.status);
    }

    uint64_t pinned_uid = 0;
    target_error = inspect_target(outcome.pid, requester_uid,
                                  !test_skip_requester(), &pinned_uid);
    if (target_error != NULL || pinned_uid != (uint64_t)outcome.uid) {
        (void)close(pidfd);
        refuse(&outcome, target_error == NULL ? "target_identity_changed" : target_error);
        print_outcome(&outcome);
        return status_exit_code(outcome.status);
    }

    /* ---- 4. Choose ranges here, from the kernel's own maps. ---- */
    struct mr_region_plan plan;
    int planned = plan_regions(outcome.pid, raw_max_bytes, &plan);
    outcome.skipped_shared = plan.skipped_shared;
    outcome.skipped_unreadable = plan.skipped_unreadable;
    outcome.skipped_writable_file = plan.skipped_writable_file;
    outcome.skipped_special = plan.skipped_special;
    outcome.skipped_too_large = plan.skipped_too_large;
    outcome.skipped_budget = plan.skipped_budget;
    outcome.skipped_malformed = plan.skipped_malformed;

    if (planned < 0) {
        (void)close(pidfd);
        refuse(&outcome, "target_maps_unreadable");
        print_outcome(&outcome);
        return status_exit_code(outcome.status);
    }
    if (planned == 0) {
        (void)close(pidfd);
        outcome.status = MR_XPROC_STATUS_NO_REGIONS;
        outcome.reason = "no_reclaimable_private_region";
        print_outcome(&outcome);
        return status_exit_code(outcome.status);
    }

    outcome.regions = (uint32_t)plan.count;
    outcome.requested_bytes = plan.bytes;

    /* ---- 5. One syscall. Not a loop, not a retry, not a batch sequence. ---- */
    errno = 0;
    long advised = call_process_madvise(pidfd, plan.vectors, plan.count);
    int call_errno = errno;
    outcome.call_count = 1;

    /* Whatever happened, the pinned target must still be the same process. */
    struct mr_proc_stat stat_after;
    int identity_stable = read_stat(outcome.pid, &stat_after) == 0 &&
                          stat_after.start_time == outcome.start_time;
    int close_result = close(pidfd);
    (void)close_result; /* nothing downstream depends on it */

    if (advised < 0) {
        outcome.error_code = call_errno;
        switch (call_errno) {
            case EPERM:
            case EACCES:
                outcome.status = MR_XPROC_STATUS_NO_PERMISSION;
                outcome.reason = "kernel_denied_permission";
                break;
            case ESRCH:
            case EBADF:
                outcome.status = MR_XPROC_STATUS_TARGET_GONE;
                outcome.reason = "kernel_target_gone";
                break;
            case EINVAL:
                outcome.status = MR_XPROC_STATUS_BAD_REQUEST;
                outcome.reason = "kernel_bad_request";
                break;
            case ENOSYS:
                outcome.status = MR_XPROC_STATUS_UNSUPPORTED;
                outcome.reason = "kernel_lacks_process_madvise";
                break;
            default:
                outcome.status = MR_XPROC_STATUS_UNKNOWN;
                outcome.reason = "kernel_other_error";
                break;
        }
        print_outcome(&outcome);
        return status_exit_code(outcome.status);
    }

    outcome.advised_bytes = (uint64_t)advised;

    if ((uint64_t)advised == 0) {
        /* The kernel accepted the call and covered nothing. That is a real
         * answer and it is not success: report it as partial with zero bytes. */
        outcome.status = MR_XPROC_STATUS_PARTIAL;
        outcome.reason = "kernel_covered_zero_bytes";
    } else if ((uint64_t)advised < plan.bytes) {
        outcome.status = MR_XPROC_STATUS_PARTIAL;
        outcome.reason = "kernel_covered_part_of_the_request";
    } else {
        outcome.status = MR_XPROC_STATUS_ACCEPTED;
        outcome.reason = "kernel_accepted_the_request";
    }

    if (!identity_stable) {
        /* The advice was issued, so this is not a clean success: the target may
         * have exited underneath it. Say so instead of reporting accepted. */
        outcome.reason = "target_identity_changed_after_call";
        if (outcome.status == MR_XPROC_STATUS_ACCEPTED) {
            outcome.status = MR_XPROC_STATUS_PARTIAL;
        }
    }

    print_outcome(&outcome);
    return status_exit_code(outcome.status);
}
