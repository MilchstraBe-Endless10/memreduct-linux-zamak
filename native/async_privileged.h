/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MEMREDUCT_ASYNC_PRIVILEGED_H
#define MEMREDUCT_ASYNC_PRIVILEGED_H

#include <stdint.h>

/* Start results: 0 started, 1 busy, 2 unsupported action, 3 launch failed. */
enum mr_reclaim_start_result {
    MR_RECLAIM_STARTED = 0,
    MR_RECLAIM_BUSY = 1,
    MR_RECLAIM_UNSUPPORTED_ACTION = 2,
    MR_RECLAIM_LAUNCH_FAILED = 3
};

/* Poll results: negative values are nonterminal; nonnegative values are terminal.
 * ACCEPTED means the helper reports kernel acceptance, never bytes reclaimed.
 * WAIT_FAILED and CHILD_ABNORMAL leave the operation outcome unknown: do not retry.
 * AUTH_UNCONFIRMED replaces the old "authorization cancelled" result: pkexec
 * forwards the child's status, so codes 126/127 prove that something refused or
 * failed but not which side produced the number. */
enum mr_reclaim_poll_result {
    MR_RECLAIM_WAIT_FAILED = 7,
    MR_RECLAIM_CHILD_ABNORMAL = 6,
    MR_RECLAIM_OUTCOME_UNCERTAIN = 5,
    MR_RECLAIM_HELPER_UNSUPPORTED_ACTION = 4,
    MR_RECLAIM_HELPER_FAILED = 3,
    MR_RECLAIM_AUTH_OR_PKEXEC_FAILED = 2,
    MR_RECLAIM_AUTH_UNCONFIRMED = 1,
    MR_RECLAIM_ACCEPTED = 0,
    MR_RECLAIM_RUNNING = -1,
    MR_RECLAIM_IDLE = -2
};

/* Production authorization switch. The production bridge above is compiled in
 * unconditionally so its behaviour stays auditable, but the GUI may only call
 * it when the build was configured with MEMREDUCT_ENABLE_PRODUCTION_CLEAN=1.
 * That switch stays off until the product owner accepts ADR 0005. */
int64_t mr_production_clean_enabled(void);
/* mode 0 is the sole supported fixed action: pagecache. */
int64_t mr_start_reclaim(int64_t mode);
/* Read-only: what the privileged chain is missing, if anything.
 * 0 ready, 1 helper not installed/untrusted, 2 polkit action missing,
 * 3 pkexec missing. Lets the GUI name the missing component instead of
 * reporting the unattributable 126/127 outcome. */
int64_t mr_reclaim_preflight(void);
/* Test-only UI path: requires MEMREDUCT_CI=1 and a private fake authorizer.
 * It never calls pkexec or the installed production helper. It is absent from
 * async_privileged_production.c builds. */
#ifndef MEMREDUCT_PRODUCTION_BUILD
int64_t mr_start_reclaim_mock(int64_t mode);
#endif
int64_t mr_poll_reclaim(void);
/* Record whether the running child reported its own failure. Exit codes cannot
 * distinguish a pkexec refusal from a child that exited with the same number,
 * so 126/127 without this evidence is reported as unattributed. */
int64_t mr_reclaim_child_reported_failure(int64_t reported);

#endif
