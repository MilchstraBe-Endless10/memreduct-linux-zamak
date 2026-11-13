/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Balanced reclaim planner (Issue #12, balanced-clean).
 *
 * The planner is a pure function over evidence. It never touches the kernel;
 * it answers one question: given what the machine looks like right now, which
 * kind of reclaim (if any) is worth attempting, how many bytes may be asked
 * for, and why is everything else skipped.
 *
 * Design rules that are load-bearing:
 *   - The unit of value is stall reduction, not a target memory percentage.
 *     A machine with a cold file cache and no pressure is left alone.
 *   - Every plan carries the evidence it used, so the GUI can explain itself.
 *   - Hysteresis, cooldown and early stop are planner state, not UI state.
 *   - No plan ever says "will free N bytes". It says "will request N bytes".
 */
#ifndef MEMREDUCT_BALANCE_PLAN_H
#define MEMREDUCT_BALANCE_PLAN_H

#include <stdint.h>

#include "balance_snapshot.h"

/* What the machine looks like, taken together. Order matters only in that a
 * later state implies more trouble than an earlier one. */
enum mr_bal_state {
    MR_BAL_STATE_UNKNOWN = 0,        /* evidence missing; refuse to act */
    MR_BAL_STATE_HEALTHY = 1,        /* enough available, no stall: do nothing */
    MR_BAL_STATE_HEALTHY_CACHE = 2,  /* low available but it is all reclaimable file */
    MR_BAL_STATE_COLD_FILE = 3,      /* reclaimable inactive file pages exist */
    MR_BAL_STATE_ANON_PRESSURE = 4,  /* anon active set too large, no swapping */
    MR_BAL_STATE_THRASHING = 5       /* swap I/O + refault: the worst case */
};

/* The action a plan proposes. FILE_FIRST is the only one enabled by default;
 * ANON_PAGEOUT needs its own budget, its own evidence and its own switch. */
enum mr_bal_action {
    MR_BAL_ACTION_NONE = 0,
    MR_BAL_ACTION_FILE_FIRST = 1,
    MR_BAL_ACTION_ANON_PAGEOUT = 2
};

/* Why a proposed action was held back or shrunk. Reported, never silent. */
enum mr_bal_reason {
    MR_BAL_REASON_NONE = 0,
    MR_BAL_REASON_EVIDENCE_MISSING = 1,
    MR_BAL_REASON_BELOW_LOW_WATER = 2,
    MR_BAL_REASON_NO_COLD_PAGES = 3,
    MR_BAL_REASON_COOLDOWN = 4,
    MR_BAL_REASON_STALL_RISING = 5,
    MR_BAL_REASON_SWAP_FULL = 6,
    MR_BAL_REASON_BUDGET_SPENT = 7,
    MR_BAL_REASON_ANON_DISABLED = 8,
    MR_BAL_REASON_DIRTY_BACKLOG = 9,
    MR_BAL_REASON_ABOVE_HIGH_WATER = 10
};

struct mr_bal_config {
    /* Water marks are fractions of MemTotal in hundredths of a percent, so
     * 5000 means "available below 50% of total". Percentages, not fixed MiB:
     * the same policy has to behave sensibly on 8 GiB and 128 GiB machines. */
    uint32_t low_water_centi;
    uint32_t high_water_centi;
    /* Reclaim stops when PSI "some" exceeds this, in hundredths of a percent. */
    uint32_t psi_stop_centi;
    /* Require this much stall before doing anything at all. Reclaiming a
     * perfectly healthy machine is the failure mode this prevents. */
    uint32_t psi_trigger_centi;
    /* Cooldown between two runs, in milliseconds. */
    uint64_t cooldown_ms;
    /* Hysteresis: available must exceed high water for this long before the
     * planner re-arms. Prevents flapping around a single threshold. */
    uint64_t recover_ms;
    /* Per-run request ceiling. This is a request budget, not a promise. */
    uint64_t max_request_bytes;
    /* Fraction of inactive file pages a single run may ask about. */
    uint32_t file_fraction_centi;
    /* Anonymous pageout is off unless both this and the backend allow it. */
    int32_t anon_enabled;
};

struct mr_bal_plan {
    int64_t state;
    int64_t action;
    int64_t reason;
    /* Bytes the caller may request. Always a request, never a promise. */
    uint64_t request_bytes;
    /* Evidence, copied so the plan explains itself without re-reading. */
    uint64_t available;
    uint64_t cold_file_bytes;    /* inactive file pages, the file-first source */
    uint64_t anon_inactive_bytes;
    uint32_t psi_some_centi;
    uint32_t psi_full_centi;
    uint64_t swap_total;
    uint64_t swap_used;
    uint64_t swap_free;
    int32_t zram_device_count;
    int32_t zram_full;
    int32_t cooldown_active;
};

/* Planner state that survives between calls. The caller owns it and passes the
 * same object; nothing is global, so two windows cannot share a cooldown. */
struct mr_bal_planner {
    uint64_t last_run_ms;
    uint64_t above_high_since_ms;
    int32_t armed;
    int32_t last_action;
    uint32_t last_state;
    uint32_t reserved;
};

void mr_bal_config_default(struct mr_bal_config *out);

/* Reset to the initial (armed, never-run) state. */
void mr_bal_planner_init(struct mr_bal_planner *planner);

/* Build a plan from a snapshot. delta may be NULL when no previous sample
 * exists; the planner then falls back to the PSI averages, which is the only
 * honest option, and records that in the evidence it returns. */
int mr_bal_plan_build(const struct mr_bal_config *config,
                      struct mr_bal_planner *planner,
                      const struct mr_bal_snapshot *snapshot,
                      const struct mr_bal_delta *delta,
                      uint64_t now_ms,
                      struct mr_bal_plan *out);

/* Tell the planner a run finished, so the cooldown starts now. */
void mr_bal_planner_note_run(struct mr_bal_planner *planner, uint64_t now_ms,
                             int64_t action);

/* Stable names for the GUI and the log. Never NULL. */
const char *mr_bal_state_name(int64_t state);
const char *mr_bal_action_name(int64_t action);
const char *mr_bal_reason_name(int64_t reason);

#endif /* MEMREDUCT_BALANCE_PLAN_H */
