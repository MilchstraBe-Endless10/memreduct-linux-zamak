/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Scalar bridge between the Zamak controller and the balanced-clean helpers.
 *
 * Zamak's FFI can only pass and return scalars and strings, so this file turns
 * the struct-based contract in balance_gui.h into a flat sequence of calls:
 * set the inputs once, then read each line. It performs no reclaim itself and
 * holds no kernel state; the numbers it presents are supplied by the caller
 * from a snapshot the caller already took.
 */
#ifndef MEMREDUCT_BALANCE_BRIDGE_H
#define MEMREDUCT_BALANCE_BRIDGE_H

#include <stdint.h>


/* Availability. All five inputs are capabilities, not measurements. */
int64_t mr_bal_bridge_entry(int64_t built_in, int64_t gate_enabled,
                            int64_t helper_ready, int64_t busy,
                            int64_t no_target);

/* Set the plan inputs for the following line calls. Returns 1 on success,
 * 0 when the arguments are out of range (the lines then describe "unknown"). */
int64_t mr_bal_bridge_plan_set(int64_t state, int64_t action,
                               uint64_t request_bytes,
                               int64_t have_pressure_evidence);

/* Set the result inputs. Returns 1 on success. */
int64_t mr_bal_bridge_result_set(int64_t status, int64_t stop_reason,
                                 uint64_t requested_bytes,
                                 uint64_t accepted_bytes,
                                 uint32_t batches_accepted,
                                 uint32_t batches_rejected,
                                 int64_t available_delta_bytes);

/* Plan lines (mr_bal_plan_line values). Empty string when nothing was set. */
const char *mr_bal_bridge_plan_action(int64_t zh);
const char *mr_bal_bridge_plan_why(int64_t zh);
const char *mr_bal_bridge_plan_budget(int64_t zh);
const char *mr_bal_bridge_plan_skips(int64_t zh);

/* Result lines (mr_bal_result_line values). */
const char *mr_bal_bridge_result_headline(int64_t zh);
const char *mr_bal_bridge_result_counters(int64_t zh);
const char *mr_bal_bridge_result_stop(int64_t zh);
const char *mr_bal_bridge_result_observed(int64_t zh);

/* Whether the last result describes something that actually happened. */
int64_t mr_bal_bridge_result_meaningful(void);

/* The stable state/action names, for the audit line. Never NULL. */
/* ---- execute path ---------------------------------------------------- */
/* The scope root for this user's session, or "" when it cannot be derived. */
const char *mr_bal_bridge_scope_root(void);

/* Rediscover the reclaimable app scopes. Returns the count. */
int64_t mr_bal_bridge_scan_scopes(void);

/* Name and display label of the i-th scanned scope, or "". */
const char *mr_bal_bridge_scope_name(int64_t index);
const char *mr_bal_bridge_scope_label(int64_t index);

/* Full skip/selection picture for one scope, so the picker can explain itself
 * instead of listing a name and a number. */
struct mr_bal_scope_view {
    int64_t file_bytes;      /* reclaimable file pages charged here */
    int64_t anon_bytes;      /* anonymous, which a file-first run must not touch */
    int64_t current_bytes;   /* memory.current */
    int64_t eligible;        /* 1 when this scope would be acted on */
};
int mr_bal_bridge_scope_view(int64_t index, struct mr_bal_scope_view *out);

/* File-page bytes charged to the i-th scanned scope, or -1 when unreadable.
 * This is the only input to "which scope is worth asking"; a scope under
 * active use has pages the kernel will not give back and should lose. */
int64_t mr_bal_bridge_scope_file_bytes(int64_t index);

/* Scalar form of mr_bal_scope_view for the Zamak controller: refresh the view
 * for one index, then read the fields. */
int64_t mr_bal_bridge_scope_refresh(int64_t index);
int64_t mr_bal_bridge_scope_anon_bytes(void);
int64_t mr_bal_bridge_scope_current_bytes(void);
int64_t mr_bal_bridge_scope_eligible(void);

/* Run the plan against the i-th scanned scope with the given budget.
 * Returns 1 when something was attempted, 0 when it refused or there was
 * nothing to do. The result is then readable through the result_* calls. */
int64_t mr_bal_bridge_run(int64_t index, uint64_t budget_bytes,
                          uint64_t batch_bytes, uint32_t max_batches,
                          uint32_t psi_stop_centi);

/* Integers out of the last result, for the controller's own state fields. */
int64_t mr_bal_bridge_result_status_code(void);
int64_t mr_bal_bridge_result_stop_code(void);
int64_t mr_bal_bridge_result_requested_value(void);
int64_t mr_bal_bridge_result_accepted_value(void);

/* Take one read-only snapshot, build a plan and publish it. Returns 1 when a
 * plan was produced, 0 when the evidence was insufficient. */
int64_t mr_bal_bridge_plan_probe(void);
int64_t mr_bal_bridge_plan_state_code(void);
int64_t mr_bal_bridge_plan_action_code(void);
int64_t mr_bal_bridge_plan_request_bytes(void);

/* ---- session target ------------------------------------------------------
 *
 * The app-scope target answers "free this application's pages". The session
 * target answers "free memory", which is what the product is for. Measured on
 * this host: the largest app scope held 107 MiB of file pages while the user
 * session held 1592 MiB, so the scope-limited path could never reach a GB. */
int64_t mr_bal_bridge_session_available(void);
int64_t mr_bal_bridge_session_file_bytes(void);
const char *mr_bal_bridge_session_root(void);
/* Returns 1 when the run happened, 0 when it could not start. The result is
 * available through the mr_bal_bridge_result_* accessors, where accepted means
 * bytes measured leaving the cgroup, not bytes the kernel acknowledged. */
int64_t mr_bal_bridge_session_run(uint64_t budget_bytes);

const char *mr_bal_bridge_state_name(int64_t state);
const char *mr_bal_bridge_action_name(int64_t action);

/* ---- deep-clean pressure stage ------------------------------------------
 *
 * Batched high-pressure sweep on a worker thread. start() launches, poll()
 * reports 1 = finished / 0 = still running / -1 = never started. The four
 * result getters are only meaningful after poll() returned 1:
 *   observed_end_to_end   MemAvailable at exit minus at entry; may be negative
 *   accumulated_positive  sum of positive per-round deltas only
 *   rounds_completed      batches the kernel accepted
 *   partial_flags         bit0 = some batch ended EAGAIN/short (sample kept) */
int64_t mr_bal_bridge_pressure_start(void);
int64_t mr_bal_bridge_pressure_poll(void);
int64_t mr_bal_bridge_pressure_observed_end_to_end(void);
int64_t mr_bal_bridge_pressure_accumulated_positive(void);
int64_t mr_bal_bridge_pressure_rounds_completed(void);
int64_t mr_bal_bridge_pressure_partial_flags(void);
/* Sleep used by the GUI poll loop between polls. */
int64_t mr_bal_bridge_sleep_ms(int64_t ms);

#endif /* MEMREDUCT_BALANCE_BRIDGE_H */
