/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Unprivileged bridge to the cross-process reclaim helper (Issue #12, M1.1).
 *
 * The GUI never holds privilege. This module is the only place that spawns the
 * privileged helper, and it does so through pkexec with a literal argv: one
 * fixed helper path, one fixed verb, and three decimal numbers. There is no
 * shell, no path from input, and no way to ask for a different action.
 *
 * Two behaviours are deliberate and tested:
 *   1. At most one run is in flight. A second request is refused, not queued.
 *   2. Nothing retries. A failed or uncertain run stays failed or uncertain.
 *      An advice may have been applied, and repeating it doubles the cost for
 *      no promised gain.
 */
#ifndef MEMREDUCT_ASYNC_CROSSPROC_H
#define MEMREDUCT_ASYNC_CROSSPROC_H

#include <stdint.h>

#include "crossproc_helper.h"

/* Start results. Nonnegative means a child was spawned; the list is separate
 * from the poll states so "it never ran" and "it ran and refused" cannot be
 * confused. */
enum mr_xproc_start {
    MR_XPROC_START_OK = 0,
    MR_XPROC_START_BUSY = 1,        /* a run is already in flight */
    MR_XPROC_START_BAD_REQUEST = 2, /* the target does not satisfy the contract */
    MR_XPROC_START_LAUNCH_FAILED = 3
};

/* Poll results. Negative values are nonterminal. The terminal set keeps four
 * kinds of "no" apart, because they need four different sentences:
 *   - we refused before spawning (REFUSED_LOCAL);
 *   - the launch failed (LAUNCH_FAILED);
 *   - pkexec or the authorization failed (AUTH_UNATTRIBUTED when the exit code
 *     cannot prove which);
 *   - the helper ran and the kernel answered (HELPER_*).
 */
enum mr_xproc_poll {
    MR_XPROC_POLL_IDLE = -2,
    MR_XPROC_POLL_RUNNING = -1,
    MR_XPROC_POLL_ACCEPTED = 0,          /* kernel accepted the whole request */
    MR_XPROC_POLL_PARTIAL = 1,           /* kernel covered part of it */
    MR_XPROC_POLL_REFUSED = 2,           /* helper refused; see the reason line */
    MR_XPROC_POLL_NO_PERMISSION = 3,     /* the kernel said EPERM */
    MR_XPROC_POLL_TARGET_GONE = 4,       /* the target vanished */
    MR_XPROC_POLL_UNSUPPORTED = 5,       /* no process_madvise on this kernel */
    MR_XPROC_POLL_NO_REGIONS = 6,        /* nothing eligible to ask about */
    MR_XPROC_POLL_AUTH_UNATTRIBUTED = 7, /* pkexec refused, or the helper did */
    MR_XPROC_POLL_LAUNCH_FAILED = 8,     /* could not spawn pkexec at all */
    MR_XPROC_POLL_OUTCOME_UNKNOWN = 9,   /* the child died or could not be read */
    MR_XPROC_POLL_REFUSED_LOCAL = 10     /* our own pre-checks said no */
};

/* Why our own pre-checks refused, readable through mr_xproc_local_refusal(). */
enum mr_xproc_local_reason {
    MR_XPROC_LOCAL_NONE = 0,
    MR_XPROC_LOCAL_BAD_PID = 1,
    MR_XPROC_LOCAL_BAD_START_TIME = 2,
    MR_XPROC_LOCAL_BAD_BUDGET = 3,
    MR_XPROC_LOCAL_NOT_INSTALLED = 4,   /* helper or policy missing */
    MR_XPROC_LOCAL_NOT_ENABLED = 5      /* this build cannot do it */
};

/* Is the privileged chain present and usable right now? Read-only: stat on the
 * fixed helper path, the fixed policy path and pkexec. Returns an
 * mr_xproc_preflight value. */
enum mr_xproc_preflight {
    MR_XPROC_PREFLIGHT_READY = 0,
    MR_XPROC_PREFLIGHT_NEED_HELPER = 1,
    MR_XPROC_PREFLIGHT_NEED_POLICY = 2,
    MR_XPROC_PREFLIGHT_NEED_PKEXEC = 3,
    MR_XPROC_PREFLIGHT_NEED_KERNEL = 4  /* no pidfd_open / process_madvise */
};
int64_t mr_xproc_preflight(void);

/* Whether this build may reach the privileged path at all. Separate from
 * preflight: a fully installed chain still cannot be used by a build that was
 * compiled with the capability switched off. */
int64_t mr_xproc_enabled(void);

/* Start one reclaim for exactly this target. All three values come from
 * mr_cand_scan plus the user's choice; the helper re-verifies all of them.
 * Returns an mr_xproc_start value. */
int64_t mr_xproc_start(int64_t pid, int64_t start_time, int64_t max_bytes);

/* Poll the running child. Returns an mr_xproc_poll value. Safe to call when
 * idle, which answers MR_XPROC_POLL_IDLE. */
int64_t mr_xproc_poll(void);

/* Fields of the most recent finished run. All valid only after poll returned a
 * terminal value; before that they read 0 and must not be shown. */
int64_t mr_xproc_result_status(void);
int64_t mr_xproc_result_advised_bytes(void);
int64_t mr_xproc_result_requested_bytes(void);
int64_t mr_xproc_result_regions(void);
int64_t mr_xproc_result_error_code(void);
int64_t mr_xproc_result_call_count(void);
int64_t mr_xproc_result_pid(void);
int64_t mr_xproc_result_start_time_verified(void);
int64_t mr_xproc_result_uid(void);
/* The helper's own reason token, stable ASCII, e.g. "target_other_uid".
 * Empty string when the helper produced no result block. */
const char *mr_xproc_result_reason(void);
/* The local refusal reason, for MR_XPROC_POLL_REFUSED_LOCAL. */
int64_t mr_xproc_local_refusal(void);

/* Test-only: run the parser over a synthetic helper output and one exit code,
 * so the classification of every reason string can be asserted without pkexec
 * and without a kernel call. Returns the poll value and fills the fields.
 * Compiled out of production builds: a GUI-reachable classify entry point would
 * let a caller decide what the helper "said". */
#ifdef MEMREDUCT_TESTING
int64_t mr_xproc_test_classify(const char *stdout_text, int64_t exit_code);
#endif

#endif /* MEMREDUCT_ASYNC_CROSSPROC_H */
