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
#include <spawn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "async_crossproc.h"

/* Fixed absolute paths. The helper installs to the first; the Polkit action
 * pins it. Only a test build may override any of them. */
#define MR_XPROC_PKEXEC "/usr/bin/pkexec"
#define MR_XPROC_POLICY_PATH \
    "/usr/share/polkit-1/actions/io.github.milchstrasse_endless10.memreduct.zamak.crossproc.policy"

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
static int64_t last_start_time_verified;
static int64_t last_uid;
static char last_reason[MR_XPROC_REASON_BYTES];
static int64_t local_refusal;

/* ---------------------------------------------------------------------------
 * Capability switch.
 *
 * Two translation units define this, exactly like the page-cache gate: the
 * default one answers 0 so the shipped binary cannot reach the privileged path,
 * and the production build script swaps in the enabled one. One definition only,
 * so a binary either can reach it or cannot, and nm can tell which.
 * ------------------------------------------------------------------------- */

static int file_is_trusted_root(const char *path)
{
    struct stat info;
    if (stat(path, &info) != 0) {
        return 0;
    }
    if (!S_ISREG(info.st_mode)) {
        return 0;
    }
    if (info.st_uid != 0) {
        return 0;
    }
    if ((info.st_mode & 0022) != 0) {
        return 0;
    }
    return 1;
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

int64_t mr_xproc_preflight(void)
{
    if (!file_is_trusted_root(helper_path())) {
        return MR_XPROC_PREFLIGHT_NEED_HELPER;
    }
    if (!file_is_trusted_root(MR_XPROC_POLICY_PATH)) {
        return MR_XPROC_PREFLIGHT_NEED_POLICY;
    }
    if (access(pkexec_path(), X_OK) != 0) {
        return MR_XPROC_PREFLIGHT_NEED_PKEXEC;
    }
    return MR_XPROC_PREFLIGHT_READY;
}

/* ---------------------------------------------------------------------------
 * Parsing the helper's result block.
 *
 * A result is accepted only when the marker line is present AND every required
 * key is present exactly once. A partial block is not a weaker success; it is
 * an unreadable answer, and it maps to OUTCOME_UNKNOWN.
 * ------------------------------------------------------------------------- */

struct parsed_result {
    int have_marker;
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
    const char *status_text;
    size_t status_len;
    const char *reason_text;
    size_t reason_len;
    uint64_t advised;
    uint64_t requested;
    uint64_t regions;
    int64_t error_code;
    int64_t calls;
    int64_t pid;
    int verified;
    int uid_known;
    int64_t uid;
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

static int parse_result(const char *text, size_t length, struct parsed_result *out)
{
    memset(out, 0, sizeof(*out));

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
                if (key_is(cursor, line_len, "status", &value, &value_len)) {
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
    return parsed->status_seen == 1 && parsed->reason_seen == 1 &&
           parsed->advised_seen == 1 && parsed->requested_seen == 1 &&
           parsed->regions_seen == 1 && parsed->error_seen == 1 &&
           parsed->calls_seen == 1 && parsed->pid_seen == 1 &&
           parsed->verified_seen == 1 && parsed->uid_seen == 1;
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
    return MR_XPROC_POLL_OUTCOME_UNKNOWN;
}

/* The single place where an exit code and a parsed block become a verdict.
 * Exposed to tests through mr_xproc_test_classify so every branch is asserted
 * against the production code rather than a copy of it. */
static int64_t classify(const char *text, size_t length, int exit_code)
{
    last_status = 0;
    last_advised_bytes = 0;
    last_requested_bytes = 0;
    last_regions = 0;
    last_error_code = 0;
    last_call_count = 0;
    last_pid = 0;
    last_start_time_verified = 0;
    last_uid = -1;
    last_reason[0] = '\0';

    struct parsed_result parsed;
    int parsed_ok = parse_result(text, length, &parsed) == 0 && result_is_complete(&parsed);

    if (parsed_ok) {
        last_advised_bytes = (int64_t)parsed.advised;
        last_requested_bytes = (int64_t)parsed.requested;
        last_regions = (int64_t)parsed.regions;
        last_error_code = parsed.error_code;
        last_call_count = parsed.calls;
        last_pid = parsed.pid;
        last_start_time_verified = parsed.verified;
        last_uid = parsed.uid_known ? parsed.uid : -1;
        size_t copy = parsed.reason_len < sizeof(last_reason) - 1u
                          ? parsed.reason_len
                          : sizeof(last_reason) - 1u;
        memcpy(last_reason, parsed.reason_text, copy);
        last_reason[copy] = '\0';
    }

    /* pkexec(1) returns 126 when the authorization was dismissed and 127 when
     * it could not authorize or launch. It forwards the child's status, so a
     * helper that exited with the same number is indistinguishable. Without a
     * complete result block we therefore report the outcome as unattributed
     * instead of claiming the user cancelled. */
    if (exit_code == 126 || exit_code == 127) {
        if (parsed_ok) {
            /* The helper did speak, so the number came from it or a failure it
             * reported: still not an acceptance. */
            return classify_token(&parsed);
        }
        return MR_XPROC_POLL_AUTH_UNATTRIBUTED;
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

    if (verdict == MR_XPROC_POLL_ACCEPTED && parsed.advised == 0) {
        /* "Accepted" with zero bytes covered contradicts itself. */
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

int64_t mr_xproc_start(int64_t pid, int64_t start_time, int64_t max_bytes)
{
    local_refusal = MR_XPROC_LOCAL_NONE;

    if (child_pid > 0) {
        return MR_XPROC_START_BUSY;
    }
    if (pid < 2 || pid > 0x7fffffff) {
        local_refusal = MR_XPROC_LOCAL_BAD_PID;
        return MR_XPROC_START_BAD_REQUEST;
    }
    if (start_time <= 0) {
        local_refusal = MR_XPROC_LOCAL_BAD_START_TIME;
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

    char pid_text[16];
    char start_text[24];
    char bytes_text[24];
    (void)snprintf(pid_text, sizeof(pid_text), "%lld", (long long)pid);
    (void)snprintf(start_text, sizeof(start_text), "%lld", (long long)start_time);
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
        /* A literal argv. No shell, no string building from input, no path from
         * anything but the fixed install location. */
        char *const argv[] = {
            (char *)pkexec_path(),
            (char *)helper_path(),
            (char *)MR_XPROC_VERB,
            pid_text,
            start_text,
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
    child_stdout = pipe_fds[0];
    output_used = 0;
    output_overflowed = 0;
    return MR_XPROC_START_OK;
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
        return MR_XPROC_POLL_OUTCOME_UNKNOWN;
    }
    if (WIFSIGNALED(status) || !WIFEXITED(status)) {
        /* Killed by a signal: we cannot know how far it got. */
        return MR_XPROC_POLL_OUTCOME_UNKNOWN;
    }
    if (output_overflowed) {
        /* A result we could not read in full is not a result. */
        return MR_XPROC_POLL_OUTCOME_UNKNOWN;
    }

    return classify(output_buffer, output_used, WEXITSTATUS(status));
}

int64_t mr_xproc_result_status(void) { return last_status; }
int64_t mr_xproc_result_advised_bytes(void) { return last_advised_bytes; }
int64_t mr_xproc_result_requested_bytes(void) { return last_requested_bytes; }
int64_t mr_xproc_result_regions(void) { return last_regions; }
int64_t mr_xproc_result_error_code(void) { return last_error_code; }
int64_t mr_xproc_result_call_count(void) { return last_call_count; }
int64_t mr_xproc_result_pid(void) { return last_pid; }
int64_t mr_xproc_result_start_time_verified(void) { return last_start_time_verified; }
int64_t mr_xproc_result_uid(void) { return last_uid; }
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
#endif
