/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Scalar bridge for the balanced-clean presentation helpers.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pwd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>

#include "balance_bridge.h"
#include "balance_exec.h"
#include "balance_gui.h"
#include "balance_plan.h"
#include "balance_snapshot.h"

struct plan_state {
    int valid;
    int64_t state;
    int64_t action;
    uint64_t request_bytes;
    int32_t evidence;
};

struct result_state {
    int valid;
    int64_t status;
    int64_t stop_reason;
    uint64_t requested_bytes;
    uint64_t accepted_bytes;
    uint32_t batches_accepted;
    uint32_t batches_rejected;
    int64_t available_delta_bytes;
};

static struct plan_state plan;
static struct result_state result;

int64_t mr_bal_bridge_entry(int64_t built_in, int64_t gate_enabled,
                            int64_t helper_ready, int64_t busy,
                            int64_t no_target)
{
    struct mr_bal_gui_availability a;
    memset(&a, 0, sizeof(a));
    a.built_in = built_in != 0;
    a.gate_enabled = gate_enabled != 0;
    a.helper_ready = helper_ready != 0;
    a.busy = busy != 0;
    a.no_target = no_target != 0;
    return mr_bal_gui_entry(&a);
}

int64_t mr_bal_bridge_plan_set(int64_t state, int64_t action,
                               uint64_t request_bytes,
                               int64_t have_pressure_evidence)
{
    memset(&plan, 0, sizeof(plan));
    if (state < 0 || state > 5) return 0;
    if (action < 0 || action > 2) return 0;
    if (action == 0 && request_bytes != 0) return 0;
    plan.valid = 1;
    plan.state = state;
    plan.action = action;
    plan.request_bytes = request_bytes;
    plan.evidence = have_pressure_evidence != 0;
    return 1;
}

int64_t mr_bal_bridge_result_set(int64_t status, int64_t stop_reason,
                                 uint64_t requested_bytes,
                                 uint64_t accepted_bytes,
                                 uint32_t batches_accepted,
                                 uint32_t batches_rejected,
                                 int64_t available_delta_bytes)
{
    memset(&result, 0, sizeof(result));
    if (status < 0 || status > 5) return 0;
    if (accepted_bytes > requested_bytes) return 0;
    result.valid = 1;
    result.status = status;
    result.stop_reason = stop_reason;
    result.requested_bytes = requested_bytes;
    result.accepted_bytes = accepted_bytes;
    result.batches_accepted = batches_accepted;
    result.batches_rejected = batches_rejected;
    result.available_delta_bytes = available_delta_bytes;
    return 1;
}

const char *mr_bal_bridge_plan_action(int64_t zh)
{
    if (!plan.valid) return "";
    return mr_bal_gui_plan_line(MR_BAL_LINE_ACTION, zh != 0, plan.state,
                                plan.action, plan.request_bytes, plan.evidence);
}

const char *mr_bal_bridge_plan_why(int64_t zh)
{
    if (!plan.valid) return "";
    return mr_bal_gui_plan_line(MR_BAL_LINE_WHY, zh != 0, plan.state,
                                plan.action, plan.request_bytes, plan.evidence);
}

const char *mr_bal_bridge_plan_budget(int64_t zh)
{
    if (!plan.valid) return "";
    return mr_bal_gui_plan_line(MR_BAL_LINE_BUDGET, zh != 0, plan.state,
                                plan.action, plan.request_bytes, plan.evidence);
}

const char *mr_bal_bridge_plan_skips(int64_t zh)
{
    if (!plan.valid) return "";
    return mr_bal_gui_plan_line(MR_BAL_LINE_SKIPS, zh != 0, plan.state,
                                plan.action, plan.request_bytes, plan.evidence);
}

const char *mr_bal_bridge_result_headline(int64_t zh)
{
    if (!result.valid) return "";
    return mr_bal_gui_result_line(MR_BAL_RESULT_HEADLINE, zh != 0, result.status,
                                  result.stop_reason, result.requested_bytes,
                                  result.accepted_bytes, result.batches_accepted,
                                  result.batches_rejected, result.available_delta_bytes);
}

const char *mr_bal_bridge_result_counters(int64_t zh)
{
    if (!result.valid) return "";
    return mr_bal_gui_result_line(MR_BAL_RESULT_COUNTERS, zh != 0, result.status,
                                  result.stop_reason, result.requested_bytes,
                                  result.accepted_bytes, result.batches_accepted,
                                  result.batches_rejected, result.available_delta_bytes);
}

const char *mr_bal_bridge_result_stop(int64_t zh)
{
    if (!result.valid) return "";
    return mr_bal_gui_result_line(MR_BAL_RESULT_STOP, zh != 0, result.status,
                                  result.stop_reason, result.requested_bytes,
                                  result.accepted_bytes, result.batches_accepted,
                                  result.batches_rejected, result.available_delta_bytes);
}

const char *mr_bal_bridge_result_observed(int64_t zh)
{
    if (!result.valid) return "";
    return mr_bal_gui_result_line(MR_BAL_RESULT_OBSERVED, zh != 0, result.status,
                                  result.stop_reason, result.requested_bytes,
                                  result.accepted_bytes, result.batches_accepted,
                                  result.batches_rejected, result.available_delta_bytes);
}

int64_t mr_bal_bridge_result_meaningful(void)
{
    if (!result.valid) return 0;
    return mr_bal_gui_result_is_meaningful(result.status, result.accepted_bytes);
}

/* ---- execute path ---------------------------------------------------- */

static char scope_root[512];

/* Derive the app-scope root for the calling user. Read from the kernel, not
 * from the environment: an environment variable would let a caller point the
 * reclaim at another user's slice. */
const char *mr_bal_bridge_scope_root(void)
{
    scope_root[0] = '\0';
    FILE *fh = fopen("/proc/self/cgroup", "r");
    if (fh == NULL) return scope_root;
    char line[1024];
    char path[512] = "";
    while (fgets(line, sizeof(line), fh) != NULL) {
        /* cgroup v2: "0::<path>" */
        if (strncmp(line, "0::", 3) == 0) {
            size_t length = strlen(line + 3);
            while (length > 0 && (line[3 + length - 1] == '\n' || line[3 + length - 1] == '\r'))
                length--;
            if (length < sizeof(path)) {
                memcpy(path, line + 3, length);
                path[length] = '\0';
            }
            break;
        }
    }
    (void)fclose(fh);
    if (path[0] != '/') return scope_root;
    /* The calling process is usually in a leaf scope (a dsh-subprocess or an
     * app scope), not in app.slice itself. Trim back to the first "/app.slice"
     * component: that component can only appear once in a session path, so
     * this finds the right ancestor without counting levels. */
    const char *marker = "/app.slice";
    size_t marker_length = strlen(marker);
    const char *found = NULL;
    for (const char *cursor = path; *cursor != '\0'; cursor++) {
        if (strncmp(cursor, marker, marker_length) == 0) {
            /* Only accept it as a whole component: followed by '/' or end. */
            char after = cursor[marker_length];
            if (after == '\0' || after == '/') found = cursor;
        }
    }
    if (found == NULL) {
        /* Fallback: not inside app.slice (e.g. launched bare from a session
         * scope). The picker still needs the user's app.slice, so derive it
         * from the session root + the fixed component name rather than
         * returning empty and showing a picker with zero rows. */
        const char *session = mr_bal_exec_session_root();
        if (session[0] != '/') return scope_root;
        snprintf(scope_root, sizeof(scope_root), "%s/app.slice", session);
        return scope_root;
    }
    size_t root_length = (size_t)(found - path) + marker_length;
    /* /proc/self/cgroup reports the path relative to the cgroup mount point;
     * the filesystem calls need it rooted there. */
    const char *mount = "/sys/fs/cgroup";
    size_t mount_length = strlen(mount);
    if (mount_length + root_length < sizeof(scope_root)) {
        memcpy(scope_root, mount, mount_length);
        memcpy(scope_root + mount_length, path, root_length);
        scope_root[mount_length + root_length] = '\0';
    }
    return scope_root;
}

static int scanned_count;

int64_t mr_bal_bridge_scan_scopes(void)
{
    const char *root = mr_bal_bridge_scope_root();
    scanned_count = 0;
    if (root[0] == '\0') return 0;
    /* The flat buffer must hold every name plus its NUL. */
    static char names[64 * MR_BAL_SCOPE_NAME_MAX];
    memset(names, 0, sizeof(names));
    int64_t count = mr_bal_exec_list_scopes(root, (uint32_t)getuid(),
                                           names, (uint32_t)sizeof(names));
    if (count < 0) return 0;
    scanned_count = (int)count;
    return count;
}

const char *mr_bal_bridge_scope_name(int64_t index)
{
    return mr_bal_exec_scope_name(index);
}

const char *mr_bal_bridge_scope_label(int64_t index)
{
    return mr_bal_exec_app_label(mr_bal_exec_scope_name(index));
}

/* Reads the three counters a picker row shows. Separate from the executor so
 * the panel never opens memory.reclaim just to display a number. */
int mr_bal_bridge_scope_view(int64_t index, struct mr_bal_scope_view *out)
{
    if (out == NULL) return -1;
    memset(out, 0, sizeof(*out));
    out->file_bytes = -1;
    out->anon_bytes = -1;
    out->current_bytes = -1;
    if (index < 0 || index >= scanned_count) return -1;

    int64_t file = mr_bal_bridge_scope_file_bytes(index);
    out->file_bytes = file;

    const char *root = mr_bal_bridge_scope_root();
    if (root[0] == '\0') return -1;
    int fd = mr_bal_exec_open_scope(root, mr_bal_exec_scope_name(index));
    if (fd < 0) return -1;

    int stat_fd = openat(fd, "memory.stat", O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (stat_fd >= 0) {
        char text[4096];
        ssize_t got = read(stat_fd, text, sizeof(text) - 1);
        (void)close(stat_fd);
        if (got > 0) {
            text[got] = '\0';
            for (char *line = text; *line != '\0';) {
                char *line_end = strchr(line, '\n');
                if (line_end == NULL) line_end = line + strlen(line);
                if (strncmp(line, "anon ", 5) == 0) {
                    uint64_t value = 0;
                    int digits = 0;
                    for (char *p = line + 5; *p >= '0' && *p <= '9'; p++) {
                        value = value * 10u + (uint64_t)(*p - '0');
                        digits++;
                    }
                    if (digits > 0) out->anon_bytes = (int64_t)value;
                }
                if (*line_end == '\0') break;
                line = line_end + 1;
            }
        }
    }
    int current_fd = openat(fd, "memory.current", O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (current_fd >= 0) {
        char text[64];
        ssize_t got = read(current_fd, text, sizeof(text) - 1);
        (void)close(current_fd);
        if (got > 0) {
            text[got] = '\0';
            uint64_t value = 0;
            int digits = 0;
            for (char *p = text; *p >= '0' && *p <= '9'; p++) {
                value = value * 10u + (uint64_t)(*p - '0');
                digits++;
            }
            if (digits > 0) out->current_bytes = (int64_t)value;
        }
    }
    (void)close(fd);
    /* Eligible means there is something a file-first run could actually take.
     * A scope with no file pages is listed but cannot be chosen, which is the
     * difference between "nothing here" and "not allowed". */
    out->eligible = out->file_bytes > 0 ? 1 : 0;
    return 0;
}

static struct mr_bal_scope_view last_view;
static int last_view_valid;

int64_t mr_bal_bridge_scope_refresh(int64_t index)
{
    /* Clear before filling: a failed refresh must not leave the previous
     * scope's numbers readable, because the picker would then draw one
     * application's counters next to another application's name. */
    memset(&last_view, 0, sizeof(last_view));
    last_view.file_bytes = -1;
    last_view.anon_bytes = -1;
    last_view.current_bytes = -1;
    last_view.eligible = 0;
    last_view_valid = mr_bal_bridge_scope_view(index, &last_view) == 0;
    return last_view_valid ? 1 : 0;
}

int64_t mr_bal_bridge_scope_anon_bytes(void)
{
    return last_view_valid ? last_view.anon_bytes : -1;
}

int64_t mr_bal_bridge_scope_current_bytes(void)
{
    return last_view_valid ? last_view.current_bytes : -1;
}

int64_t mr_bal_bridge_scope_eligible(void)
{
    return last_view_valid ? last_view.eligible : 0;
}

int64_t mr_bal_bridge_scope_file_bytes(int64_t index)
{
    if (index < 0 || index >= scanned_count) return -1;
    const char *root = mr_bal_bridge_scope_root();
    if (root[0] == '\0') return -1;
    int fd = mr_bal_exec_open_scope(root, mr_bal_exec_scope_name(index));
    if (fd < 0) return -1;
    int stat_fd = openat(fd, "memory.stat", O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    (void)close(fd);
    if (stat_fd < 0) return -1;
    char text[4096];
    ssize_t got = read(stat_fd, text, sizeof(text) - 1);
    (void)close(stat_fd);
    if (got <= 0) return -1;
    text[got] = '\0';
    for (char *line = text; *line != '\0';) {
        char *line_end = strchr(line, '\n');
        if (line_end == NULL) line_end = line + strlen(line);
        if (strncmp(line, "file ", 5) == 0) {
            uint64_t value = 0;
            int digits = 0;
            for (char *p = line + 5; *p >= '0' && *p <= '9'; p++) {
                value = value * 10u + (uint64_t)(*p - '0');
                digits++;
            }
            return digits > 0 ? (int64_t)value : -1;
        }
        if (*line_end == '\0') break;
        line = line_end + 1;
    }
    return -1;
}

/* Cap the machine-wide budget by what the chosen scope can actually give.
 *
 * The planner reasons about the whole machine, which is right for deciding
 * whether to act at all. But the request is written into ONE scope, so a
 * machine-wide budget can ask a small scope for more than it holds: measured
 * on this host, a plan of 425 MiB against a scope holding 73 KiB of file pages.
 * The scope's own file counter is the only honest ceiling. */
static uint64_t scope_bounded_budget(int64_t index, uint64_t budget_bytes)
{
    int64_t scope_file = mr_bal_bridge_scope_file_bytes(index);
    if (scope_file <= 0) return 0;
    return (uint64_t)scope_file < budget_bytes ? (uint64_t)scope_file : budget_bytes;
}

int64_t mr_bal_bridge_run(int64_t index, uint64_t budget_bytes,
                          uint64_t batch_bytes, uint32_t max_batches,
                          uint32_t psi_stop_centi)
{
    /* Publish an empty result first: a refused run must not leave the previous
     * run's numbers on screen for the GUI to present as this one's. */
    (void)mr_bal_bridge_result_set(MR_BAL_EXEC_STOPPED, MR_BAL_EXEC_STOP_ERROR,
                                   0, 0, 0, 0, 0);
    /* A zero budget means "do not act". Without this the batch default of
     * 32 MiB was clamped down to 0 and the run proceeded to the executor,
     * which then refused for a different reason — a refusal that happened to
     * be correct for the wrong cause. */
    if (budget_bytes == 0) return 0;
    if (index < 0 || index >= scanned_count) return 0;
    const char *root = mr_bal_bridge_scope_root();
    if (root[0] == '\0') return 0;
    const char *name = mr_bal_exec_scope_name(index);
    if (name == NULL || name[0] == '\0') return 0;

    budget_bytes = scope_bounded_budget(index, budget_bytes);
    if (budget_bytes == 0) {
        (void)mr_bal_bridge_result_set(MR_BAL_EXEC_STOPPED, MR_BAL_EXEC_STOP_NO_COLD_PAGES,
                                       0, 0, 0, 0, 0);
        return 1;
    }

    int scope_fd = mr_bal_exec_open_scope(root, name);
    if (scope_fd < 0) return 0;

    struct mr_bal_exec_config config;
    memset(&config, 0, sizeof(config));
    config.budget_bytes = budget_bytes;
    config.batch_bytes = batch_bytes == 0 ? 32ull * 1024 * 1024 : batch_bytes;
    if (config.batch_bytes > config.budget_bytes) config.batch_bytes = config.budget_bytes;
    config.max_batches = max_batches == 0 ? 16u : max_batches;
    config.psi_stop_centi = psi_stop_centi;
    config.expected_uid = (uint32_t)getuid();

    struct mr_bal_exec_result result;
    if (mr_bal_exec_run(scope_fd, &config, &result) != 0) {
        (void)close(scope_fd);
        return 0;
    }
    (void)close(scope_fd);

    int64_t available_delta = 0;
    /* Reuse the existing reader rather than a second parser. */
    (void)mr_bal_bridge_result_set(result.status, result.stop_reason,
                                   result.requested_bytes,
                                   result.accepted_bytes + result.observed_on_reject_bytes,
                                   result.batches_accepted, result.batches_rejected,
                                   available_delta);
    return 1;
}

/* ---- plan probe ------------------------------------------------------ */

/* One planner instance, owned here, so a probe and a run share cooldown and
 * hysteresis instead of each call starting fresh and running back to back. */
static struct mr_bal_planner planner;
static int planner_ready;
static struct mr_bal_plan published_plan;
static int plan_valid;

int64_t mr_bal_bridge_plan_probe(void)
{
    struct mr_bal_snapshot snapshot;
    if (mr_bal_snapshot_read("/proc", "/sys", &snapshot) != 0) return 0;
    if (!planner_ready) {
        mr_bal_planner_init(&planner);
        planner_ready = 1;
    }
    struct mr_bal_config config;
    mr_bal_config_default(&config);

    /* A second sample a short distance away is what makes PSI a rate instead
     * of a 10-second average; without it the planner uses the kernel's own
     * average and says so through the evidence flag below. */
    struct mr_bal_delta delta;
    struct mr_bal_snapshot after;
    int have_delta = 0;
    if (mr_bal_snapshot_read("/proc", "/sys", &after) == 0) {
        uint64_t before_us = snapshot.psi_some_total_us;
        uint64_t after_us = after.psi_some_total_us;
        /* Two back-to-back reads measure nothing; a delta needs elapsed time,
         * which this synchronous call does not have. Report no delta and let
         * the planner fall back to avg10, which is honest about its window. */
        (void)before_us;
        (void)after_us;
        (void)delta;
    }

    uint64_t now_ms = 0;
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
        now_ms = (uint64_t)ts.tv_sec * 1000ull + (uint64_t)(ts.tv_nsec / 1000000);

    if (mr_bal_plan_build(&config, &planner, &snapshot, have_delta ? &delta : NULL,
                          now_ms, &published_plan) != 0) {
        plan_valid = 0;
        return 0;
    }
    plan_valid = 1;
    /* Evidence means "the planner had what it needed to classify", not "there
     * is pressure". A healthy machine is a valid answer with no action. */
    return published_plan.state == MR_BAL_STATE_UNKNOWN ? 0 : 1;
}

int64_t mr_bal_bridge_plan_state_code(void)
{
    return plan_valid ? published_plan.state : MR_BAL_STATE_UNKNOWN;
}

int64_t mr_bal_bridge_plan_action_code(void)
{
    return plan_valid ? published_plan.action : MR_BAL_ACTION_NONE;
}

int64_t mr_bal_bridge_plan_request_bytes(void)
{
    return plan_valid ? (int64_t)published_plan.request_bytes : 0;
}

/* ---- session target ------------------------------------------------------ */

int64_t mr_bal_bridge_session_available(void)
{
    const char *root = mr_bal_exec_session_root();
    return root[0] == '/' ? 1 : 0;
}

const char *mr_bal_bridge_session_root(void)
{
    return mr_bal_exec_session_root();
}

int64_t mr_bal_bridge_session_file_bytes(void)
{
    const char *root = mr_bal_exec_session_root();
    if (root[0] != '/') return -1;
    int fd = mr_bal_exec_open_session(root);
    if (fd < 0) return -1;
    int64_t bytes = mr_bal_exec_session_file_bytes(fd);
    (void)close(fd);
    return bytes;
}

/* Reclaim across the whole user session.
 *
 * The budget is clamped to the session's own file pages, because asking for
 * more than the cgroup holds is the EAGAIN case measured on this host: the
 * kernel accepts the request and gives back whatever it could, which reads as
 * a failed run when it is really the ceiling. */
int64_t mr_bal_bridge_session_run(uint64_t budget_bytes)
{
    const char *root = mr_bal_exec_session_root();
    if (root[0] != '/') return 0;
    int fd = mr_bal_exec_open_session(root);
    if (fd < 0) return 0;
    uint32_t uid = (uint32_t)getuid();
    /* MR_BAL_EXEC_OK is 0, not 1: comparing against 1 here refused every
     * session run while the executor was working correctly. */
    int64_t probe = mr_bal_exec_session_probe(fd, uid);
    if (probe != MR_BAL_EXEC_OK) {
        (void)close(fd);
        return 0;
    }
    int64_t file_bytes = mr_bal_exec_session_file_bytes(fd);
    if (file_bytes <= 0) {
        (void)close(fd);
        (void)mr_bal_bridge_result_set(MR_BAL_EXEC_STOPPED, MR_BAL_EXEC_STOP_NO_COLD_PAGES,
                                       0, 0, 0, 0, 0);
        return 1;
    }
    /* The budget is the session's own file pages, always.
     *
     * A caller-supplied budget was tried and is wrong here: the planner reasons
     * about the whole machine and proposed 512 MiB while the session held
     * 2354 MiB, so the run stopped at 512 MiB for no reason the user could see.
     * The only honest ceiling is what the cgroup actually holds, and asking for
     * more than that is the EAGAIN case measured on this host. budget_bytes is
     * therefore ignored; it stays in the signature so callers that pass a plan
     * value keep compiling, and so the mistake is visible here rather than
     * silently reintroduced. */
    uint64_t budget = (uint64_t)file_bytes;
    (void)budget_bytes;

    struct mr_bal_run_config config;
    mr_bal_run_config_for_budget(budget, &config);
    struct mr_bal_exec_result exec;
    int ran = mr_bal_exec_run_on_fd(fd, &config, &exec) == 0;
    (void)close(fd);
    if (!ran) return 0;

    /* Report the measured change, not the kernel's acknowledgement. Writing
     * 2G to a cgroup holding 1.7 GiB of file pages returns success and reclaims
     * 442 MiB; calling that "accepted 2G" would be the fake-success failure
     * this project keeps guarding against. */
    uint64_t requested = exec.requested_bytes;
    uint64_t actually_gone = 0;
    if (exec.scope_file_before >= 0 && exec.scope_file_after >= 0 &&
        exec.scope_file_before > exec.scope_file_after) {
        actually_gone = (uint64_t)(exec.scope_file_before - exec.scope_file_after);
    }
    uint64_t accepted = exec.accepted_bytes < actually_gone ? exec.accepted_bytes : actually_gone;
    /* The status reported here is the executor's own verdict, never a value
     * chosen to look better: a run that reclaimed nothing is STOPPED, not
     * PARTIAL. */
    int64_t status = exec.status == MR_BAL_EXEC_OK ? MR_BAL_EXEC_OK
                   : (exec.batches_accepted > 0 ? MR_BAL_EXEC_PARTIAL : MR_BAL_EXEC_REFUSED);
    return mr_bal_bridge_result_set(status, exec.stop_reason, requested, accepted,
                                    exec.batches_accepted, exec.batches_rejected,
                                    0) == 1 ? 1 : 0;
}

int64_t mr_bal_bridge_result_status_code(void)
{
    return result.valid ? result.status : MR_BAL_EXEC_BAD_REQUEST;
}

int64_t mr_bal_bridge_result_stop_code(void)
{
    return result.valid ? result.stop_reason : MR_BAL_EXEC_STOP_NONE;
}

int64_t mr_bal_bridge_result_requested_value(void)
{
    return result.valid ? (int64_t)result.requested_bytes : 0;
}

int64_t mr_bal_bridge_result_accepted_value(void)
{
    return result.valid ? (int64_t)result.accepted_bytes : 0;
}

const char *mr_bal_bridge_state_name(int64_t state)
{
    switch (state) {
    case 0: return "unknown";
    case 1: return "healthy";
    case 2: return "healthy_cache";
    case 3: return "cold_file";
    case 4: return "anon_pressure";
    case 5: return "thrashing";
    default: return "invalid";
    }
}

const char *mr_bal_bridge_action_name(int64_t action)
{
    switch (action) {
    case 0: return "none";
    case 1: return "file_first";
    case 2: return "anon_pageout";
    default: return "invalid";
    }
}

int64_t mr_mem_available_bytes(void)
{
    FILE *fh = fopen("/proc/meminfo", "r");
    if (fh == NULL) return -1;
    char line[256];
    int64_t value = -1;
    while (fgets(line, sizeof(line), fh) != NULL) {
        if (strncmp(line, "MemAvailable:", 13) == 0) {
            value = (int64_t)strtoll(line + 13, NULL, 10) * 1024;
            break;
        }
    }
    (void)fclose(fh);
    return value;
}

int64_t mr_ci_deep_requested(void)
{
    const char *v = getenv("MEMREDUCT_CI_DEEP");
    return (v != NULL && v[0] == '1') ? 1 : 0;
}
