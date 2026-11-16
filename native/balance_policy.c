/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Conditional default balanced policy (Issue #12, Q14 line B).
 *
 * Pure decision logic. No open(), no write(), no sysfs, no clock. Every value
 * this file reasons about was measured by the caller and handed in, which is
 * what makes the whole branch surface reachable from a test without a cgroup.
 */
#include <stdint.h>
#include <string.h>

#include "balance_policy.h"

#define MIB (1024ull * 1024ull)

void mr_pol_config_default(struct mr_pol_config *out)
{
    if (out == NULL) return;
    memset(out, 0, sizeof(*out));
    /* Empire of cautious defaults. These are chosen so that a healthy machine
     * is left completely alone; they are NOT measured optima. Line A's replay
     * data is expected to move them, and this struct is the interface for
     * that calibration. */
    out->low_water_centi = 5000;       /* available under 50% of total */
    out->high_water_centi = 6500;      /* run ends at 65% */
    out->psi_trigger_centi = 500;      /* 5.00% some-stall to start */
    out->psi_worse_centi = 2000;       /* 20.00% = stop, we are the problem */
    out->pressure_dwell_ms = 2000;     /* stall must persist 2 s, not spike */
    out->cooldown_ms = 60000;          /* a refaulted scope rests 60 s */
    out->batch_bytes = 32ull * MIB;    /* one write, then decide again */
    out->max_batches = 8;              /* per-round ceiling */
    out->max_request_bytes = 256ull * MIB;
    out->cold_fraction_centi = 2500;   /* ask about at most 25% of inactive */
    out->refault_budget_centi = 1000;  /* >10% refaulted/file = hot scope */
}

void mr_pol_state_init(struct mr_pol_state *state)
{
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    state->running = 0;
}

static uint64_t percent_of(uint64_t total, uint32_t centi)
{
    if (total == 0 || centi == 0) return 0;
    uint64_t whole = total / 10000ull;
    uint64_t rem = total % 10000ull;
    if (whole > UINT64_MAX / centi) return UINT64_MAX;
    uint64_t value = whole * centi;
    uint64_t tail = (rem * centi) / 10000ull;
    if (value > UINT64_MAX - tail) return UINT64_MAX;
    return value + tail;
}

/* A scope is a candidate only when its INACTIVE file pages are worth asking
 * for. Billing size alone is not evidence: a scope whose file charge is mostly
 * active_file is in use, and taking those pages back is how a refault storm
 * starts. */
static int scope_is_candidate(const struct mr_pol_config *config,
                              const struct mr_pol_scope *scope,
                              const struct mr_pol_evidence *evidence,
                              uint64_t *cold_out)
{
    *cold_out = 0;
    if (scope == NULL) return 0;
    if (scope->enrolled == 0) return 0;
    /* Unreadable telemetry is not "probably fine": it is a reason to skip. */
    if (scope->readable == 0) return 0;
    if (scope->inactive_file == 0) return 0;

    /* A scope that just gave pages back and got them straight back is cooling
     * down. Clearing it again would clear a healthy cache. */
    if (scope->cooldown_until_ms != 0 &&
        evidence != NULL && evidence->now_ms < scope->cooldown_until_ms)
        return 0;

    /* Refault history marks a scope whose "cold" pages are actually wanted.
     * Compare against the file charge, not against an absolute number: a big
     * scope can legitimately refault more pages than a small one. */
    if (scope->file_bytes > 0 && config->refault_budget_centi > 0) {
        uint64_t allowed = percent_of(scope->file_bytes,
                                      config->refault_budget_centi);
        if (scope->refault_file > allowed) return 0;
    }

    /* The cold source must dominate the active side, otherwise this is a scope
     * in active use and we leave it alone. Inactive only has to be non-trivial
     * relative to active, not larger — active_file legitimately outnumbers
     * inactive_file on a busy-but-healthy app. */
    uint64_t cold = scope->inactive_file;
    if (scope->active_file > 0 && cold < scope->active_file / 8ull) return 0;

    *cold_out = cold;
    return 1;
}

/* Pick the enrolled scope with the most inactive file pages. Largest first
 * because one round issues one batch at a time: the biggest cold source gives
 * the most information per stop-decision. Ties break toward the lower index so
 * the choice is deterministic. */
static int64_t pick_scope(const struct mr_pol_config *config,
                          const struct mr_pol_scope *scopes, uint32_t count,
                          const struct mr_pol_evidence *evidence,
                          uint64_t *cold_out)
{
    int64_t best = -1;
    uint64_t best_cold = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint64_t cold = 0;
        if (!scope_is_candidate(config, &scopes[i], evidence, &cold)) continue;
        if (best < 0 || cold > best_cold) {
            best = (int64_t)i;
            best_cold = cold;
        }
    }
    if (best >= 0) *cold_out = best_cold;
    return best;
}

/* How much of one scope's cold pages this round may ask about. Bounded three
 * ways: the share of that scope's inactive list, the per-round request ceiling,
 * and what is left of the round budget. */
static uint64_t request_for(const struct mr_pol_config *config,
                            const struct mr_pol_state *state,
                            uint64_t cold)
{
    uint64_t share = percent_of(cold, config->cold_fraction_centi);
    if (share == 0) share = cold; /* a tiny scope still gets one honest ask */
    uint64_t budget = config->max_request_bytes;
    if (state->requested_this_round >= budget) return 0;
    uint64_t remaining = budget - state->requested_this_round;
    if (share > remaining) share = remaining;
    if (share > cold) share = cold;
    if (share > config->batch_bytes) share = config->batch_bytes;
    return share;
}

/* The three preconditions, together. Any one alone is not enough:
 *   - pressure without scarcity means someone else is busy, not us;
 *   - scarcity without pressure means a big cache is doing its job;
 *   - both without a suitable cold scope means there is nothing safe to take. */
static int preconditions_met(const struct mr_pol_config *config,
                             const struct mr_pol_evidence *evidence,
                             struct mr_pol_decision *out)
{
    if (evidence == NULL) {
        out->verdict = MR_POL_VERDICT_UNKNOWN;
        out->stop = MR_POL_STOP_TELEMETRY_MISSING;
        return 0;
    }
    if (evidence->mem_total == 0) {
        out->verdict = MR_POL_VERDICT_UNKNOWN;
        out->stop = MR_POL_STOP_TELEMETRY_MISSING;
        return 0;
    }
    uint64_t low = percent_of(evidence->mem_total, config->low_water_centi);
    out->water_mark_bytes = percent_of(evidence->mem_total,
                                       config->high_water_centi);

    /* Already at or above the target: nothing to do, and saying so is the
     * honest answer rather than clearing a healthy cache. */
    if (evidence->mem_available >= out->water_mark_bytes) {
        out->verdict = MR_POL_VERDICT_IDLE;
        out->stop = MR_POL_STOP_TARGET_MET;
        return 0;
    }
    /* Scarcity. */
    if (evidence->mem_available >= low) {
        out->verdict = MR_POL_VERDICT_IDLE;
        out->stop = MR_POL_STOP_NO_TRIGGER;
        return 0;
    }
    /* Sustained pressure. A single spike must not arm a reclaim. */
    if (evidence->psi_some_centi < config->psi_trigger_centi) {
        out->verdict = MR_POL_VERDICT_IDLE;
        out->stop = MR_POL_STOP_NO_TRIGGER;
        return 0;
    }
    if (evidence->pressure_seen_ms < config->pressure_dwell_ms) {
        out->verdict = MR_POL_VERDICT_IDLE;
        out->stop = MR_POL_STOP_NO_TRIGGER;
        return 0;
    }
    return 1;
}

int mr_pol_begin(const struct mr_pol_config *config,
                 struct mr_pol_state *state,
                 const struct mr_pol_evidence *evidence,
                 const struct mr_pol_scope *scopes, uint32_t scope_count,
                 struct mr_pol_decision *out)
{
    if (config == NULL || state == NULL || out == NULL) return 0;
    memset(out, 0, sizeof(*out));
    out->scope_index = -1;
    out->stop = MR_POL_STOP_NONE;
    out->verdict = MR_POL_VERDICT_IDLE;

    if (!preconditions_met(config, evidence, out)) return 0;

    /* Per-process round spacing. Not the per-scope cooldown: this is the one
     * that stops a caller from looping the whole policy. */
    if (evidence->since_last_run_ms != 0 &&
        evidence->since_last_run_ms < config->cooldown_ms &&
        state->last_run_ms != 0) {
        out->stop = MR_POL_STOP_COOLDOWN;
        out->cooldown_active = 1;
        return 0;
    }

    uint64_t cold = 0;
    int64_t index = pick_scope(config, scopes, scope_count, evidence, &cold);

    if (index < 0) {
        /* Nothing enrolled has suitable cold pages. Before calling it a plain
         * "no target", check whether the enrolled scopes are dominated by
         * anonymous memory — that is the case the issue tells us to NAME and
         * not to escalate. */
        uint64_t anon_total = 0;
        uint64_t cold_total = 0;
        int64_t biggest_anon = -1;
        uint64_t biggest_anon_bytes = 0;
        for (uint32_t i = 0; i < scope_count; i++) {
            if (scopes[i].enrolled == 0 || scopes[i].readable == 0) continue;
            anon_total += scopes[i].anon_bytes;
            cold_total += scopes[i].inactive_file;
            if (scopes[i].anon_bytes > biggest_anon_bytes) {
                biggest_anon_bytes = scopes[i].anon_bytes;
                biggest_anon = (int64_t)i;
            }
        }
        if (anon_total > cold_total && biggest_anon >= 0) {
            out->verdict = MR_POL_VERDICT_ANON_BOUND;
            out->stop = MR_POL_STOP_NO_TARGET;
            out->scope_index = biggest_anon;
            out->anon_bytes = biggest_anon_bytes;
            return 0;
        }
        out->verdict = MR_POL_VERDICT_IDLE;
        out->stop = MR_POL_STOP_NO_TARGET;
        return 0;
    }

    uint64_t request = request_for(config, state, cold);
    if (request == 0) {
        out->verdict = MR_POL_VERDICT_IDLE;
        out->stop = MR_POL_STOP_BUDGET_SPENT;
        return 0;
    }

    out->verdict = MR_POL_VERDICT_RECLAIM;
    out->stop = MR_POL_STOP_NONE;
    out->scope_index = index;
    out->request_bytes = request;
    out->cold_file_bytes = cold;
    out->anon_bytes = scopes[index].anon_bytes;

    state->running = 1;
    state->last_run_ms = evidence->now_ms;
    state->available_at_start = evidence->mem_available;
    state->psi_at_start = evidence->psi_some_centi;
    return 1;
}

int mr_pol_continue(const struct mr_pol_config *config,
                    struct mr_pol_state *state,
                    const struct mr_pol_evidence *evidence,
                    const struct mr_pol_scope *scopes, uint32_t scope_count,
                    uint64_t refaulted, uint64_t avail_after,
                    uint32_t psi_after,
                    struct mr_pol_decision *out)
{
    if (config == NULL || state == NULL || out == NULL) return 0;
    memset(out, 0, sizeof(*out));
    out->scope_index = -1;
    out->stop = MR_POL_STOP_NONE;
    out->verdict = MR_POL_VERDICT_IDLE;

    if (evidence == NULL) {
        out->verdict = MR_POL_VERDICT_UNKNOWN;
        out->stop = MR_POL_STOP_TELEMETRY_MISSING;
        return 0;
    }

    /* Target reached. The run is over even if budget and pressure would allow
     * another batch: this is the "stop at the water mark" rule. */
    uint64_t high = percent_of(evidence->mem_total, config->high_water_centi);
    out->water_mark_bytes = high;
    if (high != 0 && avail_after >= high) {
        out->stop = MR_POL_STOP_TARGET_MET;
        state->running = 0;
        return 0;
    }

    /* What we just dropped came straight back. Clearing again would be fighting
     * the workload, so the round ends and the acting scope is cooled. */
    if (refaulted > 0) {
        out->stop = MR_POL_STOP_REFAULT;
        state->running = 0;
        return 0;
    }

    /* Pressure got worse during the run. Stop; do not push the curve. */
    if (psi_after >= config->psi_worse_centi || psi_after > state->psi_at_start) {
        out->stop = MR_POL_STOP_PRESSURE_WORSE;
        state->running = 0;
        return 0;
    }

    if (state->batches_issued >= config->max_batches) {
        out->stop = MR_POL_STOP_BUDGET_SPENT;
        state->running = 0;
        return 0;
    }
    if (state->requested_this_round >= config->max_request_bytes) {
        out->stop = MR_POL_STOP_BUDGET_SPENT;
        state->running = 0;
        return 0;
    }

    uint64_t cold = 0;
    int64_t index = pick_scope(config, scopes, scope_count, evidence, &cold);
    if (index < 0) {
        out->stop = MR_POL_STOP_NO_TARGET;
        state->running = 0;
        return 0;
    }

    uint64_t request = request_for(config, state, cold);
    if (request == 0) {
        out->stop = MR_POL_STOP_BUDGET_SPENT;
        state->running = 0;
        return 0;
    }

    out->verdict = MR_POL_VERDICT_RECLAIM;
    out->scope_index = index;
    out->request_bytes = request;
    out->cold_file_bytes = cold;
    out->anon_bytes = scopes[index].anon_bytes;
    return 1;
}

void mr_pol_cool_scope(struct mr_pol_scope *scope, uint64_t now_ms,
                       uint64_t cooldown_ms)
{
    if (scope == NULL) return;
    scope->cooldown_until_ms = now_ms + cooldown_ms;
}

const char *mr_pol_scope_skip_reason(const struct mr_pol_config *config,
                                     const struct mr_pol_scope *scope,
                                     const struct mr_pol_evidence *evidence)
{
    if (scope == NULL) return "absent";
    if (scope->enrolled == 0) return "not_enrolled";
    if (scope->readable == 0) return "unreadable";
    if (scope->inactive_file == 0) return "no_cold_pages";
    if (scope->cooldown_until_ms != 0 && evidence != NULL &&
        evidence->now_ms < scope->cooldown_until_ms)
        return "cooldown";
    if (config != NULL && scope->file_bytes > 0 &&
        config->refault_budget_centi > 0) {
        uint64_t allowed = percent_of(scope->file_bytes,
                                      config->refault_budget_centi);
        if (scope->refault_file > allowed) return "refaulting";
    }
    if (scope->active_file > 0 &&
        scope->inactive_file < scope->active_file / 8ull)
        return "active_working_set";
    return "candidate";
}

const char *mr_pol_stop_name(int64_t stop)
{
    switch (stop) {
    case MR_POL_STOP_NONE: return "none";
    case MR_POL_STOP_TARGET_MET: return "target_met";
    case MR_POL_STOP_REFAULT: return "refault";
    case MR_POL_STOP_PRESSURE_WORSE: return "pressure_worse";
    case MR_POL_STOP_TELEMETRY_MISSING: return "telemetry_missing";
    case MR_POL_STOP_CANCELLED: return "cancelled";
    case MR_POL_STOP_COOLDOWN: return "cooldown";
    case MR_POL_STOP_NO_TRIGGER: return "no_trigger";
    case MR_POL_STOP_BUDGET_SPENT: return "budget_spent";
    case MR_POL_STOP_NO_TARGET: return "no_target";
    default: return "invalid";
    }
}

const char *mr_pol_verdict_name(int64_t verdict)
{
    switch (verdict) {
    case MR_POL_VERDICT_IDLE: return "idle";
    case MR_POL_VERDICT_RECLAIM: return "reclaim";
    case MR_POL_VERDICT_ANON_BOUND: return "anon_bound";
    case MR_POL_VERDICT_UNKNOWN: return "unknown";
    default: return "invalid";
    }
}
