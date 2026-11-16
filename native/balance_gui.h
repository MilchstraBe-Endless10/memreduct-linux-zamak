/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Presentation contract for the balanced clean button (Issue #12, line C).
 *
 * This translation unit holds only pure formatting and decision helpers: it
 * reads no kernel state and writes nothing. The GUI calls it so the wording of
 * a plan and of a result is testable without a window, and so no two code paths
 * can describe the same run differently.
 *
 * Two rules that show up in every string here:
 *   - a request budget is never called "freed";
 *   - a skip is always named with a reason, never silently omitted.
 */
#ifndef MEMREDUCT_BALANCE_GUI_H
#define MEMREDUCT_BALANCE_GUI_H

#include <stdint.h>

/* How the balanced entry point should be offered right now. */
enum mr_bal_entry {
    MR_BAL_ENTRY_HIDDEN = 0,      /* not built in: the row is not drawn */
    MR_BAL_ENTRY_DISABLED = 1,    /* drawn, but a prerequisite is missing */
    MR_BAL_ENTRY_READY = 2        /* drawn and clickable */
};

/* The single question the GUI may ask before offering the action. Everything
 * here is a capability fact, never a guess about memory. */
struct mr_bal_gui_availability {
    int32_t built_in;      /* 1 when the backend exists in this binary */
    int32_t gate_enabled;  /* 1 when the production gate is open */
    int32_t helper_ready;  /* 1 when the privileged helper answered preflight */
    int32_t busy;          /* 1 while another run is in flight */
    int32_t no_target;     /* 1 when no eligible scope exists */
};

int64_t mr_bal_gui_entry(const struct mr_bal_gui_availability *availability);

/* Stable identifiers for the two strings the GUI needs about a plan. */
enum mr_bal_plan_line {
    MR_BAL_LINE_ACTION = 0,
    MR_BAL_LINE_WHY = 1,
    MR_BAL_LINE_BUDGET = 2,
    MR_BAL_LINE_SKIPS = 3
};

/* One plan line, in the requested language. Never returns NULL.
 * zh is 1 for Simplified Chinese, 0 for English.
 * The returned pointer is a static string; the caller must not free it. */
const char *mr_bal_gui_plan_line(int64_t line, int32_t zh,
                                 int64_t state, int64_t action,
                                 uint64_t request_bytes,
                                 int32_t have_pressure_evidence);

/* One result line. Counters are requests, and the wording says so. */
enum mr_bal_result_line {
    MR_BAL_RESULT_HEADLINE = 0,
    MR_BAL_RESULT_COUNTERS = 1,
    MR_BAL_RESULT_STOP = 2,
    MR_BAL_RESULT_OBSERVED = 3
};

const char *mr_bal_gui_result_line(int64_t line, int32_t zh,
                                   int64_t status, int64_t stop_reason,
                                   uint64_t requested_bytes,
                                   uint64_t accepted_bytes,
                                   uint32_t batches_accepted,
                                   uint32_t batches_rejected,
                                   int64_t available_delta_bytes);

/* True when the numbers describe a real, observable outcome and the GUI should
 * show them as such. False when the run changed nothing and the honest answer
 * is "no eligible pages", not "0 bytes freed". */
int32_t mr_bal_gui_result_is_meaningful(int64_t status, uint64_t accepted_bytes);

#endif /* MEMREDUCT_BALANCE_GUI_H */
