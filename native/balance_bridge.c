/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Scalar bridge for the balanced-clean presentation helpers.
 */
#include <stdint.h>
#include <string.h>

#include "balance_bridge.h"
#include "balance_gui.h"

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
