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

/* ---- conditional default policy (Issue #12, Q14 line B) ------------------
 *
 * The default action is this policy: not a global sweep, and not the whole
 * session. One round is driven by six calls:
 *
 *   policy_enroll_clear()  - forget every enrolled app
 *   policy_enroll(index)   - explicitly include one scanned app scope
 *   policy_plan()          - measure and decide; 1 means a batch may go
 *   policy_run_batch()     - issue exactly one file-only batch
 *   policy_continue()      - re-decide after that batch; 1 means go on
 *   policy_finish()        - end the round, keep the tally
 *
 * Nothing above reaches drop_caches, the pkexec helper, swap or zram. A round
 * the policy refuses reports why through policy_stop() rather than quietly
 * doing something else.
 */
void mr_bal_bridge_policy_enroll_clear(void);
/* Include the i-th scanned scope. 1 when enrolled, 0 when the index is out of
 * range or the scope cannot be read at all. */
int64_t mr_bal_bridge_policy_enroll(int64_t index);
int64_t mr_bal_bridge_policy_enrolled_count(void);
/* Decide the first batch. 1 = a batch may be issued, 0 = nothing runs. */
int64_t mr_bal_bridge_policy_plan(void);
/* Issue one batch against the scope the last decision named. 1 = attempted. */
int64_t mr_bal_bridge_policy_run_batch(void);
/* Re-decide after a batch. 1 = issue another, 0 = stop (see policy_stop). */
int64_t mr_bal_bridge_policy_continue(void);
/* End the round, keeping the measurements for policy_observed(). */
void mr_bal_bridge_policy_finish(void);

int64_t mr_bal_bridge_policy_verdict(void);
int64_t mr_bal_bridge_policy_stop(void);
int64_t mr_bal_bridge_policy_scope_index(void);
int64_t mr_bal_bridge_policy_request_bytes(void);
int64_t mr_bal_bridge_policy_cold_file_bytes(void);
int64_t mr_bal_bridge_policy_water_mark_bytes(void);
int64_t mr_bal_bridge_policy_batches_issued(void);
int64_t mr_bal_bridge_policy_requested_total(void);
int64_t mr_bal_bridge_policy_observed_bytes(void);
const char *mr_bal_bridge_policy_stop_name(void);
const char *mr_bal_bridge_policy_verdict_name(void);
/* The app scope the policy names as the anon contributor, or "" when it named
 * none. This is what "state the contributing scope, do not escalate" looks
 * like from the interface. */
const char *mr_bal_bridge_policy_anon_scope_label(void);

/* Why the i-th enrolled scope is (or is not) a candidate. Never NULL. */
const char *mr_bal_bridge_policy_scope_reason(int64_t enrolled_index);
/* Test-only: replace the clock the policy reads, so a cooldown is a value and
 * not a sleep. A negative value restores the real clock. */
/* ---- action record (Q16 line A) --------------------------------------
 *
 * A balanced clean used to be visible only as "ran / did not run". These
 * accessors expose what actually happened: the request id, the scopes it
 * confirmed, the absolute path written, every write's byte count / exact text /
 * kernel return / errno / class, and — when nothing was written — which
 * refusal stopped it.
 *
 * The record is filled by the round itself: call action_begin() after the
 * enrollment loop and action_close_auto() when the round ends. Nothing needs
 * the caller to describe what happened; the write entries come from the seam's
 * own bounded log, so a write the seam never saw cannot appear here.
 *
 * "Not observed" is a value, not an omission: an id of 0, a count of 0, a -1
 * result and the reason "not_observed" all mean this process has not produced
 * that observation, never that it is unavailable in principle.
 */
/* Start a record for a new request and return its monotonically increasing id.
 * Must be called after the scopes are enrolled: the confirmed set is read from
 * the enrollment list at this point. */
int64_t mr_bal_bridge_action_begin(void);
/* End the record, deriving the reason from what was actually observed. */
void mr_bal_bridge_action_close_auto(void);
/* End the record with an explicit reason (used when the caller knows one the
 * derivation cannot see, e.g. a user cancel). "" leaves it to the derivation. */
void mr_bal_bridge_action_close(const char *reason);

int64_t mr_bal_bridge_action_observed(void);
int64_t mr_bal_bridge_action_id(void);

/* Confirmed scope set: count, then name / label / scanned-list index per i. */
int64_t mr_bal_bridge_action_scope_count(void);
const char *mr_bal_bridge_action_scope_name(int64_t i);
const char *mr_bal_bridge_action_scope_label(int64_t i);
int64_t mr_bal_bridge_action_scope_index(int64_t i);

/* Absolute path of the write target, "" when the scope root is unavailable. A
 * path present does not imply a write: check action_write_count(). */
const char *mr_bal_bridge_action_target(void);

/* Writes this request produced, and per-write fields of write i. A negative i
 * or one past the end yields -1 / "" so a GUI loop cannot read off the end. */
int64_t mr_bal_bridge_action_write_count(void);
int64_t mr_bal_bridge_action_write_bytes(int64_t i);
const char *mr_bal_bridge_action_write_text(int64_t i);
int64_t mr_bal_bridge_action_write_result(int64_t i);
int64_t mr_bal_bridge_action_write_errno(int64_t i);
const char *mr_bal_bridge_action_write_class(int64_t i);

/* Why nothing was written: "none" | "no_scope" | "not_run" | "write_skipped" |
 * "policy:<stop>" | "not_observed". Never NULL. */
const char *mr_bal_bridge_action_reason(void);
int64_t mr_bal_bridge_action_reason_known(void);

/* The copyable one-line record, and a one-sentence summary for the footer. */
const char *mr_bal_bridge_action_export(void);
/* Write that line to $XDG_STATE_HOME/memreduct/balanced-last.txt (0600) and
 * return the absolute path, "" when it could not be written. The footer is a
 * canvas, so this is what makes "copy that line" actually possible: the path is
 * shown next to the record. */
const char *mr_bal_bridge_action_export_path(void);
/* Why the last export failed, or "" when it succeeded or was never tried.
 * Never NULL. Exported so a failed write can be reported instead of silently
 * degrading the record from "copy this file" to "copy this text". */
const char *mr_bal_bridge_action_export_error(void);

/* When the round opened and when each write returned. Monotonic milliseconds
 * are for lining a write up with an outside sample; the wall-clock pair only
 * says when that was for a human. 0 means not observed. */
unsigned long long mr_bal_bridge_action_open_mono_ms(void);
long long mr_bal_bridge_action_open_wall_unix(void);
unsigned long long mr_bal_bridge_action_write_mono_ms(long index);
const char *mr_bal_bridge_action_summary(int64_t zh);

void mr_bal_bridge_test_set_policy_clock(int64_t now_ms);
/* Test-only: override the pressure evidence so a fixture can state "under
 * sustained pressure" instead of waiting for the machine to produce it. A
 * negative psi restores the live reading. */
void mr_bal_bridge_test_set_policy_pressure(int64_t psi_some_centi,
                                            int64_t pressure_seen_ms);

/* Test-only: pin the memory evidence the policy reasons about. Passing -1
 * restores the live reading. Production code never calls this. */
void mr_bal_bridge_test_set_policy_memory(int64_t total_bytes,
                                          int64_t available_bytes);

/* Test-only: pin the S4 guard's telemetry. -1 restores the live reading. */
void mr_bal_bridge_test_set_deep_telemetry(double psi_some_avg10,
                                           int64_t swap_pages_per_call);

/* ---- deep clean pipeline (S2-S4 on one worker) ---------------------------
 *
 * The deep clean is one asynchronous run, not a single pressure stage. The GUI
 * drives S1 itself (it needs pkexec), then starts this worker for S2-S4 and
 * polls. Nothing here blocks a GTK callback.
 *
 * start(baseline_avail) takes the whole-run MemAvailable baseline in BYTES, read
 * at click time. Passing <= 0 makes it read the current value instead, which
 * would move the baseline past S1 and misreport the run — so callers pass the
 * value they captured when the user confirmed.
 *   0 = started, -1 = thread creation failed, -2 = already running.
 * poll()
 *   1 = finished (joined; every getter below is now meaningful)
 *   0 = still running
 *  -1 = never started
 *
 * Stage indices are 0..3 (S1..S4) and the stage getters return -1 for an index
 * outside that range. S1's flags stay 0 forever: the worker does not own S1.
 *
 * Stop reasons (mr_bal_bridge_deep_stop_reason):
 *   MR_DEEP_STOP_NONE           = 0  still running, or every batch completed
 *   MR_DEEP_STOP_PSI_UNREADABLE = 1  PSI or pswpout unreadable -> stage skipped
 *   MR_DEEP_STOP_SWAP_RATE      = 2  swap-out rate or PSI growth over the limit
 *   MR_DEEP_STOP_USER_CANCEL    = 3  the user cancelled
 *   MR_DEEP_STOP_ERROR          = 4  a syscall failed
 *
 * Three measurements that must never stand in for one another:
 *   net_bytes()    whole-run MemAvailable delta (baseline -> end); may be
 *                  negative; returns 0 when a baseline is missing, so the
 *                  caller must distinguish "no change" from "no baseline".
 *   stage_positive(i)  that stage's own accumulated positive delta only.
 *   kernel_bytes() the session cgroup's pgsteal delta over the window. It
 *                  includes background reclaim, so it is not "what this button
 *                  reclaimed" — it is what the kernel says it took back.
 */
/* Values, not macros: the implementation defines these as enums, and the
 * header must not define the same identifiers a second way. */
enum mr_deep_stage {
    MR_DEEP_STAGE_S1 = 0,
    MR_DEEP_STAGE_S2 = 1,
    MR_DEEP_STAGE_S3 = 2,
    MR_DEEP_STAGE_S4 = 3,
    MR_DEEP_STAGE_COUNT = 4
};

enum mr_deep_stop {
    MR_DEEP_STOP_NONE = 0,
    MR_DEEP_STOP_PSI_UNREADABLE = 1,
    MR_DEEP_STOP_SWAP_RATE = 2,
    MR_DEEP_STOP_USER_CANCEL = 3,
    MR_DEEP_STOP_ERROR = 4,
    /* The kernel returned EAGAIN: it took the parameter list but would not
     * reclaim that many bytes. Distinct from ERROR because it may still have
     * reclaimed part of the request, and from NONE because no more batches
     * were submitted after it. */
    MR_DEEP_STOP_PARTIAL = 5
};

int64_t mr_bal_bridge_deep_start(int64_t baseline_avail);
int64_t mr_bal_bridge_deep_poll(void);
int64_t mr_bal_bridge_deep_cancel(void);
int64_t mr_bal_bridge_deep_stop_reason(void);
int64_t mr_bal_bridge_deep_stage_ran(int64_t stage);
int64_t mr_bal_bridge_deep_stage_ok(int64_t stage);
int64_t mr_bal_bridge_deep_stage_positive(int64_t stage);
int64_t mr_bal_bridge_deep_stage_elapsed_ms(int64_t stage);
int64_t mr_bal_bridge_deep_batches_completed(void);
int64_t mr_bal_bridge_deep_net_bytes(void);
int64_t mr_bal_bridge_deep_kernel_bytes(void);
/* Batches the kernel refused with EAGAIN. Returns -1 when no run has observed
 * anything yet — a caller must print "unknown", never 0, in that case. A
 * refused batch is not a failure: it may have reclaimed part of the ask. */
int64_t mr_bal_bridge_deep_eagain_batches(void);
/* Forget any previous run so the next poll() answers "never started" again.
 * Only meaningful once the worker has joined; a caller that resets a live run
 * loses its results, which is why this is named reset and not stop. */
void mr_bal_bridge_deep_reset(void);
/* Point S4 at a directory that holds a real memory.reclaim file, so the whole
 * pressure path can be driven without touching the live cgroup. Passing NULL
 * restores discovery. This exists because G-04 found the session path had no
 * injectable seam and therefore no test. */
int mr_bal_bridge_test_set_session_root(const char *dir);
/* Test-only: point the app-scope discovery at a fixture tree, so the policy's
 * per-application branches can be driven without a delegated cgroup. NULL
 * restores discovery. No production code calls this. */
int mr_bal_bridge_test_set_scope_root(const char *dir);
/* The root S4 would actually use right now. */
const char *mr_bal_bridge_test_session_root(void);

/* Sleep used by the GUI poll loop between polls. */
int64_t mr_bal_bridge_sleep_ms(int64_t ms);

#endif /* MEMREDUCT_BALANCE_BRIDGE_H */
