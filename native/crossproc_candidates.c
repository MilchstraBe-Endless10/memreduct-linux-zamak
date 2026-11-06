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
#include <linux/magic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/types.h>
#include <unistd.h>

#include "crossproc_candidates.h"
#include "crossproc_identity.h"
#include "crossproc_protection.h"

#ifndef MR_CAND_PROC_ROOT
#  define MR_CAND_PROC_ROOT "/proc"
#endif
#ifndef MR_CAND_CGROUP_ROOT
#  define MR_CAND_CGROUP_ROOT "/sys/fs/cgroup"
#endif

static const char *cand_proc_root = MR_CAND_PROC_ROOT;
static const char *cand_cgroup_root = MR_CAND_CGROUP_ROOT;

#ifdef MEMREDUCT_TESTING
/* Test build only: point enumeration at a fixture tree with fake PIDs. The
 * production binary has no code path that reads this variable. */
static void cand_test_root(void)
{
    const char *value = getenv("MEMREDUCT_TEST_PROC_ROOT");
    if (value != NULL && value[0] == '/') {
        cand_proc_root = value;
    }
    value = getenv("MEMREDUCT_TEST_CGROUP_ROOT");
    if (value != NULL && value[0] == '/') {
        cand_cgroup_root = value;
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
    if ((size_t)total + 1u >= size) {
        char extra;
        ssize_t got;
        do {
            got = read(fd, &extra, 1u);
        } while (got < 0 && errno == EINTR);
        if (got != 0) {
            (void)close(fd);
            return -1;
        }
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
    int64_t rss_bytes;
    char state;
    int have_name;
    int have_uid;
    int have_rss;
    int have_state;
};

static int parse_decimal(const char *text, size_t length, uint64_t *out);

/* Names from procfs are bytes controlled by the target process. Keep drawing
 * inputs valid UTF-8 and replace control or malformed bytes before returning
 * them to the GTK text renderer. The helper still uses the original values for
 * its independent safety checks. */
static size_t valid_utf8_sequence(const unsigned char *text)
{
    const unsigned char first = text[0];
    if (first >= 0xc2u && first <= 0xdfu &&
        (text[1] & 0xc0u) == 0x80u) {
        return 2u;
    }
    if (first >= 0xe0u && first <= 0xefu && text[1] != '\0' && text[2] != '\0' &&
        (text[1] & 0xc0u) == 0x80u && (text[2] & 0xc0u) == 0x80u &&
        !(first == 0xe0u && text[1] < 0xa0u) &&
        !(first == 0xedu && text[1] >= 0xa0u)) {
        return 3u;
    }
    if (first >= 0xf0u && first <= 0xf4u && text[1] != '\0' &&
        text[2] != '\0' && text[3] != '\0' &&
        (text[1] & 0xc0u) == 0x80u && (text[2] & 0xc0u) == 0x80u &&
        (text[3] & 0xc0u) == 0x80u &&
        !(first == 0xf0u && text[1] < 0x90u) &&
        !(first == 0xf4u && text[1] >= 0x90u)) {
        return 4u;
    }
    return 0u;
}

static void copy_display_text(char *out, size_t out_size, const char *source)
{
    const unsigned char *input = (const unsigned char *)source;
    size_t used = 0;
    if (out_size == 0u) {
        return;
    }
    while (*input != '\0' && used + 1u < out_size) {
        if (*input < 0x20u || *input == 0x7fu) {
            out[used++] = '?';
            input++;
        } else if (*input < 0x80u) {
            out[used++] = (char)*input++;
        } else {
            size_t sequence = valid_utf8_sequence(input);
            if (sequence == 0u) {
                out[used++] = '?';
                input++;
            } else if (used + sequence >= out_size) {
                break;
            } else {
                memcpy(out + used, input, sequence);
                used += sequence;
                input += sequence;
            }
        }
    }
    out[used] = '\0';
}

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
    char *line = buffer;
    while (line != NULL && *line != '\0') {
        char *next = strchr(line, '\n');
        if (next != NULL) {
            *next = '\0';
            next++;
        }
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
        } else if (strncmp(line, "VmRSS:", 6) == 0) {
            const char *value = line + 6;
            while (*value == ' ' || *value == '\t') value++;
            const char *amount_end = value;
            while (*amount_end >= '0' && *amount_end <= '9') amount_end++;
            uint64_t kib = 0;
            const char *unit = amount_end;
            while (*unit == ' ' || *unit == '\t') unit++;
            if (parse_decimal(value, (size_t)(amount_end - value), &kib) == 0 &&
                strcmp(unit, "kB") == 0 && kib <= (uint64_t)INT64_MAX / 1024u) {
                out->rss_bytes = (int64_t)(kib * 1024u);
                out->have_rss = 1;
            }
        }
        line = next;
    }
    return (out->have_name && out->have_uid && out->have_state) ? 0 : -1;
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

static int read_cgroup(int32_t pid, char *out, size_t size)
{
    char path[MR_CAND_PROC_PATH_BYTES];
    if (snprintf(path, sizeof(path), "%s/%d/cgroup", cand_proc_root, (int)pid) >=
        (int)sizeof(path)) {
        return -1;
    }
    return read_text(path, out, size);
}

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

static int parse_decimal(const char *text, size_t length, uint64_t *out)
{
    if (text == NULL || out == NULL || length == 0u) return -1;
    uint64_t value = 0;
    for (size_t index = 0; index < length; index++) {
        if (text[index] < '0' || text[index] > '9') return -1;
        unsigned int digit = (unsigned int)(text[index] - '0');
        if (value > (UINT64_MAX - digit) / 10u) return -1;
        value = value * 10u + digit;
    }
    *out = value;
    return 0;
}

/* Open only the exact user app scope already accepted by the shared policy.
 * Each component is opened relative to the verified parent without following
 * symlinks, matching the privileged helper's traversal. */
static int open_app_scope(const char *cgroup, uint32_t uid)
{
    if (cgroup == NULL || mr_xproc_cgroup_text_is_user_app(cgroup, uid) != 1 ||
        strncmp(cgroup, "0::/", 4u) != 0) return -1;
    const char *line_end = strchr(cgroup, '\n');
    if (line_end == NULL || line_end[1] != '\0') return -1;
    size_t path_length = (size_t)(line_end - (cgroup + 3));
    if (path_length < 2u || path_length >= MR_CAND_PROC_PATH_BYTES) return -1;

    int current = open(cand_cgroup_root,
                       O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (current < 0) return -1;
#ifndef MEMREDUCT_TESTING
    struct statfs fs;
    if (fstatfs(current, &fs) != 0 || (unsigned long)fs.f_type != CGROUP2_SUPER_MAGIC) {
        (void)close(current);
        errno = ENODEV;
        return -1;
    }
#endif
    const char *cursor = cgroup + 4;
    const char *end = line_end;
    while (cursor < end) {
        const char *slash = memchr(cursor, '/', (size_t)(end - cursor));
        size_t length = slash == NULL ? (size_t)(end - cursor)
                                      : (size_t)(slash - cursor);
        if (length == 0u || length >= NAME_MAX ||
            (length == 1u && cursor[0] == '.') ||
            (length == 2u && cursor[0] == '.' && cursor[1] == '.')) {
            (void)close(current);
            errno = EINVAL;
            return -1;
        }
        char component[NAME_MAX];
        memcpy(component, cursor, length);
        component[length] = '\0';
        int next = openat(current, component,
                          O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        int saved = errno;
        (void)close(current);
        if (next < 0) {
            errno = saved;
            return -1;
        }
        current = next;
        cursor = slash == NULL ? end : slash + 1;
    }
    return current;
}

static int app_scope_has_no_children(int scope_fd)
{
    int duplicate = dup(scope_fd);
    if (duplicate < 0) return -1;
    DIR *directory = fdopendir(duplicate);
    if (directory == NULL) {
        (void)close(duplicate);
        return -1;
    }
    int valid = 1;
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        struct stat info;
        if (fstatat(scope_fd, entry->d_name, &info, AT_SYMLINK_NOFOLLOW) != 0 ||
            S_ISDIR(info.st_mode) || S_ISLNK(info.st_mode)) {
            valid = 0;
            break;
        }
    }
    (void)closedir(directory);
    return valid ? 0 : -1;
}

/* The sum of anon+file cgroup charges is a useful upper bound for a user
 * choice; it is not a reclaimability measurement. Offer whole-scope mode only
 * when the dedicated kernel control and its accounting are both present. */
static int app_scope_anon_file_bytes(const char *cgroup, uint32_t uid,
                                     int64_t *bytes_out)
{
    if (bytes_out == NULL) return -1;
    *bytes_out = -1;
    int scope_fd = open_app_scope(cgroup, uid);
    if (scope_fd < 0) return -1;
    if (app_scope_has_no_children(scope_fd) != 0) {
        (void)close(scope_fd);
        return -1;
    }

    struct stat reclaim_info;
    if (fstatat(scope_fd, "memory.reclaim", &reclaim_info, AT_SYMLINK_NOFOLLOW) != 0 ||
        !S_ISREG(reclaim_info.st_mode)) {
        (void)close(scope_fd);
        return -1;
    }
    int fd = openat(scope_fd, "memory.stat", O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    (void)close(scope_fd);
    if (fd < 0) return -1;
    char contents[8192];
    int length = read_fd_text(fd, contents, sizeof(contents));
    (void)close(fd);
    if (length < 0) return -1;

    uint64_t anon = 0;
    uint64_t file = 0;
    int have_anon = 0;
    int have_file = 0;
    for (char *line = contents; *line != '\0';) {
        char *end = strchr(line, '\n');
        if (end == NULL) end = line + strlen(line);
        char saved = *end;
        *end = '\0';
        char *space = strchr(line, ' ');
        if (space != NULL) {
            *space++ = '\0';
            uint64_t value = 0;
            size_t value_length = strlen(space);
            if (parse_decimal(space, value_length, &value) != 0) {
                *end = saved;
                return -1;
            }
            if (strcmp(line, "anon") == 0) {
                if (have_anon) {
                    *end = saved;
                    return -1;
                }
                anon = value;
                have_anon = 1;
            } else if (strcmp(line, "file") == 0) {
                if (have_file) {
                    *end = saved;
                    return -1;
                }
                file = value;
                have_file = 1;
            }
        }
        *end = saved;
        if (saved == '\0') break;
        line = end + 1;
    }
    if (!have_anon || !have_file || anon > (uint64_t)INT64_MAX - file) return -1;
    *bytes_out = (int64_t)(anon + file);
    return 0;
}

/* Preserve the exact argv bytes for identity binding. A very large or empty
 * cmdline cannot be represented in the bounded snapshot and is therefore not
 * offered. The bytes stay in memory only and are never logged or displayed. */
static int read_cmdline(int32_t pid, unsigned char *out, size_t size,
                        size_t *length_out)
{
    char path[MR_CAND_PROC_PATH_BYTES];
    if (size == 0u || length_out == NULL ||
        snprintf(path, sizeof(path), "%s/%d/cmdline", cand_proc_root, (int)pid) >=
            (int)sizeof(path)) {
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
        ssize_t got = read(fd, out + used, size - used);
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

static void candidate_app_label(const char *exe, const unsigned char *cmdline,
                                size_t cmdline_length, char *out, size_t out_size)
{
    const char *selected = NULL;
    size_t selected_length = 0;
    size_t suffix_length = 0;
    size_t offset = 0;
    while (offset < cmdline_length) {
        size_t length = strnlen((const char *)cmdline + offset, cmdline_length - offset);
        const char *argument = (const char *)cmdline + offset;
        const char *base = strrchr(argument, '/');
        base = base == NULL ? argument : base + 1;
        size_t base_length = length - (size_t)(base - argument);
        if ((base_length > 4u && strncmp(base + base_length - 4u, ".dll", 4u) == 0) ||
            (base_length > 3u && strncmp(base + base_length - 3u, ".py", 3u) == 0) ||
            (base_length > 4u && strncmp(base + base_length - 4u, ".jar", 4u) == 0)) {
            selected = base;
            selected_length = base_length;
            suffix_length = base_length > 4u &&
                strncmp(base + base_length - 4u, ".dll", 4u) == 0 ? 4u :
                base_length > 4u && strncmp(base + base_length - 4u, ".jar", 4u) == 0 ? 4u : 3u;
            break;
        }
        offset += length + 1u;
    }
    if (selected == NULL) {
        selected = strrchr(exe, '/');
        selected = selected == NULL ? exe : selected + 1;
        selected_length = strlen(selected);
    } else if (selected_length > suffix_length) {
        selected_length -= suffix_length;
    }
    size_t copy = selected_length < out_size - 1u ? selected_length : out_size - 1u;
    memcpy(out, selected, copy);
    out[copy] = '\0';
    /* The label is display text only; identity uses the full raw cmdline. */
    char safe[MR_CAND_NAME_BYTES];
    copy_display_text(safe, sizeof(safe), out);
    memcpy(out, safe, strlen(safe) + 1u);
}

/* Same truncation problem the helper documents: the comm field in
 * /proc/<pid>/stat is capped at 15 characters, so "dbus-broker-launcher"
 * arrives as "dbus-broker-lau" and a naive name comparison misses it — this was
 * observed on the live machine. The exe symlink is not truncated, so its
 * basename is checked too. Keeping this in step with the helper matters: a row
 * this enumeration offers and the helper then refuses is a confusing product. */
static int pid_safety(int32_t pid, const char *comm, uint64_t uid,
                      char *exe_out, size_t exe_size,
                      char *cgroup_out, size_t cgroup_size)
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

    if (read_cgroup(pid, cgroup_out, cgroup_size) != 0) {
        return -1;
    }
    int cgroup_result = mr_xproc_cgroup_text_is_user_app(cgroup_out, (uint32_t)uid);
    return cgroup_result == 1 ? 0 : cgroup_result == 0 ? 1 : -1;
}

/* Sum the mapping lengths the helper would also consider. Same filter, so the
 * number shown before the click is the number the helper will actually work
 * with — not a larger, more flattering one. */
static int sum_reclaimable(int32_t pid, uint64_t *bytes_out,
                           uint32_t *regions_out, uint64_t *digest_out)
{
    char path[MR_CAND_PROC_PATH_BYTES];
    char line[4096];
    uint64_t total = 0;
    uint32_t count = 0;
    uint64_t digest = mr_xproc_region_digest_init();

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
        uint64_t aligned_length = aligned_end - aligned_start;
        if (UINT64_MAX - total < aligned_length || count == UINT32_MAX) {
            (void)fclose(maps);
            return -1;
        }
        total += aligned_length;
        count++;
        digest = mr_xproc_region_digest_add(digest, aligned_start, aligned_end,
                                             perms, (uint64_t)offset, device,
                                             (uint64_t)inode, name);
    }
    int read_error = ferror(maps);
    (void)fclose(maps);
    if (read_error) {
        return -1;
    }
    *bytes_out = total;
    *regions_out = count;
    *digest_out = mr_xproc_region_digest_finish(digest, count);
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
        char cgroup[4096];
        int safety = pid_safety(pid, status.name, status.uid, exe, sizeof(exe),
                                cgroup, sizeof(cgroup));
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
        uint64_t region_digest = 0;
        if (sum_reclaimable(pid, &reclaimable, &regions, &region_digest) != 0) {
            out->skipped_kernel_thread++;
            continue;
        }
        if (regions == 0 || reclaimable == 0) {
            out->skipped_no_regions++;
            continue;
        }

        if (out->count >= MR_CAND_MAX) {
            out->skipped_limit++;
            continue;
        }

        struct mr_cand_entry *slot = &out->entries[out->count];
        memset(slot, 0, sizeof(*slot));
        unsigned char cmdline[MR_CAND_CMDLINE_BYTES];
        size_t cmdline_length = 0;
        uint64_t start_time = 0;
        if (read_start_time(pid, &start_time) != 0 ||
            read_cmdline(pid, cmdline, sizeof(cmdline), &cmdline_length) != 0) {
            out->skipped_unreadable++;
            continue;
        }
        slot->pid = pid;
        slot->uid = status.uid;
        slot->state = status.state;
        slot->reclaimable_bytes = reclaimable;
        slot->rss_bytes = status.have_rss ? status.rss_bytes : -1;
        slot->scope_anon_file_bytes = -1;
        (void)app_scope_anon_file_bytes(cgroup, (uint32_t)status.uid,
                                        &slot->scope_anon_file_bytes);
        slot->regions = regions;
        slot->start_time = start_time;
        slot->scope_token = mr_xproc_scope_token(cgroup);
        slot->region_digest = region_digest;
        slot->identity_token = mr_xproc_identity_token(
            pid, start_time, status.uid, status.name, exe, cgroup,
            cmdline, cmdline_length);
        copy_display_text(slot->name, sizeof(slot->name), status.name);
        memcpy(slot->exe, exe, strlen(exe) + 1u);
        candidate_app_label(exe, cmdline, cmdline_length,
                            slot->app_name, sizeof(slot->app_name));
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
static struct mr_cand_entry selected_snapshot;
static int selected_snapshot_valid;
static uint64_t selected_request_id;

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
        case MR_CAND_SKIP_LIMIT: return (int64_t)scan_slot.skipped_limit;
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

int64_t mr_cand_uid(int64_t index)
{
    if (index < 0 || index >= (int64_t)scan_slot.count) {
        return -1;
    }
    return (int64_t)scan_slot.entries[index].uid;
}

int64_t mr_cand_state(int64_t index)
{
    if (index < 0 || index >= (int64_t)scan_slot.count) {
        return -1;
    }
    return (int64_t)(unsigned char)scan_slot.entries[index].state;
}

int64_t mr_cand_reclaimable_bytes(int64_t index)
{
    if (index < 0 || index >= (int64_t)scan_slot.count) {
        return -1;
    }
    return (int64_t)scan_slot.entries[index].reclaimable_bytes;
}

int64_t mr_cand_scope_anon_file_bytes(int64_t index)
{
    if (index < 0 || index >= (int64_t)scan_slot.count) {
        return -1;
    }
    return scan_slot.entries[index].scope_anon_file_bytes;
}

int64_t mr_cand_rss_bytes(int64_t index)
{
    if (index < 0 || index >= (int64_t)scan_slot.count) {
        return -1;
    }
    return scan_slot.entries[index].rss_bytes;
}

int64_t mr_cand_regions(int64_t index)
{
    if (index < 0 || index >= (int64_t)scan_slot.count) {
        return -1;
    }
    return (int64_t)scan_slot.entries[index].regions;
}

int64_t mr_cand_identity_token(int64_t index)
{
    if (index < 0 || index >= (int64_t)scan_slot.count) {
        return -1;
    }
    return (int64_t)scan_slot.entries[index].identity_token;
}

int64_t mr_cand_scope_token(int64_t index)
{
    if (index < 0 || index >= (int64_t)scan_slot.count) {
        return -1;
    }
    return (int64_t)scan_slot.entries[index].scope_token;
}

int64_t mr_cand_region_digest(int64_t index)
{
    if (index < 0 || index >= (int64_t)scan_slot.count) {
        return -1;
    }
    return (int64_t)scan_slot.entries[index].region_digest;
}

static int live_snapshot_matches(const struct mr_cand_entry *expected)
{
    struct cand_status status;
    char exe[MR_CAND_PATH_BYTES];
    char cgroup[4096];
    unsigned char cmdline[MR_CAND_CMDLINE_BYTES];
    size_t cmdline_length = 0;
    uint64_t start_time = 0;
    uint64_t bytes = 0;
    uint64_t region_digest = 0;
    uint32_t regions = 0;

    if (expected == NULL || read_status(expected->pid, &status) != 0 ||
        status.state == 'Z' || status.state == 'X' || status.state == 'x' ||
        status.uid != cand_own_uid() || read_start_time(expected->pid, &start_time) != 0 ||
        start_time != expected->start_time ||
        pid_safety(expected->pid, status.name, status.uid, exe, sizeof(exe),
                   cgroup, sizeof(cgroup)) != 0 ||
        read_cmdline(expected->pid, cmdline, sizeof(cmdline), &cmdline_length) != 0 ||
        sum_reclaimable(expected->pid, &bytes, &regions, &region_digest) != 0) {
        return -1;
    }
    if (bytes != expected->reclaimable_bytes || regions != expected->regions ||
        region_digest != expected->region_digest ||
        mr_xproc_scope_token(cgroup) != expected->scope_token) {
        return -1;
    }
    uint64_t token = mr_xproc_identity_token(
        expected->pid, start_time, status.uid, status.name, exe, cgroup,
        cmdline, cmdline_length);
    return token == expected->identity_token ? 0 : -1;
}

int64_t mr_cand_snapshot_select(int64_t index)
{
    selected_request_id = 0u;
    if (index < 0 || index >= (int64_t)scan_slot.count) {
        selected_snapshot_valid = 0;
        return 1;
    }
    selected_snapshot = scan_slot.entries[index];
    selected_snapshot_valid = 1;
    if (live_snapshot_matches(&selected_snapshot) != 0) {
        selected_snapshot_valid = 0;
        memset(&selected_snapshot, 0, sizeof(selected_snapshot));
        return 2;
    }
    return 0;
}

int64_t mr_cand_snapshot_bind_request(int64_t request_id)
{
    if (!selected_snapshot_valid || request_id <= 0) {
        selected_request_id = 0u;
        return 1;
    }
    selected_request_id = (uint64_t)request_id;
    mr_xproc_audit_emit("selection", selected_request_id,
                        selected_snapshot.pid, selected_snapshot.start_time,
                        selected_snapshot.uid, selected_snapshot.scope_token,
                        selected_snapshot.identity_token,
                        selected_snapshot.region_digest);
    return 0;
}

int64_t mr_cand_snapshot_request_id(void)
{
    return selected_request_id > 0u && selected_request_id <= (uint64_t)INT64_MAX
               ? (int64_t)selected_request_id
               : -1;
}

void mr_cand_snapshot_clear(void)
{
    selected_snapshot_valid = 0;
    selected_request_id = 0u;
    memset(&selected_snapshot, 0, sizeof(selected_snapshot));
}

int64_t mr_cand_snapshot_verify(int64_t request_id, int64_t pid,
                                int64_t start_time, int64_t uid,
                                int64_t identity_token, int64_t scope_token,
                                int64_t region_digest, int64_t bytes,
                                int64_t regions)
{
    if (!selected_snapshot_valid || selected_request_id == 0u ||
        request_id <= 0 || (uint64_t)request_id != selected_request_id ||
        pid < 2 || start_time <= 0 || uid < 0 || identity_token <= 0 ||
        scope_token <= 0 || region_digest <= 0 || bytes <= 0 || regions <= 0 ||
        selected_snapshot.pid != pid ||
        selected_snapshot.start_time != (uint64_t)start_time ||
        selected_snapshot.uid != (uint64_t)uid ||
        selected_snapshot.identity_token != (uint64_t)identity_token ||
        selected_snapshot.scope_token != (uint64_t)scope_token ||
        selected_snapshot.region_digest != (uint64_t)region_digest ||
        selected_snapshot.reclaimable_bytes != (uint64_t)bytes ||
        selected_snapshot.regions != (uint32_t)regions) {
        return 1;
    }
    return live_snapshot_matches(&selected_snapshot) == 0 ? 0 : 2;
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

const char *mr_cand_app_name(int64_t index)
{
    if (index < 0 || index >= (int64_t)scan_slot.count) {
        return "";
    }
    return scan_slot.entries[index].app_name;
}
