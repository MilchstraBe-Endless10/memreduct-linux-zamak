/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The unprivileged execute path for balanced clean (Issue #12).
 *
 * Measured on the target host: with the `memory` controller delegated to the
 * user session, every `app-*.scope` in the user's slice has `memory.reclaim`
 * owned by that user with mode 0200. A uid-1000 process can open and write it,
 * for its own scopes and for other apps' scopes. No pkexec, no Polkit, no root.
 *
 * That fact is the whole reason this file exists rather than routing the
 * balanced action through crossproc_helper.c.
 */
#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "balance_exec.h"

#define MIB (1024ull * 1024ull)
#define RECLAIM_FILE "memory.reclaim"

/* A scope name must be a bare name. Anything that could escape the root, or
 * that names a path, is refused rather than normalised: normalising a hostile
 * name is how a "confined" open becomes an unconfined one. */
static int scope_name_is_bare(const char *name)
{
    if (name == NULL || name[0] == '\0') return 0;
    if (name[0] == '/' || name[0] == '.') return 0;
    for (const char *p = name; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\') return 0;
    }
    return 1;
}

int mr_bal_exec_open_scope(const char *scope_root, const char *scope_name)
{
    if (scope_root == NULL || !scope_name_is_bare(scope_name)) {
        errno = EINVAL;
        return -1;
    }
    int root_fd = open(scope_root, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (root_fd < 0) return -1;
    /* O_NOFOLLOW on the scope itself: a symlink planted in the root would
     * otherwise let the caller read or write a cgroup outside its slice. */
    int scope_fd = openat(root_fd, scope_name,
                          O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    int saved = errno;
    (void)close(root_fd);
    if (scope_fd < 0) {
        errno = saved;
        return -1;
    }
    return scope_fd;
}

/* The scope directory's owner is the uid whose cgroup it is. Reading it from
 * the fd, not from the path, means a rename between open and use cannot
 * redirect the check to a different directory. */
int64_t mr_bal_exec_probe(int scope_fd, uint32_t expected_uid)
{
    if (scope_fd < 0) return MR_BAL_EXEC_BAD_REQUEST;

    struct stat scope_info;
    if (fstat(scope_fd, &scope_info) != 0) return MR_BAL_EXEC_REFUSED;
    if (!S_ISDIR(scope_info.st_mode)) return MR_BAL_EXEC_REFUSED;
    if ((uint32_t)scope_info.st_uid != expected_uid) return MR_BAL_EXEC_WRONG_OWNER;

    struct stat control;
    if (fstatat(scope_fd, RECLAIM_FILE, &control, AT_SYMLINK_NOFOLLOW) != 0) {
        /* ENOENT means the `memory` controller is not delegated to this
         * level, which is a different answer from "refused". */
        return errno == ENOENT ? MR_BAL_EXEC_NOT_DELEGATED : MR_BAL_EXEC_REFUSED;
    }
    if (!S_ISREG(control.st_mode)) return MR_BAL_EXEC_REFUSED;
    if ((uint32_t)control.st_uid != expected_uid) return MR_BAL_EXEC_WRONG_OWNER;

    /* A zero-byte request parses the parameter list and, measured on this host,
     * reclaimed exactly 0 bytes. It therefore answers "will this kernel take
     * swappiness=0 here" without touching a page. (A non-zero request that
     * returns EAGAIN can still have reclaimed: see the note in mr_bal_exec_run.) */
    int fd = openat(scope_fd, RECLAIM_FILE, O_WRONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return MR_BAL_EXEC_REFUSED;
    errno = 0;
    ssize_t written = write(fd, "0 swappiness=0", 14);
    int saved = errno;
    (void)close(fd);
    if (written == 14) return MR_BAL_EXEC_OK;
    if (saved == EINVAL) {
        /* The kernel does not understand the parameter here. Without it the
         * write would reclaim anonymous pages, so this is a hard refusal. */
        return MR_BAL_EXEC_REFUSED;
    }
    return MR_BAL_EXEC_REFUSED;
}

static int read_scope_stat(int scope_fd, const char *key, int64_t *out)
{
    int fd = openat(scope_fd, "memory.stat", O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return -1;
    char text[4096];
    ssize_t got = read(fd, text, sizeof(text) - 1);
    (void)close(fd);
    if (got <= 0) return -1;
    text[got] = '\0';
    size_t key_length = strlen(key);
    for (char *line = text; *line != '\0';) {
        char *line_end = strchr(line, '\n');
        if (line_end == NULL) line_end = line + strlen(line);
        if ((size_t)(line_end - line) > key_length &&
            strncmp(line, key, key_length) == 0 && line[key_length] == ' ') {
            uint64_t value = 0;
            int digits = 0;
            for (char *p = line + key_length + 1; *p >= '0' && *p <= '9'; p++) {
                value = value * 10u + (uint64_t)(*p - '0');
                digits++;
            }
            if (digits > 0) {
                *out = (int64_t)value;
                return 0;
            }
            return -1;
        }
        if (*line_end == '\0') break;
        line = line_end + 1;
    }
    return -1;
}

static int read_scope_number(int scope_fd, const char *name, int64_t *out)
{
    int fd = openat(scope_fd, name, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return -1;
    char text[64];
    ssize_t got = read(fd, text, sizeof(text) - 1);
    (void)close(fd);
    if (got <= 0) return -1;
    text[got] = '\0';
    uint64_t value = 0;
    int digits = 0;
    for (char *p = text; *p >= '0' && *p <= '9'; p++) {
        value = value * 10u + (uint64_t)(*p - '0');
        digits++;
    }
    if (digits == 0) return -1;
    *out = (int64_t)value;
    return 0;
}

static void snapshot_scope(int scope_fd, int64_t *current, int64_t *anon,
                           int64_t *file)
{
    *current = -1;
    *anon = -1;
    *file = -1;
    (void)read_scope_number(scope_fd, "memory.current", current);
    (void)read_scope_stat(scope_fd, "anon", anon);
    (void)read_scope_stat(scope_fd, "file", file);
}

static int reclaim_batch(int scope_fd, uint64_t bytes, uint32_t *saved_errno)
{
    *saved_errno = 0;
    int fd = openat(scope_fd, RECLAIM_FILE, O_WRONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        *saved_errno = (uint32_t)errno;
        return -1;
    }
    char value[64];
    int length = snprintf(value, sizeof(value), "%llu swappiness=0",
                          (unsigned long long)bytes);
    if (length <= 0 || (size_t)length >= sizeof(value)) {
        (void)close(fd);
        *saved_errno = (uint32_t)EINVAL;
        return -1;
    }
    errno = 0;
    ssize_t written = write(fd, value, (size_t)length);
    int saved = errno;
    (void)close(fd);
    if (written != (ssize_t)length) {
        *saved_errno = (uint32_t)(written < 0 ? saved : EIO);
        return -1;
    }
    return 0;
}

static uint32_t read_psi_some_centi(void)
{
    int fd = open("/proc/pressure/memory", O_RDONLY | O_CLOEXEC);
    if (fd < 0) return 0;
    char text[256];
    ssize_t got = read(fd, text, sizeof(text) - 1);
    (void)close(fd);
    if (got <= 0) return 0;
    text[got] = '\0';
    const char *avg = strstr(text, "avg10=");
    if (avg == NULL) return 0;
    avg += 6;
    uint32_t whole = 0, frac = 0;
    while (*avg >= '0' && *avg <= '9') { whole = whole * 10u + (uint32_t)(*avg - '0'); avg++; }
    if (*avg == '.') {
        avg++;
        int digits = 0;
        while (*avg >= '0' && *avg <= '9' && digits < 2) {
            frac = frac * 10u + (uint32_t)(*avg - '0');
            avg++;
            digits++;
        }
        while (digits < 2) { frac *= 10u; digits++; }
    }
    return whole * 100u + frac;
}

int mr_bal_exec_run(int scope_fd, const struct mr_bal_exec_config *config,
                    struct mr_bal_exec_result *out)
{
    if (config == NULL || out == NULL) return -1;
    memset(out, 0, sizeof(*out));
    out->status = MR_BAL_EXEC_BAD_REQUEST;
    out->scope_current_before = -1;
    out->scope_current_after = -1;
    out->scope_anon_before = -1;
    out->scope_anon_after = -1;
    out->scope_file_before = -1;
    out->scope_file_after = -1;
    if (scope_fd < 0 || config->budget_bytes == 0 || config->batch_bytes == 0 ||
        config->batch_bytes > config->budget_bytes || config->max_batches == 0) {
        return 0;
    }

    /* Snapshot BEFORE the probe: the probe writes a zero-byte request, and on
     * this kernel that is a no-op, but measuring after it meant any reclaim a
     * kernel did perform during the probe was invisible in the deltas and made
     * the run look like it had changed nothing. Observed live: cgroup
     * file 151552 -> 4096 bytes while the result reported accepted=0. */
    snapshot_scope(scope_fd, &out->scope_current_before,
                   &out->scope_anon_before, &out->scope_file_before);

    int64_t probe = mr_bal_exec_probe(scope_fd, config->expected_uid);
    if (probe != MR_BAL_EXEC_OK) {
        out->status = probe;
        out->stop_reason = MR_BAL_EXEC_STOP_ERROR;
        return 0;
    }

    uint64_t remaining = config->budget_bytes;
    int stopped = 0;
    int64_t stop_reason = MR_BAL_EXEC_STOP_NONE;

    for (uint32_t batch = 0; batch < config->max_batches && remaining > 0; batch++) {
        if (stopped) break;
        uint64_t size = remaining < config->batch_bytes ? remaining : config->batch_bytes;

        if (config->psi_stop_centi > 0 &&
            read_psi_some_centi() >= config->psi_stop_centi) {
            stopped = 1;
            stop_reason = MR_BAL_EXEC_STOP_STALL;
            break;
        }
        int64_t file_now = -1;
        if (read_scope_stat(scope_fd, "file", &file_now) == 0 && file_now == 0) {
            stopped = 1;
            stop_reason = MR_BAL_EXEC_STOP_NO_COLD_PAGES;
            break;
        }

        out->batches_attempted++;
        out->requested_bytes += size;

        /* Measure before and after every batch, because the kernel's answer
         * and the kernel's effect are not the same thing. Measured on this
         * host: a 32 MiB swappiness=0 request returned EAGAIN *and* reclaimed
         * 4 MiB of file pages. Treating EAGAIN as "nothing happened" undersold
         * every run; treating a 0 return as "the full request was taken" would
         * overstate it. Only the scope's own counters are evidence. */
        int64_t file_before = -1;
        int64_t current_before = -1;
        (void)read_scope_stat(scope_fd, "file", &file_before);
        (void)read_scope_number(scope_fd, "memory.current", &current_before);

        uint32_t batch_errno = 0;
        int call_ok = reclaim_batch(scope_fd, size, &batch_errno) == 0;

        int64_t file_after = -1;
        int64_t current_after = -1;
        (void)read_scope_stat(scope_fd, "file", &file_after);
        (void)read_scope_number(scope_fd, "memory.current", &current_after);

        uint64_t took = 0;
        if (current_before >= 0 && current_after >= 0 && current_after < current_before)
            took = (uint64_t)(current_before - current_after);

        if (call_ok) {
            out->batches_accepted++;
            /* The bytes the kernel took, capped by what was asked for: a
             * concurrent allocation can shrink the scope for reasons that are
             * not our request. */
            out->accepted_bytes += took < size ? took : size;
            remaining -= took < size ? took : size;
            if (remaining == 0) {
                stop_reason = MR_BAL_EXEC_STOP_BUDGET_MET;
                break;
            }
            continue;
        }

        /* A rejected batch may still have reclaimed something (EAGAIN does).
         * That is counted as observed, not as accepted. */
        out->batches_rejected++;
        if (out->error_code == 0) out->error_code = (int32_t)batch_errno;
        out->observed_on_reject_bytes += took;
        if (batch_errno == (uint32_t)EAGAIN) {
            stopped = 1;
            stop_reason = took > 0 ? MR_BAL_EXEC_STOP_BUDGET_MET
                                   : MR_BAL_EXEC_STOP_NO_COLD_PAGES;
            break;
        }
        stopped = 1;
        stop_reason = MR_BAL_EXEC_STOP_ERROR;
        break;
    }

    snapshot_scope(scope_fd, &out->scope_current_after,
                   &out->scope_anon_after, &out->scope_file_after);
    out->stop_reason = stop_reason;
    if (out->batches_accepted == 0) out->status = MR_BAL_EXEC_STOPPED;
    else if (stopped) out->status = MR_BAL_EXEC_PARTIAL;
    else out->status = MR_BAL_EXEC_OK;
    return 0;
}

const char *mr_bal_exec_status_name(int64_t status)
{
    switch (status) {
    case MR_BAL_EXEC_OK: return "ok";
    case MR_BAL_EXEC_PARTIAL: return "partial";
    case MR_BAL_EXEC_STOPPED: return "stopped";
    case MR_BAL_EXEC_REFUSED: return "refused";
    case MR_BAL_EXEC_NOT_DELEGATED: return "not_delegated";
    case MR_BAL_EXEC_WRONG_OWNER: return "wrong_owner";
    case MR_BAL_EXEC_BAD_REQUEST: return "bad_request";
    default: return "invalid";
    }
}

const char *mr_bal_exec_stop_name(int64_t stop)
{
    switch (stop) {
    case MR_BAL_EXEC_STOP_NONE: return "none";
    case MR_BAL_EXEC_STOP_NO_COLD_PAGES: return "no_cold_pages";
    case MR_BAL_EXEC_STOP_STALL: return "stall";
    case MR_BAL_EXEC_STOP_ERROR: return "error";
    case MR_BAL_EXEC_STOP_BUDGET_MET: return "budget_met";
    default: return "invalid";
    }
}

/* ---- scope discovery --------------------------------------------------- */

/* Fixed-size table so the bridge can address entries by index. 64 app scopes
 * is far more than a desktop session normally has and keeps the table small. */
#define MAX_LISTED_SCOPES 64

static char listed_names[MAX_LISTED_SCOPES][MR_BAL_SCOPE_NAME_MAX];
static int listed_count;

/* `app-<desktop-id>-<pid>.scope` -> `<desktop-id>`. Anything that does not
 * match that shape is returned unchanged minus the .scope suffix, because
 * inventing a prettier name for an unknown shape is how a UI lies.
 *
 * systemd escapes characters that are special in unit names, so a desktop id
 * containing a dash arrives as `\x2d`. That escape is decoded here, because a
 * user reading "deepseek\x2dai\x2ddsh\x2ddesktop" would have no idea what app
 * that is. */
static void decode_unit_escapes(const char *src, size_t length, char *out, size_t cap)
{
    size_t written = 0;
    for (size_t i = 0; i < length && written + 1 < cap; i++) {
        if (src[i] == '\\' && i + 3 < length && src[i + 1] == 'x') {
            int hi = 0, lo = 0;
            char a = src[i + 2], b = src[i + 3];
            hi = (a >= '0' && a <= '9') ? a - '0'
                 : (a >= 'a' && a <= 'f') ? a - 'a' + 10
                 : (a >= 'A' && a <= 'F') ? a - 'A' + 10 : -1;
            lo = (b >= '0' && b <= '9') ? b - '0'
                 : (b >= 'a' && b <= 'f') ? b - 'a' + 10
                 : (b >= 'A' && b <= 'F') ? b - 'A' + 10 : -1;
            if (hi >= 0 && lo >= 0) {
                out[written++] = (char)(hi * 16 + lo);
                i += 3;
                continue;
            }
        }
        out[written++] = src[i];
    }
    out[written] = '\0';
}

const char *mr_bal_exec_app_label(const char *scope_name)
{
    static char label[MR_BAL_SCOPE_NAME_MAX];
    label[0] = '\0';
    if (scope_name == NULL || scope_name[0] == '\0') return label;
    const char *start = scope_name;
    if (strncmp(start, "app-", 4) == 0) start += 4;
    size_t length = strlen(start);
    if (length > 6 && strcmp(start + length - 6, ".scope") == 0) length -= 6;
    /* Strip the trailing -<pid>, which is all digits after the last dash. */
    size_t cut = length;
    size_t i = length;
    while (i > 0 && start[i - 1] >= '0' && start[i - 1] <= '9') i--;
    if (i > 0 && i < length && start[i - 1] == '-') cut = i - 1;
    if (cut == 0) return label;
    decode_unit_escapes(start, cut, label, sizeof(label));
    return label;
}

int64_t mr_bal_exec_list_scopes(const char *scope_root, uint32_t expected_uid,
                                char *names, uint32_t cap)
{
    listed_count = 0;
    if (scope_root == NULL || names == NULL || cap == 0) return 0;

    int root_fd = open(scope_root, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (root_fd < 0) return 0;
    int dir_fd = dup(root_fd);
    if (dir_fd < 0) {
        (void)close(root_fd);
        return 0;
    }
    DIR *dir = fdopendir(dir_fd);
    if (dir == NULL) {
        (void)close(dir_fd);
        (void)close(root_fd);
        return 0;
    }

    uint32_t written = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (listed_count >= MAX_LISTED_SCOPES || written >= cap) break;
        size_t name_length = strlen(entry->d_name);
        if (name_length == 0 || name_length >= MR_BAL_SCOPE_NAME_MAX) continue;
        if (name_length < 6 || strcmp(entry->d_name + name_length - 6, ".scope") != 0)
            continue;
        /* Only desktop app scopes. `dsh-subprocess-*` is this harness's own
         * plumbing: offering it as an "application to release" would be an
         * unfamiliar name the user never chose. */
        if (strncmp(entry->d_name, "app-", 4) != 0) continue;

        struct stat info;
        if (fstatat(root_fd, entry->d_name, &info, AT_SYMLINK_NOFOLLOW) != 0) continue;
        if (!S_ISDIR(info.st_mode)) continue;
        if ((uint32_t)info.st_uid != expected_uid) continue;

        /* The control file must exist and belong to the same uid; a scope
         * without one cannot be reclaimed in, and offering it would be a
         * promise the run then breaks. */
        char relative[MR_BAL_SCOPE_NAME_MAX + 16];
        int needed = snprintf(relative, sizeof(relative), "%s/%s",
                              entry->d_name, RECLAIM_FILE);
        if (needed <= 0 || (size_t)needed >= sizeof(relative)) continue;
        struct stat control_info;
        if (fstatat(root_fd, relative, &control_info, AT_SYMLINK_NOFOLLOW) != 0) continue;
        if (!S_ISREG(control_info.st_mode)) continue;
        if ((uint32_t)control_info.st_uid != expected_uid) continue;

        memcpy(listed_names[listed_count], entry->d_name, name_length + 1);
        written += (uint32_t)name_length + 1;
        listed_count++;
    }
    (void)closedir(dir);
    (void)close(root_fd);

    /* Copy into the caller's flat buffer, NUL-separated. */
    uint32_t offset = 0;
    for (int i = 0; i < listed_count && offset < cap; i++) {
        size_t length = strlen(listed_names[i]) + 1;
        if (offset + length > cap) break;
        memcpy(names + offset, listed_names[i], length);
        offset += (uint32_t)length;
    }
    return listed_count;
}

const char *mr_bal_exec_scope_name(int64_t index)
{
    if (index < 0 || index >= listed_count) return "";
    return listed_names[index];
}
