/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The unprivileged execute path for balanced clean (Issue #12).
 *
 * Discovering that this needs no privilege is the point of this file. On a
 * host where the `memory` controller is delegated to the user session, every
 * `app-*.scope` under the user's own slice has a `memory.reclaim` owned by that
 * user with mode 0200. Measured on the target host: uid 1000 can open it
 * O_WRONLY and write, for its own scopes and for other apps' scopes alike.
 *
 * So the balanced clean does NOT go through pkexec, does NOT need a Polkit
 * action, and does NOT touch the cross-process helper. It opens one exact
 * scope directory by fd, verifies that the scope belongs to the caller's uid,
 * and writes the measured-safe form to it.
 *
 * What that comes with, and what this file therefore refuses:
 *   - writing to another app's scope affects that whole scope, so the caller
 *     must have named that scope explicitly. No path here enumerates and picks.
 *   - the kernel may still take anonymous pages if the caller omits the
 *     parameter, so the parameter is mandatory and non-negotiable.
 *   - a scope can disappear between the plan and the run, so the fd is opened
 *     once and the identity re-checked against it.
 */
#ifndef MEMREDUCT_BALANCE_EXEC_H
#define MEMREDUCT_BALANCE_EXEC_H

#include <stdint.h>

#include "balance_reclaim.h"

enum mr_bal_exec_status {
    MR_BAL_EXEC_OK = 0,
    MR_BAL_EXEC_PARTIAL = 1,
    MR_BAL_EXEC_STOPPED = 2,
    MR_BAL_EXEC_REFUSED = 3,
    MR_BAL_EXEC_NOT_DELEGATED = 4,
    MR_BAL_EXEC_WRONG_OWNER = 5,
    MR_BAL_EXEC_BAD_REQUEST = 6
};

/* Why a run stopped, distinct from the planner's reasons because this layer
 * sees different failures (ownership, delegation) than the policy does. */
enum mr_bal_exec_stop {
    MR_BAL_EXEC_STOP_NONE = 0,
    MR_BAL_EXEC_STOP_NO_COLD_PAGES = 1,
    MR_BAL_EXEC_STOP_STALL = 2,
    MR_BAL_EXEC_STOP_ERROR = 3,
    MR_BAL_EXEC_STOP_BUDGET_MET = 4
};

struct mr_bal_exec_config {
    uint64_t budget_bytes;
    uint64_t batch_bytes;
    uint32_t max_batches;
    uint32_t psi_stop_centi;
    /* The verified owning uid. A scope owned by anyone else is refused before
     * a single byte is written, even when the file happens to be writable. */
    uint32_t expected_uid;
};

struct mr_bal_exec_result {
    int64_t status;
    int64_t stop_reason;
    uint64_t requested_bytes;
    uint64_t accepted_bytes;
    uint32_t batches_attempted;
    uint32_t batches_accepted;
    uint32_t batches_rejected;
    /* Bytes observed leaving the scope during a batch the kernel rejected.
     * EAGAIN does not mean "nothing was reclaimed"; it means the request was
     * not fully satisfied. Kept separate from accepted_bytes so the GUI can
     * say "X requested, Y accepted, Z reclaimed by a partial answer". */
    uint64_t observed_on_reject_bytes;
    int32_t error_code;
    int64_t scope_current_before;
    int64_t scope_current_after;
    int64_t scope_anon_before;
    int64_t scope_anon_after;
    int64_t scope_file_before;
    int64_t scope_file_after;
};

/* Open one app scope directory read-only. Returns a directory fd, or -1 with
 * errno set. The caller closes it. The name must be a bare scope name: a path
 * separator, a dot-dot, or a leading slash is refused. */
int mr_bal_exec_open_scope(const char *scope_root, const char *scope_name);

/* Is `memory.reclaim` present and writable by this process for this scope?
 * Checks the fd's owner against expected_uid first. Returns an
 * enum mr_bal_exec_status. */
int64_t mr_bal_exec_probe(int scope_fd, uint32_t expected_uid);

/* Run the batches. Same write form as the privileged path
 * ("<bytes> swappiness=0"), same sticky stop, no fallback. */
int mr_bal_exec_run(int scope_fd, const struct mr_bal_exec_config *config,
                    struct mr_bal_exec_result *out);

/* How many app scopes the caller can actually reclaim in. Returns a count and
 * writes up to `cap` scope names into a flat buffer, each NUL-terminated, so
 * Zamak's scalar FFI can read them by index. Only scopes whose control file is
 * present AND owned by expected_uid are listed: anything else would be offered
 * and then refused, which teaches the user nothing. */
#define MR_BAL_SCOPE_NAME_MAX 128u
int64_t mr_bal_exec_list_scopes(const char *scope_root, uint32_t expected_uid,
                                char *names, uint32_t cap);

/* Name of the i-th listed scope, or "" when out of range. */
const char *mr_bal_exec_scope_name(int64_t index);

/* A short, human-readable app name derived from an `app-*.scope` name:
 * `app-com.google.Chrome-2701326.scope` becomes `com.google.Chrome`. Returns a
 * static buffer; do not free. */
const char *mr_bal_exec_app_label(const char *scope_name);

/* ---- session target -------------------------------------------------------
 *
 * An app scope is the right unit for "this application", but it is the wrong
 * unit for "free memory": measured on this host, the largest app scope held
 * 107 MiB of file pages while the user session held 1592 MiB. A product whose
 * whole purpose is reclaiming memory must be able to address the larger thing.
 *
 * The session target is the user's own systemd session cgroup, which this uid
 * owns and may write. It is NOT the root cgroup: that one is root-only, and
 * reaching for it would mean privilege the balanced path does not need. */
int mr_bal_exec_open_session(const char *session_path);
int64_t mr_bal_exec_session_probe(int fd, uint32_t expected_uid);
int64_t mr_bal_exec_run_on_fd(int fd, const struct mr_bal_run_config *config,
                              struct mr_bal_exec_result *out);

/* Where this user's session cgroup is, derived from /proc/self/cgroup by
 * walking up to the `user@UID.service` component. Returns "" when the process
 * is not inside such a session, which is the honest answer for a container. */
const char *mr_bal_exec_session_root(void);

/* File-page bytes charged to the session cgroup, or -1. */
int64_t mr_bal_exec_session_file_bytes(int fd);

const char *mr_bal_exec_status_name(int64_t status);
const char *mr_bal_exec_stop_name(int64_t stop);

#endif /* MEMREDUCT_BALANCE_EXEC_H */
