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
 * start_time, its liveness, its ancestry, and its actual mappings/scope. The
 * command line has one fixed verb, one fixed mode, and ten decimal claims.
 * There is no path argument, shell text, address range, signal, or retry.
 *
 * The kernel only accepts an advice request. The number it returns is a length
 * it covered, never an amount of memory that stayed out. Every output line that
 * could be misread as "freed" says what it actually is.
 */
#define _GNU_SOURCE

#include <ctype.h>
#include <dirent.h>
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
#include <sys/vfs.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <linux/capability.h>
#include <linux/magic.h>

#include "crossproc_helper.h"
#include "crossproc_identity.h"
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
#define MR_XPROC_CMDLINE_BYTES 8192u
/* Room for the walk up the parent chain before giving up on a malformed tree. */
#define MR_XPROC_MAX_ANCESTRY 256

#define MR_CGROUP_ROOT "/sys/fs/cgroup"
#define MR_CGROUP_PATH_BYTES 4096u
#define MR_CGROUP_FILE_BYTES 128u

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

static const char *cgroup_root(void)
{
    const char *value = getenv("MEMREDUCT_TEST_CGROUP_ROOT");
    return value != NULL && value[0] == '/' ? value : MR_CGROUP_ROOT;
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
static const char *cgroup_root(void) { return MR_CGROUP_ROOT; }
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

static int read_status(int32_t pid, char *buffer, size_t size);

static int status_memory_kib(const char *status, const char *key, int64_t *out)
{
    size_t key_length = strlen(key);
    const char *line = status;
    while (line != NULL && *line != '\0') {
        if (strncmp(line, key, key_length) == 0 && line[key_length] == ':') {
            const char *value = line + key_length + 1u;
            while (*value == ' ' || *value == '\t') value++;
            uint64_t amount = 0;
            const char *start = value;
            while (*value >= '0' && *value <= '9') {
                unsigned int digit = (unsigned int)(*value - '0');
                if (amount > ((uint64_t)INT64_MAX - digit) / 10u) return -1;
                amount = amount * 10u + digit;
                value++;
            }
            while (*value == ' ' || *value == '\t') value++;
            if (value == start || strncmp(value, "kB", 2u) != 0 ||
                (value[2] != '\n' && value[2] != '\0')) return -1;
            *out = (int64_t)amount;
            return 0;
        }
        const char *next = strchr(line, '\n');
        line = next == NULL ? NULL : next + 1;
    }
    return -1;
}

static int read_target_memory_kib(int32_t pid, const char *key, int64_t *out)
{
    char status[MR_XPROC_LINE_BYTES];
    if (out == NULL || read_status(pid, status, sizeof(status)) != 0) return -1;
    return status_memory_kib(status, key, out);
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

static int read_target_cmdline(int32_t pid, unsigned char *buffer, size_t size,
                               size_t *length_out)
{
    char path[MR_XPROC_PATH_BYTES];
    if (size == 0u || length_out == NULL ||
        proc_path(path, sizeof(path), pid, "cmdline") != 0) {
        return -1;
    }
    int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        return -1;
    }
    size_t used = 0;
    for (;;) {
        if (used == size) {
            char extra;
            ssize_t got;
            do {
                got = read(fd, &extra, 1u);
            } while (got < 0 && errno == EINTR);
            (void)close(fd);
            if (got != 0) {
                return -1;
            }
            break;
        }
        ssize_t got = read(fd, buffer + used, size - used);
        if (got < 0) {
            if (errno == EINTR) {
                continue;
            }
            (void)close(fd);
            return -1;
        }
        if (got == 0) {
            (void)close(fd);
            break;
        }
        used += (size_t)got;
    }
    if (used == 0u) {
        return -1;
    }
    *length_out = used;
    return 0;
}

/* Fail closed unless the process is a same-UID user application whose kernel
 * identity can be re-derived from status, exe, and cgroup. `enforce_requester`
 * is false only in fixture tests; even there all four target UIDs must agree. */
static const char *inspect_target(int32_t pid, uint64_t requester_uid,
                                  int enforce_requester, uint64_t *target_uid_out,
                                  uint64_t *identity_token_out,
                                  uint64_t *scope_token_out)
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
    struct mr_proc_stat stat;
    unsigned char cmdline[MR_XPROC_CMDLINE_BYTES];
    size_t cmdline_length = 0;
    if (read_stat(pid, &stat) != 0 ||
        read_target_cmdline(pid, cmdline, sizeof(cmdline), &cmdline_length) != 0) {
        return "target_identity_unverifiable";
    }
    if (identity_token_out != NULL) {
        *identity_token_out = mr_xproc_identity_token(
            pid, stat.start_time, uids[0], name, exe, cgroup,
            cmdline, cmdline_length);
    }
    if (scope_token_out != NULL) {
        *scope_token_out = mr_xproc_scope_token(cgroup);
    }
    return NULL;
}

struct mr_scope_snapshot {
    int64_t current_bytes;
    int64_t anon_bytes;
    int64_t file_bytes;
    int64_t swap_bytes;
};

static int read_fd_text(int fd, char *buffer, size_t size)
{
    if (fd < 0 || buffer == NULL || size < 2u) return -1;
    size_t used = 0;
    while (used + 1u < size) {
        ssize_t got = read(fd, buffer + used, size - used - 1u);
        if (got < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (got == 0) break;
        used += (size_t)got;
    }
    if (used + 1u >= size) {
        errno = EOVERFLOW;
        return -1;
    }
    buffer[used] = '\0';
    return (int)used;
}

static int read_cgroup_path(int32_t pid, uint32_t uid, char *path, size_t size)
{
    char text[MR_XPROC_LINE_BYTES];
    if (path == NULL || size < 2u || read_target_cgroup(pid, text, sizeof(text)) != 0) {
        return -1;
    }
    const char *line_end = strchr(text, '\n');
    size_t line_length = line_end == NULL ? strlen(text) : (size_t)(line_end - text);
    if (line_length < 4u || memcmp(text, "0::", 3u) != 0 ||
        (line_end != NULL && line_end[1] != '\0') ||
        mr_xproc_cgroup_path_is_user_app(text + 3u, line_length - 3u, uid) != 1) {
        return -1;
    }
    size_t path_length = line_length - 3u;
    if (path_length + 1u > size) return -1;
    memcpy(path, text + 3u, path_length);
    path[path_length] = '\0';
    return 0;
}

static int open_scope_directory(int32_t pid, uint32_t uid, int *scope_fd_out,
                                char *scope_path, size_t path_size)
{
    if (scope_fd_out == NULL || read_cgroup_path(pid, uid, scope_path, path_size) != 0) {
        errno = EINVAL;
        return -1;
    }
    int current = open(cgroup_root(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (current < 0) return -1;
#ifndef MEMREDUCT_TESTING
    struct statfs fs;
    if (fstatfs(current, &fs) != 0 || (unsigned long)fs.f_type != CGROUP2_SUPER_MAGIC) {
        (void)close(current);
        errno = ENODEV;
        return -1;
    }
#endif
    const char *cursor = scope_path + 1;
    while (*cursor != '\0') {
        const char *slash = strchr(cursor, '/');
        size_t component_length = slash == NULL ? strlen(cursor) : (size_t)(slash - cursor);
        if (component_length == 0u || component_length >= NAME_MAX ||
            (component_length == 1u && cursor[0] == '.') ||
            (component_length == 2u && cursor[0] == '.' && cursor[1] == '.')) {
            (void)close(current);
            errno = EINVAL;
            return -1;
        }
        char component[NAME_MAX];
        memcpy(component, cursor, component_length);
        component[component_length] = '\0';
        int next = openat(current, component,
                          O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        int saved = errno;
        (void)close(current);
        if (next < 0) {
            errno = saved;
            return -1;
        }
        current = next;
        cursor = slash == NULL ? cursor + component_length : slash + 1;
    }
    *scope_fd_out = current;
    return 0;
}

static int scope_has_no_child_cgroups(int scope_fd)
{
    int duplicate = dup(scope_fd);
    if (duplicate < 0) return -1;
    DIR *directory = fdopendir(duplicate);
    if (directory == NULL) {
        (void)close(duplicate);
        return -1;
    }
    int result = 0;
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        struct stat info;
        if (fstatat(scope_fd, entry->d_name, &info, AT_SYMLINK_NOFOLLOW) != 0 ||
            S_ISDIR(info.st_mode) || S_ISLNK(info.st_mode)) {
            result = -1;
            break;
        }
    }
    (void)closedir(directory);
    return result;
}

static int validate_scope_members(int scope_fd, int32_t target_pid,
                                  uint32_t requester_uid, uint64_t expected_scope_hash)
{
    int fd = openat(scope_fd, "cgroup.procs", O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return -1;
    char contents[MR_XPROC_MAX_SCOPE_PROCESSES * 16u + 1u];
    int length = read_fd_text(fd, contents, sizeof(contents));
    (void)close(fd);
    if (length < 0) return -1;

    uint32_t count = 0;
    int target_seen = 0;
    char *cursor = contents;
    while (*cursor != '\0') {
        char *line_end = strchr(cursor, '\n');
        if (line_end == NULL) line_end = cursor + strlen(cursor);
        if (line_end == cursor || count >= MR_XPROC_MAX_SCOPE_PROCESSES) return -1;
        char saved = *line_end;
        *line_end = '\0';
        uint64_t pid_value = 0;
        if (parse_u64(cursor, 0x7fffffffull, &pid_value) != 0 || pid_value < 2u) return -1;
        uint64_t member_uid = 0;
        uint64_t member_identity = 0;
        uint64_t member_scope = 0;
        const char *error = inspect_target((int32_t)pid_value, requester_uid, 1,
                                           &member_uid, &member_identity, &member_scope);
        if (error != NULL || member_uid != requester_uid ||
            member_scope != expected_scope_hash) return -1;
        if ((int32_t)pid_value == target_pid) target_seen = 1;
        count++;
        if (saved == '\0') break;
        cursor = line_end + 1;
    }
    return count > 0u && target_seen ? 0 : -1;
}

static int read_scope_number(int scope_fd, const char *name, int64_t *out)
{
    char text[MR_CGROUP_FILE_BYTES];
    int fd = openat(scope_fd, name, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return -1;
    int length = read_fd_text(fd, text, sizeof(text));
    (void)close(fd);
    if (length < 0) return -1;
    size_t value_length = strcspn(text, "\n");
    if (text[value_length] == '\n' && text[value_length + 1u] != '\0') return -1;
    text[value_length] = '\0';
    uint64_t value = 0;
    if (value_length == 0u || parse_u64(text, UINT64_MAX, &value) != 0 ||
        value > (uint64_t)INT64_MAX) return -1;
    *out = (int64_t)value;
    return 0;
}

static int read_scope_stat(int scope_fd, int64_t *anon_bytes, int64_t *file_bytes)
{
    char text[MR_XPROC_LINE_BYTES];
    int fd = openat(scope_fd, "memory.stat", O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return -1;
    int length = read_fd_text(fd, text, sizeof(text));
    (void)close(fd);
    if (length < 0) return -1;
    int have_anon = 0;
    int have_file = 0;
    for (char *line = text; *line != '\0';) {
        char *line_end = strchr(line, '\n');
        if (line_end == NULL) line_end = line + strlen(line);
        char saved = *line_end;
        *line_end = '\0';
        char *space = strchr(line, ' ');
        if (space != NULL) {
            *space++ = '\0';
            uint64_t value = 0;
            if (parse_u64(space, (uint64_t)INT64_MAX, &value) == 0) {
                if (strcmp(line, "anon") == 0) {
                    *anon_bytes = (int64_t)value;
                    have_anon = 1;
                } else if (strcmp(line, "file") == 0) {
                    *file_bytes = (int64_t)value;
                    have_file = 1;
                }
            }
        }
        if (saved == '\0') break;
        line = line_end + 1;
    }
    return have_anon && have_file ? 0 : -1;
}

static void read_scope_snapshot(int scope_fd, struct mr_scope_snapshot *snapshot)
{
    snapshot->current_bytes = -1;
    snapshot->anon_bytes = -1;
    snapshot->file_bytes = -1;
    snapshot->swap_bytes = -1;
    (void)read_scope_number(scope_fd, "memory.current", &snapshot->current_bytes);
    (void)read_scope_stat(scope_fd, &snapshot->anon_bytes, &snapshot->file_bytes);
    (void)read_scope_number(scope_fd, "memory.swap.current", &snapshot->swap_bytes);
}

static const char *scope_reclaim_once(int scope_fd, uint64_t requested_bytes,
                                      int *error_code)
{
    int fd = openat(scope_fd, "memory.reclaim", O_WRONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        *error_code = errno;
        return "scope_memory_reclaim_unavailable";
    }
    char value[32];
    int length = snprintf(value, sizeof(value), "%llu\n", (unsigned long long)requested_bytes);
#ifdef MEMREDUCT_TESTING
    const char *injected = getenv("MEMREDUCT_TEST_SCOPE_WRITE_RESULT");
    ssize_t written;
    if (injected != NULL && strcmp(injected, "eagain") == 0) {
        errno = EAGAIN;
        written = -1;
    } else if (injected != NULL && strcmp(injected, "short") == 0) {
        written = length > 1 ? length - 1 : 0;
    } else if (injected != NULL && strcmp(injected, "io") == 0) {
        errno = EIO;
        written = -1;
    } else {
        written = length > 0 ? write(fd, value, (size_t)length) : -1;
    }
#else
    ssize_t written = length > 0 ? write(fd, value, (size_t)length) : -1;
#endif
    int saved = errno;
    (void)close(fd);
    if (written == length) {
        *error_code = 0;
        return "scope_reclaim_request_accepted";
    }
    *error_code = written < 0 ? saved : EIO;
    if (written < 0 && saved == EAGAIN) return "scope_reclaim_shortfall";
    return "scope_reclaim_outcome_uncertain";
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
                                               const char *pkexec_uid,
                                               uint64_t *requester_uid)
{
    if (effective_uid != 0) {
        return "not_root";
    }
    if (real_uid != 0) {
        return "invalid_pkexec_root_credentials";
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
                                                       const char *pkexec_uid,
                                                       uint64_t *requester_uid)
{
    return validate_helper_credentials(real_uid, effective_uid, pkexec_uid,
                                       requester_uid);
}
#endif

/* PAGEOUT crosses a process boundary and the kernel requires CAP_SYS_NICE.
 * cgroup memory.reclaim is instead authorized by this root helper's Polkit
 * action and the exact control-file open; it does not use process_madvise. */
static const char *validate_pageout_capability(int cap_sys_nice)
{
    if (cap_sys_nice < 0) return "capget_failed";
    if (cap_sys_nice != 1) return "missing_cap_sys_nice";
    return NULL;
}

#ifdef MEMREDUCT_TESTING
const char *mr_xproc_test_validate_pageout_capability(int cap_sys_nice)
{
    return validate_pageout_capability(cap_sys_nice);
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
    /* The UI scanner excludes every kernel-labelled mapping, including newer
     * labels this helper may not know yet. Keep the privileged filter equally
     * narrow so its reviewed map summary is comparable. */
    return path != NULL && path[0] == '[';
}

static int target_is_self(int32_t target_pid, int32_t helper_pid)
{
    return target_pid == helper_pid;
}

#ifdef MEMREDUCT_TESTING
int mr_xproc_test_target_is_self(int32_t target_pid, int32_t helper_pid)
{
    return target_is_self(target_pid, helper_pid);
}
#endif

static uint64_t page_size(void)
{
    long value = sysconf(_SC_PAGESIZE);
    return value > 0 ? (uint64_t)value : 4096ull;
}

struct mr_region_plan {
    struct iovec vectors[MR_XPROC_MAX_IOVECS];
    size_t count;
    uint64_t bytes;
    uint64_t candidate_bytes;
    uint64_t candidate_digest;
    uint32_t candidate_regions;
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
    uint64_t candidate_digest = mr_xproc_region_digest_init();

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
        if (fields < 5) {
            plan->skipped_malformed++;
            continue;
        }
        if (end <= begin) {
            plan->skipped_malformed++;
            continue;
        }

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
        /* Kernel advice takes page-aligned ranges; round inward so the range is
         * entirely inside the mapping. */
        uint64_t aligned_start = ((uint64_t)begin + alignment - 1u) & ~(alignment - 1u);
        uint64_t aligned_end = (uint64_t)end & ~(alignment - 1u);
        if (aligned_end <= aligned_start) {
            plan->skipped_malformed++;
            continue;
        }

        uint64_t aligned_length = aligned_end - aligned_start;
        if (UINT64_MAX - plan->candidate_bytes < aligned_length ||
            plan->candidate_regions == UINT32_MAX) {
            (void)fclose(maps);
            errno = EOVERFLOW;
            return -1;
        }
        plan->candidate_bytes += aligned_length;
        plan->candidate_regions++;
        candidate_digest = mr_xproc_region_digest_add(
            candidate_digest, aligned_start, aligned_end, perms,
            (uint64_t)offset, device, (uint64_t)inode, name);

        if (plan->bytes >= max_bytes) {
            plan->skipped_budget++;
            continue;
        }
        if (plan->count >= MR_XPROC_MAX_IOVECS) {
            plan->skipped_budget++;
            continue;
        }

        uint64_t remaining = max_bytes - plan->bytes;
        uint64_t request_length = aligned_length < remaining ? aligned_length : remaining;
        request_length &= ~(alignment - 1u);
        if (request_length == 0u) {
            plan->skipped_budget++;
            continue;
        }
        if (request_length < aligned_length) plan->skipped_too_large++;

        plan->vectors[plan->count].iov_base = (void *)(uintptr_t)aligned_start;
        plan->vectors[plan->count].iov_len = (size_t)request_length;
        plan->count++;
        plan->bytes += request_length;
    }
    int read_error = ferror(maps);
    (void)fclose(maps);
    if (read_error) {
        return -1;
    }
    plan->candidate_digest = mr_xproc_region_digest_finish(
        candidate_digest, plan->candidate_regions);
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
        case MR_XPROC_STATUS_SCOPE_UNAVAILABLE: return "scope_unavailable";
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
    const char *mode;
    uint64_t request_id;
    int32_t pid;
    /* The caller's claim, echoed back so the GUI can prove the helper answered
     * about the target it asked for. This is what the caller said, not what the
     * helper verified; `start_time_verified` is the separate answer. */
    uint64_t start_time;
    int start_time_verified;
    uint64_t expected_identity_hash;
    uint64_t observed_identity_hash;
    int identity_hash_verified;
    uint64_t expected_scope_hash;
    uint64_t observed_scope_hash;
    int scope_hash_verified;
    uint64_t expected_region_digest;
    uint64_t observed_region_digest;
    int region_digest_verified;
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
    int64_t target_rss_before_kib;
    int64_t target_rss_after_kib;
    int64_t target_swap_before_kib;
    int64_t target_swap_after_kib;
    int64_t scope_current_before_bytes;
    int64_t scope_current_after_bytes;
    int64_t scope_anon_before_bytes;
    int64_t scope_anon_after_bytes;
    int64_t scope_file_before_bytes;
    int64_t scope_file_after_bytes;
    int64_t scope_swap_before_bytes;
    int64_t scope_swap_after_bytes;
};

static void emit_measurement(const char *name, int64_t value)
{
    if (value < 0) emit(stdout, "%s=unavailable\n", name);
    else emit(stdout, "%s=%lld\n", name, (long long)value);
}

static void print_outcome(const struct mr_outcome *outcome)
{
    emit(stdout, "MR_CROSSPROC_RESULT\n");
    emit(stdout, "protocol_version=%u\n", (unsigned)MR_XPROC_PROTOCOL_VERSION);
    emit(stdout, "protocol_marker=%s\n", MR_XPROC_PROTOCOL_MARKER);
    emit(stdout, "mode=%s\n", outcome->mode == NULL ? "unknown" : outcome->mode);
    emit(stdout, "status=%s\n", classify_status(outcome->status));
    emit(stdout, "reason=%s\n", outcome->reason == NULL ? "none" : outcome->reason);
    emit(stdout, "request_id=%llu\n", (unsigned long long)outcome->request_id);
    emit(stdout, "pid=%d\n", (int)outcome->pid);
    emit(stdout, "start_time=%llu\n", (unsigned long long)outcome->start_time);
    emit(stdout, "start_time_verified=%s\n", outcome->start_time_verified ? "true" : "false");
    emit(stdout, "expected_identity_hash=%llu\n",
         (unsigned long long)outcome->expected_identity_hash);
    emit(stdout, "observed_identity_hash=%llu\n",
         (unsigned long long)outcome->observed_identity_hash);
    emit(stdout, "identity_hash_verified=%s\n",
         outcome->identity_hash_verified ? "true" : "false");
    emit(stdout, "expected_scope_hash=%llu\n",
         (unsigned long long)outcome->expected_scope_hash);
    emit(stdout, "observed_scope_hash=%llu\n",
         (unsigned long long)outcome->observed_scope_hash);
    emit(stdout, "scope_hash_verified=%s\n",
         outcome->scope_hash_verified ? "true" : "false");
    emit(stdout, "expected_region_digest=%llu\n",
         (unsigned long long)outcome->expected_region_digest);
    emit(stdout, "observed_region_digest=%llu\n",
         (unsigned long long)outcome->observed_region_digest);
    emit(stdout, "region_digest_verified=%s\n",
         outcome->region_digest_verified ? "true" : "false");
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
    emit_measurement("target_rss_before_kib", outcome->target_rss_before_kib);
    emit_measurement("target_rss_after_kib", outcome->target_rss_after_kib);
    emit_measurement("target_swap_before_kib", outcome->target_swap_before_kib);
    emit_measurement("target_swap_after_kib", outcome->target_swap_after_kib);
    emit_measurement("scope_current_before_bytes", outcome->scope_current_before_bytes);
    emit_measurement("scope_current_after_bytes", outcome->scope_current_after_bytes);
    emit_measurement("scope_anon_before_bytes", outcome->scope_anon_before_bytes);
    emit_measurement("scope_anon_after_bytes", outcome->scope_anon_after_bytes);
    emit_measurement("scope_file_before_bytes", outcome->scope_file_before_bytes);
    emit_measurement("scope_file_after_bytes", outcome->scope_file_after_bytes);
    emit_measurement("scope_swap_before_bytes", outcome->scope_swap_before_bytes);
    emit_measurement("scope_swap_after_bytes", outcome->scope_swap_after_bytes);
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
         "usage: %s reclaim <pageout|app_scope> <request_id> <pid> <start_time> <uid> <identity_hash> <scope_hash> <region_digest> <reviewed_bytes> <reviewed_regions> <max_bytes>\n"
         "All numeric values are decimal and treated as untrusted candidates;\n"
         "this helper re-verifies identity, ownership and mappings from the kernel.\n",
         MR_XPROC_HELPER_NAME);
    return 2;
}

int main(int argc, char **argv)
{
    struct mr_outcome outcome;
    memset(&outcome, 0, sizeof(outcome));
    outcome.target_rss_before_kib = -1;
    outcome.target_rss_after_kib = -1;
    outcome.target_swap_before_kib = -1;
    outcome.target_swap_after_kib = -1;
    outcome.scope_current_before_bytes = -1;
    outcome.scope_current_after_bytes = -1;
    outcome.scope_anon_before_bytes = -1;
    outcome.scope_anon_after_bytes = -1;
    outcome.scope_file_before_bytes = -1;
    outcome.scope_file_after_bytes = -1;
    outcome.scope_swap_before_bytes = -1;
    outcome.scope_swap_after_bytes = -1;

    /* Twelve arguments follow argv[0]: fixed verb and mode plus ten decimal
     * claims. No path, command, or arbitrary action is accepted. */
    if (argc != 13) {
        return usage();
    }
    if (strcmp(argv[1], "reclaim") != 0) {
        return usage();
    }
    if (strcmp(argv[2], MR_XPROC_MODE_PAGEOUT_TEXT) == 0) {
        outcome.mode = MR_XPROC_MODE_PAGEOUT_TEXT;
    } else if (strcmp(argv[2], MR_XPROC_MODE_APP_SCOPE_TEXT) == 0) {
        outcome.mode = MR_XPROC_MODE_APP_SCOPE_TEXT;
    } else {
        return usage();
    }

    uint64_t raw_request_id = 0;
    uint64_t raw_pid = 0;
    uint64_t raw_start_time = 0;
    uint64_t raw_uid = 0;
    uint64_t raw_identity_token = 0;
    uint64_t raw_scope_token = 0;
    uint64_t raw_region_digest = 0;
    uint64_t raw_reviewed_bytes = 0;
    uint64_t raw_reviewed_regions = 0;
    uint64_t raw_max_bytes = 0;
    if (parse_u64(argv[3], 0x7fffffffffffffffull, &raw_request_id) != 0 ||
        parse_u64(argv[4], 0x7fffffffull, &raw_pid) != 0 ||
        parse_u64(argv[5], UINT64_MAX, &raw_start_time) != 0 ||
        parse_u64(argv[6], UINT32_MAX, &raw_uid) != 0 ||
        parse_u64(argv[7], 0x7fffffffffffffffull, &raw_identity_token) != 0 ||
        parse_u64(argv[8], 0x7fffffffffffffffull, &raw_scope_token) != 0 ||
        parse_u64(argv[9], 0x7fffffffffffffffull, &raw_region_digest) != 0 ||
        parse_u64(argv[10], UINT64_MAX, &raw_reviewed_bytes) != 0 ||
        parse_u64(argv[11], UINT32_MAX, &raw_reviewed_regions) != 0 ||
        parse_u64(argv[12], MR_XPROC_MAX_BYTES, &raw_max_bytes) != 0) {
        return usage();
    }
    if (raw_request_id == 0 || raw_pid < 2 || raw_uid == 0 ||
        raw_identity_token == 0 || raw_scope_token == 0 || raw_region_digest == 0 ||
        ((raw_reviewed_bytes == 0u) != (raw_reviewed_regions == 0u)) ||
        raw_max_bytes < page_size()) {
        /* PID 1 is never a target, and a budget below one page cannot describe
         * a page-aligned range. */
        return usage();
    }
    outcome.request_id = raw_request_id;
    outcome.pid = (int32_t)raw_pid;
    outcome.start_time = raw_start_time;
    outcome.expected_identity_hash = raw_identity_token;
    outcome.expected_scope_hash = raw_scope_token;
    outcome.expected_region_digest = raw_region_digest;
    /* Unknown until the kernel tells us it matches. Nothing is verified yet. */
    outcome.start_time_verified = 0;
    outcome.uid = -1;

    /* ---- 1. Validate the root target credentials used by pkexec. ---- */
    uint64_t requester_uid = 0;
    uint64_t real_uid = test_skip_privilege() ? 0u : (uint64_t)getuid();
    uint64_t effective_uid = test_skip_privilege() ? 0u : (uint64_t)geteuid();
    const char *pkexec_uid = test_skip_requester() ? "1" : getenv("PKEXEC_UID");
    const char *credential_error = validate_helper_credentials(
        real_uid, effective_uid, pkexec_uid, &requester_uid);
    if (credential_error != NULL) {
        refuse(&outcome, credential_error);
        print_outcome(&outcome);
        return status_exit_code(outcome.status);
    }
    if (strcmp(outcome.mode, MR_XPROC_MODE_PAGEOUT_TEXT) == 0) {
        int capability = test_skip_capability() ? 1 : has_effective_cap(CAP_SYS_NICE);
        const char *capability_error = validate_pageout_capability(capability);
        if (capability_error != NULL) {
            refuse(&outcome, capability_error);
            print_outcome(&outcome);
            return status_exit_code(outcome.status);
        }
    }
    if (!test_skip_requester() && raw_uid != requester_uid) {
        refuse(&outcome, "selected_uid_mismatch");
        print_outcome(&outcome);
        return status_exit_code(outcome.status);
    }

    /* ---- 2. Target identity, re-derived from the kernel. ---- */
    if (outcome.pid == 1) {
        refuse(&outcome, "target_is_pid1");
        print_outcome(&outcome);
        return status_exit_code(outcome.status);
    }
    if (target_is_self(outcome.pid, (int32_t)getpid())) {
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
    uint64_t identity_token = 0;
    uint64_t scope_token = 0;
    const char *target_error = inspect_target(outcome.pid, requester_uid,
                                              !test_skip_requester(), &target_uid,
                                              &identity_token, &scope_token);
    if (target_error != NULL) {
        refuse(&outcome, target_error);
        print_outcome(&outcome);
        return status_exit_code(outcome.status);
    }
    outcome.observed_identity_hash = identity_token;
    outcome.observed_scope_hash = scope_token;
    outcome.identity_hash_verified = identity_token == raw_identity_token;
    outcome.scope_hash_verified = scope_token == raw_scope_token;
    if (target_uid != raw_uid || !outcome.identity_hash_verified ||
        !outcome.scope_hash_verified) {
        refuse(&outcome, "target_identity_changed");
        print_outcome(&outcome);
        return status_exit_code(outcome.status);
    }
    outcome.uid = (int64_t)target_uid;
    /* From here on the start_time in this report is the value the kernel
     * confirmed, not merely the value the caller supplied. */
    outcome.start_time_verified = 1;

    (void)read_target_memory_kib(outcome.pid, "VmRSS", &outcome.target_rss_before_kib);
    (void)read_target_memory_kib(outcome.pid, "VmSwap", &outcome.target_swap_before_kib);
    if (strcmp(outcome.mode, MR_XPROC_MODE_PAGEOUT_TEXT) == 0 &&
        outcome.target_rss_before_kib < 0) {
        refuse(&outcome, "target_rss_unavailable");
        print_outcome(&outcome);
        return status_exit_code(outcome.status);
    }

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
    uint64_t pinned_identity_token = 0;
    uint64_t pinned_scope_token = 0;
    target_error = inspect_target(outcome.pid, requester_uid,
                                  !test_skip_requester(), &pinned_uid,
                                  &pinned_identity_token, &pinned_scope_token);
    outcome.observed_identity_hash = pinned_identity_token;
    outcome.observed_scope_hash = pinned_scope_token;
    outcome.identity_hash_verified = pinned_identity_token == raw_identity_token;
    outcome.scope_hash_verified = pinned_scope_token == raw_scope_token;
    if (target_error != NULL || pinned_uid != (uint64_t)outcome.uid ||
        pinned_uid != raw_uid || !outcome.identity_hash_verified ||
        !outcome.scope_hash_verified) {
        (void)close(pidfd);
        refuse(&outcome, target_error == NULL ? "target_identity_changed" : target_error);
        print_outcome(&outcome);
        return status_exit_code(outcome.status);
    }

    int scope_fd = -1;
    char scope_path[MR_CGROUP_PATH_BYTES] = {0};
    struct mr_scope_snapshot scope_before;
    scope_before.current_bytes = -1;
    scope_before.anon_bytes = -1;
    scope_before.file_bytes = -1;
    scope_before.swap_bytes = -1;
    uint64_t effective_budget = raw_max_bytes;
    if (strcmp(outcome.mode, MR_XPROC_MODE_APP_SCOPE_TEXT) == 0) {
        if (open_scope_directory(outcome.pid, requester_uid, &scope_fd,
                                 scope_path, sizeof(scope_path)) != 0) {
            outcome.error_code = errno;
            close(pidfd);
            outcome.status = MR_XPROC_STATUS_SCOPE_UNAVAILABLE;
            outcome.reason = "app_scope_directory_unavailable";
            print_outcome(&outcome);
            return status_exit_code(outcome.status);
        }
        if (scope_has_no_child_cgroups(scope_fd) != 0) {
            outcome.error_code = errno;
            (void)close(scope_fd);
            (void)close(pidfd);
            outcome.status = MR_XPROC_STATUS_SCOPE_UNAVAILABLE;
            outcome.reason = "app_scope_has_child_cgroup_or_is_unreadable";
            print_outcome(&outcome);
            return status_exit_code(outcome.status);
        }
        if (validate_scope_members(scope_fd, outcome.pid, (uint32_t)requester_uid,
                                   raw_scope_token) != 0) {
            outcome.error_code = errno;
            (void)close(scope_fd);
            (void)close(pidfd);
            outcome.status = MR_XPROC_STATUS_SCOPE_UNAVAILABLE;
            outcome.reason = "app_scope_membership_unverifiable";
            print_outcome(&outcome);
            return status_exit_code(outcome.status);
        }
        read_scope_snapshot(scope_fd, &scope_before);
        if (scope_before.current_bytes < 0 || scope_before.anon_bytes < 0 ||
            scope_before.file_bytes < 0) {
            (void)close(scope_fd);
            (void)close(pidfd);
            outcome.status = MR_XPROC_STATUS_SCOPE_UNAVAILABLE;
            outcome.reason = "app_scope_memory_stats_unavailable";
            print_outcome(&outcome);
            return status_exit_code(outcome.status);
        }
        outcome.scope_current_before_bytes = scope_before.current_bytes;
        outcome.scope_anon_before_bytes = scope_before.anon_bytes;
        outcome.scope_file_before_bytes = scope_before.file_bytes;
        outcome.scope_swap_before_bytes = scope_before.swap_bytes;
        uint64_t resident_scope_bytes = (uint64_t)scope_before.anon_bytes +
                                        (uint64_t)scope_before.file_bytes;
        if (resident_scope_bytes < (uint64_t)scope_before.anon_bytes) {
            (void)close(scope_fd);
            (void)close(pidfd);
            refuse(&outcome, "app_scope_memory_stats_overflow");
            print_outcome(&outcome);
            return status_exit_code(outcome.status);
        }
        if (effective_budget > resident_scope_bytes) effective_budget = resident_scope_bytes;
        if (effective_budget < page_size()) {
            (void)close(scope_fd);
            (void)close(pidfd);
            outcome.status = MR_XPROC_STATUS_NO_REGIONS;
            outcome.reason = "app_scope_has_no_page_sized_anon_or_file_charge";
            print_outcome(&outcome);
            return status_exit_code(outcome.status);
        }
    } else {
        uint64_t rss_bytes = (uint64_t)outcome.target_rss_before_kib * 1024u;
        if (rss_bytes / 1024u != (uint64_t)outcome.target_rss_before_kib) {
            (void)close(pidfd);
            refuse(&outcome, "target_rss_overflow");
            print_outcome(&outcome);
            return status_exit_code(outcome.status);
        }
        if (effective_budget > rss_bytes) effective_budget = rss_bytes;
    }

    /* ---- 4. Choose ranges here, from the kernel's own maps. ---- */
    struct mr_region_plan plan;
    int planned = plan_regions(outcome.pid, effective_budget, &plan);
    outcome.skipped_shared = plan.skipped_shared;
    outcome.skipped_unreadable = plan.skipped_unreadable;
    outcome.skipped_writable_file = plan.skipped_writable_file;
    outcome.skipped_special = plan.skipped_special;
    outcome.skipped_too_large = plan.skipped_too_large;
    outcome.skipped_budget = plan.skipped_budget;
    outcome.skipped_malformed = plan.skipped_malformed;

    if (planned < 0) {
        if (scope_fd >= 0) (void)close(scope_fd);
        (void)close(pidfd);
        refuse(&outcome, "target_maps_unreadable");
        print_outcome(&outcome);
        return status_exit_code(outcome.status);
    }
    outcome.observed_region_digest = plan.candidate_digest;
    outcome.region_digest_verified = plan.candidate_digest == raw_region_digest;
    if (plan.candidate_bytes != raw_reviewed_bytes ||
        plan.candidate_regions != raw_reviewed_regions ||
        !outcome.region_digest_verified) {
        if (scope_fd >= 0) (void)close(scope_fd);
        (void)close(pidfd);
        refuse(&outcome, "target_mappings_changed");
        print_outcome(&outcome);
        return status_exit_code(outcome.status);
    }
    if (planned == 0 && strcmp(outcome.mode, MR_XPROC_MODE_PAGEOUT_TEXT) == 0) {
        if (scope_fd >= 0) (void)close(scope_fd);
        (void)close(pidfd);
        outcome.status = MR_XPROC_STATUS_NO_REGIONS;
        outcome.reason = "no_reclaimable_private_region";
        print_outcome(&outcome);
        return status_exit_code(outcome.status);
    }

    outcome.regions = strcmp(outcome.mode, MR_XPROC_MODE_PAGEOUT_TEXT) == 0
                          ? (uint32_t)plan.count : 0u;
    outcome.requested_bytes = strcmp(outcome.mode, MR_XPROC_MODE_PAGEOUT_TEXT) == 0
                                  ? plan.bytes : effective_budget;

    long advised = 0;
    int call_errno = 0;
    if (strcmp(outcome.mode, MR_XPROC_MODE_PAGEOUT_TEXT) == 0) {
        /* One bounded process_madvise call, at most 1 GiB and 1024 vectors.
         * A partial return is reported and is never retried. */
        mr_xproc_audit_emit("helper_pre_syscall", raw_request_id, outcome.pid,
                            outcome.start_time, raw_uid, raw_scope_token,
                            raw_identity_token, raw_region_digest);
        errno = 0;
        advised = call_process_madvise(pidfd, plan.vectors, plan.count);
        call_errno = errno;
        outcome.call_count = 1;
    } else {
        char current_scope_path[MR_CGROUP_PATH_BYTES];
        uint64_t current_uid = 0;
        uint64_t current_identity = 0;
        uint64_t current_scope_hash = 0;
        struct mr_proc_stat current_stat;
        int still_same = read_stat(outcome.pid, &current_stat) == 0 &&
                         current_stat.start_time == outcome.start_time &&
                         current_stat.state != 'Z' && current_stat.state != 'X' &&
                         current_stat.state != 'x' &&
                         inspect_target(outcome.pid, requester_uid, 1, &current_uid,
                                        &current_identity, &current_scope_hash) == NULL &&
                         current_uid == requester_uid &&
                         current_identity == raw_identity_token &&
                         current_scope_hash == raw_scope_token &&
                         read_cgroup_path(outcome.pid, (uint32_t)requester_uid,
                                          current_scope_path, sizeof(current_scope_path)) == 0 &&
                         strcmp(current_scope_path, scope_path) == 0 &&
                         validate_scope_members(scope_fd, outcome.pid,
                                                (uint32_t)requester_uid,
                                                raw_scope_token) == 0;
        if (!still_same) {
            (void)close(scope_fd);
            (void)close(pidfd);
            outcome.status = MR_XPROC_STATUS_REFUSED;
            outcome.reason = "app_scope_changed_before_reclaim";
            print_outcome(&outcome);
            return status_exit_code(outcome.status);
        }
        mr_xproc_audit_emit("helper_pre_scope_write", raw_request_id, outcome.pid,
                            outcome.start_time, raw_uid, raw_scope_token,
                            raw_identity_token, raw_region_digest);
        const char *scope_reason = scope_reclaim_once(scope_fd, effective_budget,
                                                     &call_errno);
        outcome.call_count = strcmp(scope_reason, "scope_memory_reclaim_unavailable") == 0
                                 ? 0 : 1;
        outcome.reason = scope_reason;
        if (strcmp(scope_reason, "scope_reclaim_request_accepted") == 0) {
            outcome.status = MR_XPROC_STATUS_ACCEPTED;
        } else if (strcmp(scope_reason, "scope_reclaim_shortfall") == 0) {
            outcome.status = MR_XPROC_STATUS_PARTIAL;
        } else if (strcmp(scope_reason, "scope_reclaim_outcome_uncertain") == 0) {
            outcome.status = MR_XPROC_STATUS_UNKNOWN;
        } else {
            outcome.status = MR_XPROC_STATUS_SCOPE_UNAVAILABLE;
        }
        outcome.error_code = call_errno;
    }

    /* Whatever happened, the pinned target must still be the same process. */
    struct mr_proc_stat stat_after;
    int identity_stable = read_stat(outcome.pid, &stat_after) == 0 &&
                          stat_after.start_time == outcome.start_time;
    (void)read_target_memory_kib(outcome.pid, "VmRSS", &outcome.target_rss_after_kib);
    (void)read_target_memory_kib(outcome.pid, "VmSwap", &outcome.target_swap_after_kib);
    if (scope_fd >= 0) {
        struct mr_scope_snapshot scope_after;
        read_scope_snapshot(scope_fd, &scope_after);
        outcome.scope_current_after_bytes = scope_after.current_bytes;
        outcome.scope_anon_after_bytes = scope_after.anon_bytes;
        outcome.scope_file_after_bytes = scope_after.file_bytes;
        outcome.scope_swap_after_bytes = scope_after.swap_bytes;
        (void)close(scope_fd);
    }
    int close_result = close(pidfd);
    (void)close_result; /* nothing downstream depends on it */

    if (strcmp(outcome.mode, MR_XPROC_MODE_APP_SCOPE_TEXT) == 0) {
        if (!identity_stable) {
            outcome.reason = "target_identity_changed_after_scope_reclaim";
            if (outcome.status == MR_XPROC_STATUS_ACCEPTED) {
                outcome.status = MR_XPROC_STATUS_PARTIAL;
            }
        }
        print_outcome(&outcome);
        return status_exit_code(outcome.status);
    }

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
