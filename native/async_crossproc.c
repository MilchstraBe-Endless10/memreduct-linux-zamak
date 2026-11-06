/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Unprivileged bridge to the cross-process reclaim helper (Issue #12, M1.1).
 *
 * The child's stdout is read through a pipe into a fixed buffer. Nothing here
 * parses stderr: that text comes from pkexec and from an unprivileged process,
 * and the product must not derive an authorization verdict from text it cannot
 * authenticate. The only trusted input is the helper's own result block, and
 * even that is treated as a claim whose numbers are reported, not believed as
 * an outcome in themselves.
 */
#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <spawn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/random.h>
#include <sys/types.h>
#include <time.h>
#include <sys/wait.h>
#include <unistd.h>

#include "async_crossproc.h"
#include "crossproc_identity.h"
#include "crossproc_helper.h"

/* The bridge parses the helper's versioned result ABI. Keep this translation
 * unit tied to the helper header so the build cache cannot reuse a stale
 * protocol marker after an ABI update without the production build check
 * catching it. */
_Static_assert(MR_XPROC_PROTOCOL_VERSION == 4,
               "update the bridge when the Crossproc helper protocol changes");

/* Fixed absolute paths. The helper installs to the first; the Polkit action
 * pins it. Only a test build may override any of them. */
#define MR_XPROC_PKEXEC "/usr/bin/pkexec"
#define MR_XPROC_POLICY_PATH \
    "/usr/share/polkit-1/actions/io.github.milchstrasse_endless10.memreduct.zamak.crossproc.policy"
#define MR_XPROC_POLICY_MAX_BYTES 65536u
#define MR_XPROC_EXEC_PATH_ANNOTATION \
    "org.freedesktop.policykit.exec.path"

/* Room for the whole result block. The helper prints at most ~20 short lines;
 * anything longer is truncated and therefore treated as unparseable rather than
 * silently accepted with missing fields. */
#define MR_XPROC_OUTPUT_BYTES 4096u
#define MR_XPROC_REASON_BYTES 64u

extern char **environ;

static pid_t child_pid = -1;
static int child_stdout = -1;
static char output_buffer[MR_XPROC_OUTPUT_BYTES];
static size_t output_used;
static int output_overflowed;

/* The last classified result. */
static int64_t last_status;
static int64_t last_advised_bytes;
static int64_t last_requested_bytes;
static int64_t last_regions;
static int64_t last_error_code;
static int64_t last_call_count;
static int64_t last_pid;
static int64_t last_start_time;
static int64_t last_start_time_verified;
static int64_t last_uid;
static uint64_t last_request_id;
static uint64_t last_expected_identity_hash;
static uint64_t last_observed_identity_hash;
static int64_t last_identity_hash_verified;
static uint64_t last_expected_scope_hash;
static uint64_t last_observed_scope_hash;
static int64_t last_scope_hash_verified;
static uint64_t last_expected_region_digest;
static uint64_t last_observed_region_digest;
static int64_t last_region_digest_verified;
static int64_t last_exit_code = -1;
static int64_t last_signal;
static int64_t last_mode;
static int64_t last_target_rss_before_kib = -1;
static int64_t last_target_rss_after_kib = -1;
static int64_t last_target_swap_before_kib = -1;
static int64_t last_target_swap_after_kib = -1;
static int64_t last_scope_current_before_bytes = -1;
static int64_t last_scope_current_after_bytes = -1;
static int64_t last_scope_anon_before_bytes = -1;
static int64_t last_scope_anon_after_bytes = -1;
static int64_t last_scope_file_before_bytes = -1;
static int64_t last_scope_file_after_bytes = -1;
static int64_t last_scope_swap_before_bytes = -1;
static int64_t last_scope_swap_after_bytes = -1;
static char last_reason[MR_XPROC_REASON_BYTES];
static int64_t local_refusal;
static int64_t expected_pid;
static int64_t expected_start_time;
static int64_t expected_uid;
static int64_t expected_mode;
static uint64_t expected_request_id;
static uint64_t expected_identity_hash;
static uint64_t expected_scope_hash;
static uint64_t expected_region_digest;
static uint64_t expected_max_bytes;

/* ---------------------------------------------------------------------------
 * Capability switch.
 *
 * Two translation units define this, exactly like the page-cache gate: the
 * default one answers 0 so the shipped binary cannot reach the privileged path,
 * and the production build script swaps in the enabled one. One definition only,
 * so a binary either can reach it or cannot, and nm can tell which.
 * ------------------------------------------------------------------------- */

static int trusted_owner_group_mode(const struct stat *info, uid_t owner, gid_t group,
                                    mode_t expected_mode, int exact_mode, int executable)
{
    if (!S_ISREG(info->st_mode) || info->st_uid != owner || info->st_gid != group) {
        return 0;
    }
    if ((info->st_mode & 0022) != 0) {
        return 0;
    }
    if (exact_mode && (info->st_mode & 07777) != expected_mode) {
        return 0;
    }
    if (executable && (info->st_mode & S_IXUSR) == 0) {
        return 0;
    }
    return 1;
}

static int trusted_directory_fd(int fd, uid_t owner, gid_t group,
                                int allow_user_owned_test_directories)
{
    struct stat info;
    if (fstat(fd, &info) != 0 || !S_ISDIR(info.st_mode)) {
        return 0;
    }
    if ((info.st_mode & 0022) == 0 && info.st_uid == 0 && info.st_gid == 0) {
        return 1;
    }
    if (allow_user_owned_test_directories && (info.st_mode & 0022) == 0 &&
        info.st_uid == owner && info.st_gid == group) {
        return 1;
    }
    /* /tmp is an allowed test ancestor only because the sticky bit prevents
     * users from removing each other's entries; production paths never use it. */
    return allow_user_owned_test_directories && info.st_uid == 0 && info.st_gid == 0 &&
           (info.st_mode & S_ISVTX) != 0 && (info.st_mode & 0002) != 0;
}

/* Open each path component with O_NOFOLLOW and verify every parent directory.
 * This avoids lstat/stat races and refuses symlinked helper or policy paths. */
static int open_trusted_file(const char *path, uid_t owner, gid_t group,
                             mode_t expected_mode, int exact_mode,
                             int require_executable, int allow_user_owned_test_directories)
{
    if (path == NULL || path[0] != '/') {
        return -1;
    }
    size_t length = strnlen(path, (size_t)PATH_MAX);
    if (length == 0u || length >= (size_t)PATH_MAX) {
        return -1;
    }
    char copy[PATH_MAX];
    memcpy(copy, path, length + 1u);

    int directory = open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (directory < 0 || !trusted_directory_fd(directory, owner, group,
                                                allow_user_owned_test_directories)) {
        if (directory >= 0) {
            (void)close(directory);
        }
        return -1;
    }

    char *component = copy + 1;
    for (;;) {
        if (*component == '\0' || strcmp(component, ".") == 0 ||
            strcmp(component, "..") == 0) {
            (void)close(directory);
            return -1;
        }
        char *slash = strchr(component, '/');
        int is_leaf = slash == NULL;
        if (slash != NULL) {
            *slash = '\0';
            if (*component == '\0') {
                (void)close(directory);
                return -1;
            }
        }
        if (!is_leaf) {
            int next = openat(directory, component,
                              O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
            if (next < 0 || !trusted_directory_fd(next, owner, group,
                                                   allow_user_owned_test_directories)) {
                if (next >= 0) {
                    (void)close(next);
                }
                (void)close(directory);
                return -1;
            }
            (void)close(directory);
            directory = next;
            component = slash + 1;
            continue;
        }

        int file = openat(directory, component,
                          O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
        (void)close(directory);
        if (file < 0) {
            return -1;
        }
        struct stat info;
        if (fstat(file, &info) != 0 ||
            !trusted_owner_group_mode(&info, owner, group, expected_mode,
                                      exact_mode, require_executable)) {
            (void)close(file);
            return -1;
        }
        return file;
    }
}

static size_t count_text(const char *text, const char *needle)
{
    size_t count = 0;
    size_t length = strlen(needle);
    const char *cursor = text;
    while ((cursor = strstr(cursor, needle)) != NULL) {
        count++;
        cursor += length;
    }
    return count;
}

static size_t count_text_range(const char *text, size_t text_length, const char *needle)
{
    size_t count = 0;
    size_t needle_length = strlen(needle);
    if (needle_length == 0u || needle_length > text_length) {
        return 0;
    }
    for (size_t index = 0; index <= text_length - needle_length; index++) {
        if (memcmp(text + index, needle, needle_length) == 0) {
            count++;
            index += needle_length - 1u;
        }
    }
    return count;
}

static int policy_fd_matches_action(int fd)
{
    char contents[MR_XPROC_POLICY_MAX_BYTES + 1u];
    size_t used = 0;
    for (;;) {
        if (used == MR_XPROC_POLICY_MAX_BYTES) {
            return 0;
        }
        ssize_t got = read(fd, contents + used, MR_XPROC_POLICY_MAX_BYTES - used);
        if (got < 0) {
            if (errno == EINTR) {
                continue;
            }
            return 0;
        }
        if (got == 0) {
            break;
        }
        used += (size_t)got;
    }
    if (memchr(contents, '\0', used) != NULL) {
        return 0;
    }
    contents[used] = '\0';

    char action[256];
    char exec_annotation[512];
    int action_length = snprintf(action, sizeof(action), "<action id=\"%s\">",
                                 MR_XPROC_ACTION_ID);
    int annotation_length = snprintf(exec_annotation, sizeof(exec_annotation),
                                     "<annotate key=\"%s\">%s</annotate>",
                                     MR_XPROC_EXEC_PATH_ANNOTATION,
                                     MR_XPROC_HELPER_PATH);
    if (action_length <= 0 || (size_t)action_length >= sizeof(action) ||
        annotation_length <= 0 || (size_t)annotation_length >= sizeof(exec_annotation) ||
        count_text(contents, action) != 1u) {
        return 0;
    }
    const char *action_start = strstr(contents, action);
    const char *action_end = action_start == NULL ? NULL : strstr(action_start, "</action>");
    if (action_end == NULL) {
        return 0;
    }
    size_t action_length_bytes = (size_t)(action_end - action_start);
    return count_text_range(action_start, action_length_bytes,
                            MR_XPROC_EXEC_PATH_ANNOTATION) == 1u &&
           count_text_range(action_start, action_length_bytes, exec_annotation) == 1u;
}

static const char *helper_path(void)
{
#ifdef MEMREDUCT_TESTING
    const char *override = getenv("MEMREDUCT_TEST_XPROC_HELPER");
    if (override != NULL && override[0] == '/') {
        return override;
    }
#endif
    return MR_XPROC_HELPER_PATH;
}

static const char *policy_path(void)
{
#ifdef MEMREDUCT_TESTING
    const char *override = getenv("MEMREDUCT_TEST_XPROC_POLICY");
    if (override != NULL && override[0] == '/') {
        return override;
    }
#endif
    return MR_XPROC_POLICY_PATH;
}

static const char *pkexec_path(void)
{
#ifdef MEMREDUCT_TESTING
    const char *override = getenv("MEMREDUCT_TEST_PKEXEC");
    if (override != NULL && override[0] == '/') {
        return override;
    }
#endif
    return MR_XPROC_PKEXEC;
}

/* The helper and GUI have a strict result protocol. A root-owned helper can
 * still be from an older install, so ownership and mode alone are not enough.
 * Scan its already-open trusted file instead of executing a capability-bearing
 * binary outside the Polkit path. */
static int helper_protocol_matches(int fd)
{
    struct stat info;
    if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size <= 0 ||
        (uint64_t)info.st_size > 64ull * 1024ull * 1024ull) {
        return 0;
    }
    static const unsigned char marker[] = MR_XPROC_PROTOCOL_MARKER;
    unsigned char buffer[4096u + sizeof(marker)];
    size_t carry = 0u;
    off_t offset = 0;
    while ((uint64_t)offset < (uint64_t)info.st_size) {
        ssize_t got = pread(fd, buffer + carry, 4096u, offset);
        if (got < 0) {
            if (errno == EINTR) {
                continue;
            }
            return 0;
        }
        if (got == 0) {
            return 0;
        }
        size_t used = carry + (size_t)got;
        for (size_t index = 0u; index + sizeof(marker) - 1u <= used; index++) {
            if (memcmp(buffer + index, marker, sizeof(marker) - 1u) == 0) {
                return 1;
            }
        }
        carry = used < sizeof(marker) - 1u ? used : sizeof(marker) - 1u;
        memmove(buffer, buffer + used - carry, carry);
        offset += got;
    }
    return 0;
}

int64_t mr_xproc_preflight(void)
{
    const char *path = helper_path();
    int helper = open_trusted_file(path, 0, 0, 0755, 1, 0, 0);
    if (helper < 0) {
        return MR_XPROC_PREFLIGHT_NEED_HELPER;
    }
    if (!helper_protocol_matches(helper)) {
        (void)close(helper);
        return MR_XPROC_PREFLIGHT_HELPER_PROTOCOL;
    }
    (void)close(helper);

    int policy = open_trusted_file(policy_path(), 0, 0, 0644, 1, 0, 0);
    if (policy < 0) {
        return MR_XPROC_PREFLIGHT_NEED_POLICY;
    }
    int policy_matches = policy_fd_matches_action(policy);
    (void)close(policy);
    if (!policy_matches) {
        return MR_XPROC_PREFLIGHT_NEED_POLICY;
    }

    int pkexec = open_trusted_file(pkexec_path(), 0, 0, 0, 0, 1, 0);
    if (pkexec < 0) {
        return MR_XPROC_PREFLIGHT_NEED_PKEXEC;
    }
    (void)close(pkexec);
    return MR_XPROC_PREFLIGHT_READY;
}

#ifdef MEMREDUCT_TESTING
int mr_xproc_test_policy_file_matches(const char *path, uid_t owner, gid_t group)
{
    int fd = open_trusted_file(path, owner, group, 0644, 1, 0, 1);
    if (fd < 0) {
        return 0;
    }
    int matches = policy_fd_matches_action(fd);
    (void)close(fd);
    return matches;
}

int mr_xproc_test_helper_protocol_matches(const char *path, uid_t owner, gid_t group)
{
    int fd = open_trusted_file(path, owner, group, 0755, 1, 1, 1);
    if (fd < 0) {
        return 0;
    }
    int matches = helper_protocol_matches(fd);
    (void)close(fd);
    return matches;
}
#endif

/* ---------------------------------------------------------------------------
 * Parsing the helper's result block.
 *
 * A result is accepted only when the marker line is present AND every required
 * key is present exactly once. A partial block is not a weaker success; it is
 * an unreadable answer, and it maps to OUTCOME_UNKNOWN.
 * ------------------------------------------------------------------------- */

struct parsed_result {
    int have_marker;
    int protocol_seen;
    int mode_seen;
    int status_seen;
    int reason_seen;
    int advised_seen;
    int requested_seen;
    int regions_seen;
    int error_seen;
    int calls_seen;
    int pid_seen;
    int verified_seen;
    int uid_seen;
    int request_id_seen;
    int expected_identity_hash_seen;
    int observed_identity_hash_seen;
    int identity_hash_verified_seen;
    int expected_scope_hash_seen;
    int observed_scope_hash_seen;
    int scope_hash_verified_seen;
    int expected_region_digest_seen;
    int observed_region_digest_seen;
    int region_digest_verified_seen;
    int target_rss_before_seen;
    int target_rss_after_seen;
    int target_swap_before_seen;
    int target_swap_after_seen;
    int scope_current_before_seen;
    int scope_current_after_seen;
    int scope_anon_before_seen;
    int scope_anon_after_seen;
    int scope_file_before_seen;
    int scope_file_after_seen;
    int scope_swap_before_seen;
    int scope_swap_after_seen;
    uint64_t protocol_version;
    const char *status_text;
    size_t status_len;
    const char *reason_text;
    size_t reason_len;
    const char *mode_text;
    size_t mode_len;
    int64_t mode;
    uint64_t advised;
    uint64_t requested;
    uint64_t regions;
    int64_t error_code;
    int64_t calls;
    int64_t pid;
    uint64_t start_time;
    int start_time_seen;
    int verified;
    int uid_known;
    int64_t uid;
    uint64_t request_id;
    uint64_t expected_identity_hash;
    uint64_t observed_identity_hash;
    int identity_hash_verified;
    uint64_t expected_scope_hash;
    uint64_t observed_scope_hash;
    int scope_hash_verified;
    uint64_t expected_region_digest;
    uint64_t observed_region_digest;
    int region_digest_verified;
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

static int key_is(const char *line, size_t line_len, const char *key, const char **value,
                  size_t *value_len)
{
    size_t key_len = strlen(key);
    if (line_len < key_len + 1u) {
        return 0;
    }
    if (memcmp(line, key, key_len) != 0 || line[key_len] != '=') {
        return 0;
    }
    *value = line + key_len + 1u;
    *value_len = line_len - key_len - 1u;
    return 1;
}

static int parse_u64_text(const char *text, size_t length, uint64_t *out)
{
    if (length == 0) {
        return -1;
    }
    uint64_t value = 0;
    for (size_t index = 0; index < length; index++) {
        char character = text[index];
        if (character < '0' || character > '9') {
            return -1;
        }
        uint64_t next = value * 10u + (uint64_t)(character - '0');
        if (next < value) {
            return -1;
        }
        value = next;
    }
    *out = value;
    return 0;
}

static int parse_i64_text(const char *text, size_t length, int64_t *out)
{
    int negative = 0;
    if (length > 0 && text[0] == '-') {
        negative = 1;
        text++;
        length--;
    }
    uint64_t magnitude = 0;
    if (parse_u64_text(text, length, &magnitude) != 0) {
        return -1;
    }
    if (magnitude > (uint64_t)INT64_MAX) {
        return -1;
    }
    *out = negative ? -(int64_t)magnitude : (int64_t)magnitude;
    return 0;
}

static int parse_measurement_text(const char *text, size_t length, int64_t *out)
{
    if (length == sizeof("unavailable") - 1u &&
        memcmp(text, "unavailable", length) == 0) {
        *out = -1;
        return 0;
    }
    return parse_i64_text(text, length, out);
}

static int parse_result(const char *text, size_t length, struct parsed_result *out)
{
    memset(out, 0, sizeof(*out));
    out->mode = -1;
    out->target_rss_before_kib = INT64_MIN;
    out->target_rss_after_kib = INT64_MIN;
    out->target_swap_before_kib = INT64_MIN;
    out->target_swap_after_kib = INT64_MIN;
    out->scope_current_before_bytes = INT64_MIN;
    out->scope_current_after_bytes = INT64_MIN;
    out->scope_anon_before_bytes = INT64_MIN;
    out->scope_anon_after_bytes = INT64_MIN;
    out->scope_file_before_bytes = INT64_MIN;
    out->scope_file_after_bytes = INT64_MIN;
    out->scope_swap_before_bytes = INT64_MIN;
    out->scope_swap_after_bytes = INT64_MIN;

    const char *cursor = text;
    const char *end = text + length;
    int marker_seen = 0;

    while (cursor < end) {
        const char *newline = memchr(cursor, '\n', (size_t)(end - cursor));
        const char *line_end = newline == NULL ? end : newline;
        size_t line_len = (size_t)(line_end - cursor);

        if (line_len > 0) {
            /* The marker must come first: anything before it is noise, and a
             * result that appears after foreign text is not trustworthy. */
            if (!marker_seen) {
                if (line_len == strlen("MR_CROSSPROC_RESULT") &&
                    memcmp(cursor, "MR_CROSSPROC_RESULT", line_len) == 0) {
                    marker_seen = 1;
                    out->have_marker = 1;
                }
            } else {
                const char *value = NULL;
                size_t value_len = 0;
                if (key_is(cursor, line_len, "protocol_version", &value, &value_len)) {
                    out->protocol_seen++;
                    (void)parse_u64_text(value, value_len, &out->protocol_version);
                } else if (key_is(cursor, line_len, "mode", &value, &value_len)) {
                    out->mode_seen++;
                    out->mode_text = value;
                    out->mode_len = value_len;
                    if (value_len == strlen(MR_XPROC_MODE_PAGEOUT_TEXT) &&
                        memcmp(value, MR_XPROC_MODE_PAGEOUT_TEXT, value_len) == 0) out->mode = 0;
                    else if (value_len == strlen(MR_XPROC_MODE_APP_SCOPE_TEXT) &&
                             memcmp(value, MR_XPROC_MODE_APP_SCOPE_TEXT, value_len) == 0) out->mode = 1;
                    else out->mode = -1;
                } else if (key_is(cursor, line_len, "status", &value, &value_len)) {
                    out->status_seen++;
                    out->status_text = value;
                    out->status_len = value_len;
                } else if (key_is(cursor, line_len, "reason", &value, &value_len)) {
                    out->reason_seen++;
                    out->reason_text = value;
                    out->reason_len = value_len;
                } else if (key_is(cursor, line_len, "advised_bytes", &value, &value_len)) {
                    out->advised_seen++;
                    (void)parse_u64_text(value, value_len, &out->advised);
                } else if (key_is(cursor, line_len, "requested_bytes", &value, &value_len)) {
                    out->requested_seen++;
                    (void)parse_u64_text(value, value_len, &out->requested);
                } else if (key_is(cursor, line_len, "regions", &value, &value_len)) {
                    out->regions_seen++;
                    (void)parse_u64_text(value, value_len, &out->regions);
                } else if (key_is(cursor, line_len, "error_code", &value, &value_len)) {
                    out->error_seen++;
                    (void)parse_i64_text(value, value_len, &out->error_code);
                } else if (key_is(cursor, line_len, "call_count", &value, &value_len)) {
                    out->calls_seen++;
                    (void)parse_i64_text(value, value_len, &out->calls);
                } else if (key_is(cursor, line_len, "pid", &value, &value_len)) {
                    out->pid_seen++;
                    (void)parse_i64_text(value, value_len, &out->pid);
                } else if (key_is(cursor, line_len, "start_time", &value, &value_len)) {
                    out->start_time_seen++;
                    (void)parse_u64_text(value, value_len, &out->start_time);
                } else if (key_is(cursor, line_len, "start_time_verified", &value, &value_len)) {
                    out->verified_seen++;
                    out->verified = (value_len == 4 && memcmp(value, "true", 4) == 0);
                } else if (key_is(cursor, line_len, "uid", &value, &value_len)) {
                    out->uid_seen++;
                    if (value_len == 7 && memcmp(value, "unknown", 7) == 0) {
                        out->uid_known = 0;
                    } else if (parse_i64_text(value, value_len, &out->uid) == 0) {
                        out->uid_known = 1;
                    }
                } else if (key_is(cursor, line_len, "request_id", &value, &value_len)) {
                    out->request_id_seen++;
                    (void)parse_u64_text(value, value_len, &out->request_id);
                } else if (key_is(cursor, line_len, "expected_identity_hash", &value, &value_len)) {
                    out->expected_identity_hash_seen++;
                    (void)parse_u64_text(value, value_len, &out->expected_identity_hash);
                } else if (key_is(cursor, line_len, "observed_identity_hash", &value, &value_len)) {
                    out->observed_identity_hash_seen++;
                    (void)parse_u64_text(value, value_len, &out->observed_identity_hash);
                } else if (key_is(cursor, line_len, "identity_hash_verified", &value, &value_len)) {
                    out->identity_hash_verified_seen++;
                    out->identity_hash_verified = value_len == 4u &&
                        memcmp(value, "true", 4u) == 0;
                } else if (key_is(cursor, line_len, "expected_scope_hash", &value, &value_len)) {
                    out->expected_scope_hash_seen++;
                    (void)parse_u64_text(value, value_len, &out->expected_scope_hash);
                } else if (key_is(cursor, line_len, "observed_scope_hash", &value, &value_len)) {
                    out->observed_scope_hash_seen++;
                    (void)parse_u64_text(value, value_len, &out->observed_scope_hash);
                } else if (key_is(cursor, line_len, "scope_hash_verified", &value, &value_len)) {
                    out->scope_hash_verified_seen++;
                    out->scope_hash_verified = value_len == 4u &&
                        memcmp(value, "true", 4u) == 0;
                } else if (key_is(cursor, line_len, "expected_region_digest", &value, &value_len)) {
                    out->expected_region_digest_seen++;
                    (void)parse_u64_text(value, value_len, &out->expected_region_digest);
                } else if (key_is(cursor, line_len, "observed_region_digest", &value, &value_len)) {
                    out->observed_region_digest_seen++;
                    (void)parse_u64_text(value, value_len, &out->observed_region_digest);
                } else if (key_is(cursor, line_len, "region_digest_verified", &value, &value_len)) {
                    out->region_digest_verified_seen++;
                    out->region_digest_verified = value_len == 4u &&
                        memcmp(value, "true", 4u) == 0;
                } else if (key_is(cursor, line_len, "target_rss_before_kib", &value, &value_len)) {
                    out->target_rss_before_seen++;
                    (void)parse_measurement_text(value, value_len, &out->target_rss_before_kib);
                } else if (key_is(cursor, line_len, "target_rss_after_kib", &value, &value_len)) {
                    out->target_rss_after_seen++;
                    (void)parse_measurement_text(value, value_len, &out->target_rss_after_kib);
                } else if (key_is(cursor, line_len, "target_swap_before_kib", &value, &value_len)) {
                    out->target_swap_before_seen++;
                    (void)parse_measurement_text(value, value_len, &out->target_swap_before_kib);
                } else if (key_is(cursor, line_len, "target_swap_after_kib", &value, &value_len)) {
                    out->target_swap_after_seen++;
                    (void)parse_measurement_text(value, value_len, &out->target_swap_after_kib);
                } else if (key_is(cursor, line_len, "scope_current_before_bytes", &value, &value_len)) {
                    out->scope_current_before_seen++;
                    (void)parse_measurement_text(value, value_len, &out->scope_current_before_bytes);
                } else if (key_is(cursor, line_len, "scope_current_after_bytes", &value, &value_len)) {
                    out->scope_current_after_seen++;
                    (void)parse_measurement_text(value, value_len, &out->scope_current_after_bytes);
                } else if (key_is(cursor, line_len, "scope_anon_before_bytes", &value, &value_len)) {
                    out->scope_anon_before_seen++;
                    (void)parse_measurement_text(value, value_len, &out->scope_anon_before_bytes);
                } else if (key_is(cursor, line_len, "scope_anon_after_bytes", &value, &value_len)) {
                    out->scope_anon_after_seen++;
                    (void)parse_measurement_text(value, value_len, &out->scope_anon_after_bytes);
                } else if (key_is(cursor, line_len, "scope_file_before_bytes", &value, &value_len)) {
                    out->scope_file_before_seen++;
                    (void)parse_measurement_text(value, value_len, &out->scope_file_before_bytes);
                } else if (key_is(cursor, line_len, "scope_file_after_bytes", &value, &value_len)) {
                    out->scope_file_after_seen++;
                    (void)parse_measurement_text(value, value_len, &out->scope_file_after_bytes);
                } else if (key_is(cursor, line_len, "scope_swap_before_bytes", &value, &value_len)) {
                    out->scope_swap_before_seen++;
                    (void)parse_measurement_text(value, value_len, &out->scope_swap_before_bytes);
                } else if (key_is(cursor, line_len, "scope_swap_after_bytes", &value, &value_len)) {
                    out->scope_swap_after_seen++;
                    (void)parse_measurement_text(value, value_len, &out->scope_swap_after_bytes);
                } else if (key_is(cursor, line_len, "bytes_advised_is_not_bytes_freed",
                                   &value, &value_len)) {
                    /* Present for readers of the raw stream; the bridge does not
                     * need it, and its absence is not an error because no
                     * decision may depend on a disclaimer being printed. */
                }
            }
        }
        cursor = newline == NULL ? end : newline + 1;
    }
    return out->have_marker ? 0 : -1;
}

/* Every required field must appear exactly once. A duplicate is as unusable as
 * a missing one: it means two answers, and picking either would be a guess. */
static int result_is_complete(const struct parsed_result *parsed)
{
    return parsed->protocol_seen == 1 &&
           parsed->protocol_version == MR_XPROC_PROTOCOL_VERSION &&
           parsed->mode_seen == 1 && parsed->mode >= 0 &&
           parsed->request_id_seen == 1 &&
           parsed->expected_identity_hash_seen == 1 &&
           parsed->observed_identity_hash_seen == 1 &&
           parsed->identity_hash_verified_seen == 1 &&
           parsed->expected_scope_hash_seen == 1 &&
           parsed->observed_scope_hash_seen == 1 &&
           parsed->scope_hash_verified_seen == 1 &&
           parsed->expected_region_digest_seen == 1 &&
           parsed->observed_region_digest_seen == 1 &&
           parsed->region_digest_verified_seen == 1 &&
           parsed->status_seen == 1 && parsed->reason_seen == 1 &&
           parsed->advised_seen == 1 && parsed->requested_seen == 1 &&
           parsed->regions_seen == 1 && parsed->error_seen == 1 &&
           parsed->calls_seen == 1 && parsed->pid_seen == 1 &&
           parsed->start_time_seen == 1 && parsed->verified_seen == 1 &&
           parsed->uid_seen == 1 && parsed->start_time > 0 &&
           parsed->target_rss_before_seen == 1 && parsed->target_rss_after_seen == 1 &&
           parsed->target_swap_before_seen == 1 && parsed->target_swap_after_seen == 1 &&
           parsed->scope_current_before_seen == 1 && parsed->scope_current_after_seen == 1 &&
           parsed->scope_anon_before_seen == 1 && parsed->scope_anon_after_seen == 1 &&
           parsed->scope_file_before_seen == 1 && parsed->scope_file_after_seen == 1 &&
           parsed->scope_swap_before_seen == 1 && parsed->scope_swap_after_seen == 1 &&
           parsed->target_rss_before_kib != INT64_MIN &&
           parsed->target_rss_after_kib != INT64_MIN &&
           parsed->target_swap_before_kib != INT64_MIN &&
           parsed->target_swap_after_kib != INT64_MIN &&
           parsed->scope_current_before_bytes != INT64_MIN &&
           parsed->scope_current_after_bytes != INT64_MIN &&
           parsed->scope_anon_before_bytes != INT64_MIN &&
           parsed->scope_anon_after_bytes != INT64_MIN &&
           parsed->scope_file_before_bytes != INT64_MIN &&
           parsed->scope_file_after_bytes != INT64_MIN &&
           parsed->scope_swap_before_bytes != INT64_MIN &&
           parsed->scope_swap_after_bytes != INT64_MIN &&
           parsed->advised <= (uint64_t)INT64_MAX &&
           parsed->requested <= (uint64_t)INT64_MAX &&
           parsed->requested <= MR_XPROC_MAX_BYTES;
}

static int text_equals(const char *text, size_t length, const char *literal)
{
    size_t literal_len = strlen(literal);
    return length == literal_len && memcmp(text, literal, literal_len) == 0;
}

/* Map the helper's status token to a poll value. Unknown tokens become
 * OUTCOME_UNKNOWN: an unrecognised word is not a success. */
static int64_t classify_token(const struct parsed_result *parsed)
{
    const char *text = parsed->status_text;
    size_t length = parsed->status_len;
    if (text_equals(text, length, "accepted")) return MR_XPROC_POLL_ACCEPTED;
    if (text_equals(text, length, "partial")) return MR_XPROC_POLL_PARTIAL;
    if (text_equals(text, length, "refused")) return MR_XPROC_POLL_REFUSED;
    if (text_equals(text, length, "no_permission")) return MR_XPROC_POLL_NO_PERMISSION;
    if (text_equals(text, length, "target_gone")) return MR_XPROC_POLL_TARGET_GONE;
    if (text_equals(text, length, "bad_request")) return MR_XPROC_POLL_REFUSED;
    if (text_equals(text, length, "unsupported")) return MR_XPROC_POLL_UNSUPPORTED;
    if (text_equals(text, length, "no_regions")) return MR_XPROC_POLL_NO_REGIONS;
    if (text_equals(text, length, "scope_unavailable")) return MR_XPROC_POLL_REFUSED;
    return MR_XPROC_POLL_OUTCOME_UNKNOWN;
}

/* The single place where an exit code and a parsed block become a verdict.
 * Exposed to tests through mr_xproc_test_classify so every branch is asserted
 * against the production code rather than a copy of it. */
static void reset_last_result(void)
{
    last_status = 0;
    last_advised_bytes = 0;
    last_requested_bytes = 0;
    last_regions = 0;
    last_error_code = 0;
    last_call_count = 0;
    last_pid = 0;
    last_start_time = 0;
    last_start_time_verified = 0;
    last_uid = -1;
    last_request_id = 0u;
    last_expected_identity_hash = 0u;
    last_observed_identity_hash = 0u;
    last_identity_hash_verified = 0;
    last_expected_scope_hash = 0u;
    last_observed_scope_hash = 0u;
    last_scope_hash_verified = 0;
    last_expected_region_digest = 0u;
    last_observed_region_digest = 0u;
    last_region_digest_verified = 0;
    last_exit_code = -1;
    last_signal = 0;
    last_mode = -1;
    last_target_rss_before_kib = -1;
    last_target_rss_after_kib = -1;
    last_target_swap_before_kib = -1;
    last_target_swap_after_kib = -1;
    last_scope_current_before_bytes = -1;
    last_scope_current_after_bytes = -1;
    last_scope_anon_before_bytes = -1;
    last_scope_anon_after_bytes = -1;
    last_scope_file_before_bytes = -1;
    last_scope_file_after_bytes = -1;
    last_scope_swap_before_bytes = -1;
    last_scope_swap_after_bytes = -1;
    last_reason[0] = '\0';
}

static int64_t classify(const char *text, size_t length, int exit_code)
{
    reset_last_result();
    last_exit_code = exit_code;

    struct parsed_result parsed;
    int parsed_ok = parse_result(text, length, &parsed) == 0 && result_is_complete(&parsed);

    if (parsed_ok) {
        last_advised_bytes = (int64_t)parsed.advised;
        last_requested_bytes = (int64_t)parsed.requested;
        last_regions = (int64_t)parsed.regions;
        last_error_code = parsed.error_code;
        last_call_count = parsed.calls;
        last_pid = parsed.pid;
        last_start_time = (int64_t)parsed.start_time;
        last_start_time_verified = parsed.verified;
        last_uid = parsed.uid_known ? parsed.uid : -1;
        last_request_id = parsed.request_id;
        last_expected_identity_hash = parsed.expected_identity_hash;
        last_observed_identity_hash = parsed.observed_identity_hash;
        last_identity_hash_verified = parsed.identity_hash_verified;
        last_expected_scope_hash = parsed.expected_scope_hash;
        last_observed_scope_hash = parsed.observed_scope_hash;
        last_scope_hash_verified = parsed.scope_hash_verified;
        last_expected_region_digest = parsed.expected_region_digest;
        last_observed_region_digest = parsed.observed_region_digest;
        last_region_digest_verified = parsed.region_digest_verified;
        last_mode = parsed.mode;
        last_target_rss_before_kib = parsed.target_rss_before_kib;
        last_target_rss_after_kib = parsed.target_rss_after_kib;
        last_target_swap_before_kib = parsed.target_swap_before_kib;
        last_target_swap_after_kib = parsed.target_swap_after_kib;
        last_scope_current_before_bytes = parsed.scope_current_before_bytes;
        last_scope_current_after_bytes = parsed.scope_current_after_bytes;
        last_scope_anon_before_bytes = parsed.scope_anon_before_bytes;
        last_scope_anon_after_bytes = parsed.scope_anon_after_bytes;
        last_scope_file_before_bytes = parsed.scope_file_before_bytes;
        last_scope_file_after_bytes = parsed.scope_file_after_bytes;
        last_scope_swap_before_bytes = parsed.scope_swap_before_bytes;
        last_scope_swap_after_bytes = parsed.scope_swap_after_bytes;
        size_t copy = parsed.reason_len < sizeof(last_reason) - 1u
                          ? parsed.reason_len
                          : sizeof(last_reason) - 1u;
        memcpy(last_reason, parsed.reason_text, copy);
        last_reason[copy] = '\0';
    }

    /* pkexec uses 126 when authorization is dismissed and 127 when it cannot
     * authorize or start the fixed command. The helper's documented exit set is
     * 0..4 or 77, so these codes without a result block identify the polkit /
     * exec boundary. If a result block accompanies either code, the two
     * channels contradict each other and the outcome is unknown. */
    if (exit_code == 126) {
        return parsed_ok ? MR_XPROC_POLL_OUTCOME_UNKNOWN : MR_XPROC_POLL_AUTH_CANCELLED;
    }
    if (exit_code == 127) {
        return parsed_ok ? MR_XPROC_POLL_OUTCOME_UNKNOWN : MR_XPROC_POLL_AUTH_FAILED;
    }

    if (!parsed_ok) {
        /* No usable block. A zero exit would otherwise look like success. */
        return MR_XPROC_POLL_OUTCOME_UNKNOWN;
    }

    int64_t verdict = classify_token(&parsed);

    /* Cross-check the token against the exit code and the byte counts. These
     * two sources disagreeing is itself a finding, and it must not resolve in
     * favour of the friendlier value. */
    int exit_agrees =
        (verdict == MR_XPROC_POLL_ACCEPTED && exit_code == MR_XPROC_EXIT_OK) ||
        (verdict == MR_XPROC_POLL_PARTIAL && exit_code == MR_XPROC_EXIT_OK) ||
        (verdict == MR_XPROC_POLL_UNSUPPORTED && exit_code == MR_XPROC_EXIT_UNSUPPORTED) ||
        (verdict != MR_XPROC_POLL_ACCEPTED && verdict != MR_XPROC_POLL_PARTIAL &&
         verdict != MR_XPROC_POLL_UNSUPPORTED && exit_code == MR_XPROC_EXIT_REFUSED);
    if (!exit_agrees) {
        return MR_XPROC_POLL_OUTCOME_UNKNOWN;
    }

    if (verdict == MR_XPROC_POLL_ACCEPTED && parsed.mode == MR_XPROC_MODE_PROCESS_PAGEOUT &&
        (parsed.advised == 0 || parsed.advised != parsed.requested)) {
        /* PAGEOUT acceptance means every requested byte was covered. */
        return MR_XPROC_POLL_OUTCOME_UNKNOWN;
    }
    if ((verdict == MR_XPROC_POLL_ACCEPTED || verdict == MR_XPROC_POLL_PARTIAL) &&
        parsed.mode == MR_XPROC_MODE_PROCESS_PAGEOUT && parsed.advised > parsed.requested) {
        return MR_XPROC_POLL_OUTCOME_UNKNOWN;
    }
    if ((parsed.mode == MR_XPROC_MODE_APP_SCOPE && parsed.advised != 0) ||
        (parsed.mode == MR_XPROC_MODE_APP_SCOPE &&
         (verdict == MR_XPROC_POLL_ACCEPTED || verdict == MR_XPROC_POLL_PARTIAL) &&
         (parsed.requested == 0 || parsed.calls != 1))) {
        return MR_XPROC_POLL_OUTCOME_UNKNOWN;
    }
    if (parsed.calls != 1 && verdict != MR_XPROC_POLL_REFUSED) {
        /* This product issues exactly one call per action. Any other number
         * means the report does not describe what was authorized. */
        return MR_XPROC_POLL_OUTCOME_UNKNOWN;
    }
    return verdict;
}

/* ---------------------------------------------------------------------------
 * Spawning and polling.
 * ------------------------------------------------------------------------- */

int64_t mr_xproc_enabled(void)
{
    /* Defined in exactly one gate translation unit, like the page-cache bridge. */
    extern int64_t mr_xproc_capability_enabled(void);
    return mr_xproc_capability_enabled();
}

/* The reason a start was refused locally, as a stable token the GUI can show
 * without inventing an explanation. */
const char *mr_xproc_local_refusal_name(void)
{
    switch (local_refusal) {
        case MR_XPROC_LOCAL_BAD_PID: return "bad_pid";
        case MR_XPROC_LOCAL_BAD_START_TIME: return "bad_start_time";
        case MR_XPROC_LOCAL_BAD_BUDGET: return "bad_budget";
        case MR_XPROC_LOCAL_NOT_INSTALLED: return "not_installed";
        case MR_XPROC_LOCAL_NOT_ENABLED: return "not_enabled";
        case MR_XPROC_LOCAL_BAD_IDENTITY: return "bad_identity";
        default: return "none";
    }
}

static uint64_t last_generated_request_id;

int64_t mr_xproc_request_id_new(void)
{
    uint64_t candidate = 0u;
    ssize_t received;
    do {
        received = getrandom(&candidate, sizeof(candidate), GRND_NONBLOCK);
    } while (received < 0 && errno == EINTR);

    if (received != (ssize_t)sizeof(candidate)) {
        struct timespec now;
        if (clock_gettime(CLOCK_BOOTTIME, &now) != 0 || now.tv_sec < 0) {
            return 0;
        }
        candidate = (uint64_t)now.tv_sec * 1000000000ull + (uint64_t)now.tv_nsec;
        candidate ^= (uint64_t)(unsigned long)getpid() << 21;
    }
    candidate &= 0x7fffffffffffffffull;
    if (candidate == 0u) {
        candidate = 1u;
    }
    if (candidate <= last_generated_request_id) {
        if (last_generated_request_id >= (uint64_t)INT64_MAX) {
            return 0;
        }
        candidate = last_generated_request_id + 1u;
    }
    last_generated_request_id = candidate;
    return (int64_t)candidate;
}

int64_t mr_xproc_audit_confirmation(int64_t request_id, int64_t pid,
                                    int64_t start_time, int64_t uid,
                                    int64_t scope_hash, int64_t identity_hash,
                                    int64_t region_digest)
{
    if (request_id <= 0 || pid < 2 || start_time <= 0 || uid <= 0 ||
        (uint64_t)uid > UINT32_MAX || scope_hash <= 0 || identity_hash <= 0 ||
        region_digest <= 0) {
        return 1;
    }
    mr_xproc_audit_emit("confirmation", (uint64_t)request_id, (int32_t)pid,
                        (uint64_t)start_time, (uint64_t)uid,
                        (uint64_t)scope_hash, (uint64_t)identity_hash,
                        (uint64_t)region_digest);
    return 0;
}

static void close_child_stdout(void)
{
    if (child_stdout >= 0) {
        (void)close(child_stdout);
        child_stdout = -1;
    }
}

/* Drain whatever is available without blocking. Called from poll, so the pipe
 * never fills and stalls the helper while the GUI is busy drawing. */
static void drain_output(void)
{
    if (child_stdout < 0) {
        return;
    }
    for (;;) {
        if (output_used + 1u >= sizeof(output_buffer)) {
            output_overflowed = 1;
            return;
        }
        ssize_t got = read(child_stdout, output_buffer + output_used,
                           sizeof(output_buffer) - output_used - 1u);
        if (got > 0) {
            output_used += (size_t)got;
            continue;
        }
        if (got == 0) {
            /* EOF: the child closed its end. Keep the descriptor until reap so
             * the bookkeeping stays in one place. */
            return;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        }
        return;
    }
}

int64_t mr_xproc_start(int64_t request_id, int64_t mode, int64_t pid, int64_t start_time,
                       int64_t uid, int64_t identity_hash,
                       int64_t scope_hash, int64_t region_digest,
                       int64_t reviewed_bytes, int64_t reviewed_regions,
                       int64_t max_bytes)
{
    /* Cleared on entry but set on every refusal path below: a caller that reads
     * mr_xproc_local_refusal() after a refusal must never see "none" (0), which
     * is also the value that means "no refusal". Found by probing the bridge:
     * a start request refused for an unbuilt gate reported reason 0. */
    local_refusal = MR_XPROC_LOCAL_NONE;

    if (child_pid > 0) {
        return MR_XPROC_START_BUSY;
    }
    expected_pid = 0;
    expected_start_time = 0;
    expected_uid = 0;
    expected_mode = -1;
    expected_request_id = 0u;
    expected_identity_hash = 0u;
    expected_scope_hash = 0u;
    expected_region_digest = 0u;
    expected_max_bytes = 0u;
    reset_last_result();
    if (request_id <= 0) {
        local_refusal = MR_XPROC_LOCAL_BAD_IDENTITY;
        return MR_XPROC_START_BAD_REQUEST;
    }
    if (mode != MR_XPROC_MODE_PROCESS_PAGEOUT && mode != MR_XPROC_MODE_APP_SCOPE) {
        local_refusal = MR_XPROC_LOCAL_BAD_IDENTITY;
        return MR_XPROC_START_BAD_REQUEST;
    }
    if (pid < 2 || pid > 0x7fffffff) {
        local_refusal = MR_XPROC_LOCAL_BAD_PID;
        return MR_XPROC_START_BAD_REQUEST;
    }
    if (start_time <= 0) {
        local_refusal = MR_XPROC_LOCAL_BAD_START_TIME;
        return MR_XPROC_START_BAD_REQUEST;
    }
    if (uid <= 0 || (uint64_t)uid > UINT32_MAX || identity_hash <= 0 ||
        scope_hash <= 0 || region_digest <= 0) {
        local_refusal = MR_XPROC_LOCAL_BAD_IDENTITY;
        return MR_XPROC_START_BAD_REQUEST;
    }
    if (reviewed_bytes <= 0 || reviewed_regions <= 0 ||
        (uint64_t)reviewed_regions > UINT32_MAX) {
        local_refusal = MR_XPROC_LOCAL_BAD_IDENTITY;
        return MR_XPROC_START_BAD_REQUEST;
    }
    if (max_bytes <= 0 || (uint64_t)max_bytes > MR_XPROC_MAX_BYTES) {
        local_refusal = MR_XPROC_LOCAL_BAD_BUDGET;
        return MR_XPROC_START_BAD_REQUEST;
    }
    if (!mr_xproc_enabled()) {
        local_refusal = MR_XPROC_LOCAL_NOT_ENABLED;
        return MR_XPROC_START_BAD_REQUEST;
    }
    if (mr_xproc_preflight() != MR_XPROC_PREFLIGHT_READY) {
        local_refusal = MR_XPROC_LOCAL_NOT_INSTALLED;
        return MR_XPROC_START_BAD_REQUEST;
    }

    char request_text[24];
    const char *mode_text = mode == MR_XPROC_MODE_APP_SCOPE
                                ? MR_XPROC_MODE_APP_SCOPE_TEXT : MR_XPROC_MODE_PAGEOUT_TEXT;
    char pid_text[16];
    char start_text[24];
    char uid_text[16];
    char identity_text[24];
    char scope_text[24];
    char digest_text[24];
    char reviewed_bytes_text[24];
    char reviewed_regions_text[16];
    char bytes_text[24];
    (void)snprintf(request_text, sizeof(request_text), "%lld", (long long)request_id);
    (void)snprintf(pid_text, sizeof(pid_text), "%lld", (long long)pid);
    (void)snprintf(start_text, sizeof(start_text), "%lld", (long long)start_time);
    (void)snprintf(uid_text, sizeof(uid_text), "%lld", (long long)uid);
    (void)snprintf(identity_text, sizeof(identity_text), "%lld", (long long)identity_hash);
    (void)snprintf(scope_text, sizeof(scope_text), "%lld", (long long)scope_hash);
    (void)snprintf(digest_text, sizeof(digest_text), "%lld", (long long)region_digest);
    (void)snprintf(reviewed_bytes_text, sizeof(reviewed_bytes_text), "%lld", (long long)reviewed_bytes);
    (void)snprintf(reviewed_regions_text, sizeof(reviewed_regions_text), "%lld", (long long)reviewed_regions);
    (void)snprintf(bytes_text, sizeof(bytes_text), "%lld", (long long)max_bytes);

    int pipe_fds[2];
    if (pipe2(pipe_fds, O_CLOEXEC) != 0) {
        return MR_XPROC_START_LAUNCH_FAILED;
    }

    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0) {
        (void)close(pipe_fds[0]);
        (void)close(pipe_fds[1]);
        return MR_XPROC_START_LAUNCH_FAILED;
    }
    /* The child writes into the pipe; its stderr is left alone so pkexec's own
     * message reaches the user's terminal-equivalent and is never parsed. */
    int actions_ok =
        posix_spawn_file_actions_adddup2(&actions, pipe_fds[1], STDOUT_FILENO) == 0;

    pid_t spawned = -1;
    int spawn_status = -1;
    if (actions_ok) {
        mr_xproc_audit_emit("bridge_argv", (uint64_t)request_id,
                            (int32_t)pid, (uint64_t)start_time,
                            (uint64_t)uid, (uint64_t)scope_hash,
                            (uint64_t)identity_hash, (uint64_t)region_digest);
        /* A literal argv. No shell, no string building from input, no path from
         * anything but the fixed install location. */
        char *const argv[] = {
            (char *)pkexec_path(),
            (char *)helper_path(),
            (char *)MR_XPROC_VERB,
            (char *)mode_text,
            request_text,
            pid_text,
            start_text,
            uid_text,
            identity_text,
            scope_text,
            digest_text,
            reviewed_bytes_text,
            reviewed_regions_text,
            bytes_text,
            NULL
        };
        spawn_status = posix_spawn(&spawned, pkexec_path(), &actions, NULL, argv, environ);
    }
    (void)posix_spawn_file_actions_destroy(&actions);
    (void)close(pipe_fds[1]);

    if (!actions_ok || spawn_status != 0) {
        (void)close(pipe_fds[0]);
        return MR_XPROC_START_LAUNCH_FAILED;
    }

    /* Keep the read end non-blocking so polling never stalls the GUI. */
    int flags = fcntl(pipe_fds[0], F_GETFL, 0);
    if (flags >= 0) {
        (void)fcntl(pipe_fds[0], F_SETFL, flags | O_NONBLOCK);
    }

    child_pid = spawned;
    expected_pid = pid;
    expected_start_time = start_time;
    expected_uid = uid;
    expected_mode = mode;
    expected_request_id = (uint64_t)request_id;
    expected_identity_hash = (uint64_t)identity_hash;
    expected_scope_hash = (uint64_t)scope_hash;
    expected_region_digest = (uint64_t)region_digest;
    expected_max_bytes = (uint64_t)max_bytes;
    child_stdout = pipe_fds[0];
    output_used = 0;
    output_overflowed = 0;
    return MR_XPROC_START_OK;
}

static int result_matches_expected_target(int64_t verdict)
{
    if (expected_request_id == 0u || last_reason[0] == '\0') {
        return 1;
    }
    if (last_request_id != expected_request_id ||
        last_expected_identity_hash != expected_identity_hash ||
        last_expected_scope_hash != expected_scope_hash ||
        last_expected_region_digest != expected_region_digest ||
        last_pid != expected_pid || last_start_time != expected_start_time ||
        last_mode != expected_mode ||
        (last_uid >= 0 && last_uid != expected_uid)) {
        return 0;
    }
    if (verdict == MR_XPROC_POLL_ACCEPTED || verdict == MR_XPROC_POLL_PARTIAL) {
        return last_requested_bytes > 0 &&
               (uint64_t)last_requested_bytes <= expected_max_bytes &&
               (last_mode != MR_XPROC_MODE_PROCESS_PAGEOUT ||
                last_advised_bytes <= last_requested_bytes) &&
               last_start_time_verified == 1 && last_uid == expected_uid &&
               last_identity_hash_verified == 1 && last_scope_hash_verified == 1 &&
               last_region_digest_verified == 1 &&
               last_observed_identity_hash == expected_identity_hash &&
               last_observed_scope_hash == expected_scope_hash &&
               last_observed_region_digest == expected_region_digest;
    }
    return 1;
}

int64_t mr_xproc_poll(void)
{
    if (child_pid < 0) {
        return MR_XPROC_POLL_IDLE;
    }
    drain_output();

    int status = 0;
    pid_t result = waitpid(child_pid, &status, WNOHANG);
    if (result == 0 || (result < 0 && errno == EINTR)) {
        return MR_XPROC_POLL_RUNNING;
    }

    /* The child is done; take the rest of its output before deciding. */
    drain_output();
    close_child_stdout();
    child_pid = -1;

    if (result < 0) {
        reset_last_result();
        return MR_XPROC_POLL_OUTCOME_UNKNOWN;
    }
    if (WIFSIGNALED(status)) {
        /* Killed by a signal: we cannot know how far it got. */
        reset_last_result();
        last_signal = WTERMSIG(status);
        return MR_XPROC_POLL_OUTCOME_UNKNOWN;
    }
    if (!WIFEXITED(status)) {
        reset_last_result();
        return MR_XPROC_POLL_OUTCOME_UNKNOWN;
    }
    last_exit_code = WEXITSTATUS(status);
    last_signal = 0;
    if (output_overflowed) {
        /* A result we could not read in full is not a result. */
        return MR_XPROC_POLL_OUTCOME_UNKNOWN;
    }

    int64_t verdict = classify(output_buffer, output_used, (int)last_exit_code);
    if (!result_matches_expected_target(verdict)) {
        return MR_XPROC_POLL_OUTCOME_UNKNOWN;
    }
    return verdict;
}

int64_t mr_xproc_result_status(void) { return last_status; }
int64_t mr_xproc_result_advised_bytes(void) { return last_advised_bytes; }
int64_t mr_xproc_result_requested_bytes(void) { return last_requested_bytes; }
int64_t mr_xproc_result_regions(void) { return last_regions; }
int64_t mr_xproc_result_error_code(void) { return last_error_code; }
int64_t mr_xproc_result_call_count(void) { return last_call_count; }
int64_t mr_xproc_result_pid(void) { return last_pid; }
int64_t mr_xproc_result_start_time(void) { return last_start_time; }
int64_t mr_xproc_result_start_time_verified(void) { return last_start_time_verified; }
int64_t mr_xproc_result_uid(void) { return last_uid; }
int64_t mr_xproc_result_exit_code(void) { return last_exit_code; }
int64_t mr_xproc_result_signal(void) { return last_signal; }
int64_t mr_xproc_result_mode(void) { return last_mode; }
int64_t mr_xproc_result_target_rss_before_kib(void) { return last_target_rss_before_kib; }
int64_t mr_xproc_result_target_rss_after_kib(void) { return last_target_rss_after_kib; }
int64_t mr_xproc_result_target_swap_before_kib(void) { return last_target_swap_before_kib; }
int64_t mr_xproc_result_target_swap_after_kib(void) { return last_target_swap_after_kib; }
int64_t mr_xproc_result_scope_current_before_bytes(void) { return last_scope_current_before_bytes; }
int64_t mr_xproc_result_scope_current_after_bytes(void) { return last_scope_current_after_bytes; }
int64_t mr_xproc_result_scope_anon_before_bytes(void) { return last_scope_anon_before_bytes; }
int64_t mr_xproc_result_scope_anon_after_bytes(void) { return last_scope_anon_after_bytes; }
int64_t mr_xproc_result_scope_file_before_bytes(void) { return last_scope_file_before_bytes; }
int64_t mr_xproc_result_scope_file_after_bytes(void) { return last_scope_file_after_bytes; }
int64_t mr_xproc_result_scope_swap_before_bytes(void) { return last_scope_swap_before_bytes; }
int64_t mr_xproc_result_scope_swap_after_bytes(void) { return last_scope_swap_after_bytes; }
const char *mr_xproc_result_reason(void) { return last_reason; }
int64_t mr_xproc_local_refusal(void) { return local_refusal; }

#ifdef MEMREDUCT_TESTING
/* Test-only. In the production build this symbol must not exist at all: a
 * classify entry point reachable from the GUI would let a caller decide what
 * the helper "said". A regression test asserts its absence from the ELF. */
int64_t mr_xproc_test_classify(const char *stdout_text, int64_t exit_code)
{
    const char *text = stdout_text == NULL ? "" : stdout_text;
    return classify(text, strlen(text), (int)exit_code);
}

int64_t mr_xproc_test_classify_expected(const char *stdout_text, int64_t exit_code,
                                         int64_t request_id, int64_t pid,
                                         int64_t start_time, int64_t uid,
                                         int64_t identity_hash, int64_t scope_hash,
                                         int64_t region_digest, int64_t max_bytes)
{
    expected_mode = MR_XPROC_MODE_PROCESS_PAGEOUT;
    expected_request_id = (uint64_t)request_id;
    expected_pid = pid;
    expected_start_time = start_time;
    expected_uid = uid;
    expected_identity_hash = (uint64_t)identity_hash;
    expected_scope_hash = (uint64_t)scope_hash;
    expected_region_digest = (uint64_t)region_digest;
    expected_max_bytes = (uint64_t)max_bytes;
    const char *text = stdout_text == NULL ? "" : stdout_text;
    int64_t verdict = classify(text, strlen(text), (int)exit_code);
    return result_matches_expected_target(verdict)
               ? verdict
               : MR_XPROC_POLL_OUTCOME_UNKNOWN;
}
#endif
