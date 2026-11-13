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
const char *mr_bal_bridge_state_name(int64_t state);
const char *mr_bal_bridge_action_name(int64_t action);

#endif /* MEMREDUCT_BALANCE_BRIDGE_H */
