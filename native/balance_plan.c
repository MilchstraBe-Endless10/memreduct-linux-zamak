/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Balanced reclaim planner (Issue #12, balanced-clean, line A).
 *
 * Pure: reads a snapshot, writes a plan, touches nothing. The policy is stated
 * once here so the GUI, the log and the tests all describe the same machine.
 */
#include <stdint.h>
#include <string.h>

#include "balance_plan.h"

#define MIB (1024ull * 1024ull)

void mr_bal_config_default(struct mr_bal_config *out)
{
    if (out == NULL) return;
    memset(out, 0, sizeof(*out));
    /* Act only when available memory is under half of total AND the machine is
     * actually stalling. Both conditions, because either alone is misleading:
     * a full file cache is fine, and a stall with plenty available is not our
     * problem to solve by dropping someone's pages. */
    out->low_water_centi = 5000;      /* 50% of MemTotal */
    out->high_water_centi = 6500;     /* 65%: must recover this far to re-arm */
    out->psi_trigger_centi = 500;     /* 5.00% some-stall over the sample window */
    out->psi_stop_centi = 2000;       /* 20.00%: stop, do not make it worse */
    out->cooldown_ms = 30000;         /* 30 s between runs */
    out->recover_ms = 60000;          /* must stay above high water for 60 s */
    out->max_request_bytes = 512ull * MIB;
    out->file_fraction_centi = 2500;  /* ask about at most 25% of cold file pages */
    out->anon_enabled = 0;            /* off unless explicitly switched on */
}

void mr_bal_planner_init(struct mr_bal_planner *planner)
{
    if (planner == NULL) return;
    memset(planner, 0, sizeof(*planner));
    planner->armed = 1;
}

static uint64_t percent_of(uint64_t total, uint32_t centi)
{
    /* centi is hundredths of a percent: total * centi / 10000, without overflow. */
    if (total == 0) return 0;
    uint64_t whole = total / 10000ull;
    uint64_t rem = total % 10000ull;
    if (whole > UINT64_MAX / centi) return UINT64_MAX;
    uint64_t value = whole * centi;
    uint64_t tail = (rem * centi) / 10000ull;
    if (value > UINT64_MAX - tail) return UINT64_MAX;
    return value + tail;
}

static uint32_t psi_for(const struct mr_bal_snapshot *snapshot,
                        const struct mr_bal_delta *delta,
                        int full)
{
    if (delta != NULL && delta->elapsed_ms > 0) {
        uint64_t rate = full ? mr_bal_psi_full_rate_centi(delta)
                             : mr_bal_psi_some_rate_centi(delta);
        return rate > UINT32_MAX ? UINT32_MAX : (uint32_t)rate;
    }
    /* No delta: the kernel's own avg10 is a 10-second average and is exactly
     * what a fresh caller has. Using it is not a guess, it is the weaker
     * evidence, and the plan reports the same numbers either way. */
    return full ? snapshot->psi_full_avg10_centi : snapshot->psi_some_avg10_centi;
}

/* Classify without deciding to act. Ordering encodes severity. */
static int64_t classify(const struct mr_bal_config *config,
                        const struct mr_bal_snapshot *snapshot,
                        const struct mr_bal_delta *delta,
                        uint32_t psi_some,
                        uint64_t *cold_file_out)
{
    if ((snapshot->missing & MR_BAL_MISSING_MEMINFO) != 0) return MR_BAL_STATE_UNKNOWN;
    if (snapshot->total == 0) return MR_BAL_STATE_UNKNOWN;

    /* Cold file pages: the inactive file list is what a file-first reclaim can
     * actually take without touching anyone's anonymous working set. */
    uint64_t cold_file = snapshot->inactive_file;
    *cold_file_out = cold_file;

    /* Swapping with refaults: the machine is already paying for its memory. */
    int swapping = 0;
    if (delta != NULL && delta->elapsed_ms > 0) {
        swapping = (delta->pswpin > 0 || delta->pswpout > 0);
    } else {
        swapping = psi_some > 0 && snapshot->swap_total > snapshot->swap_free;
    }
    uint64_t refaults = delta != NULL ? (delta->refault_anon + delta->refault_file) : 0;
    if (psi_some >= config->psi_stop_centi || (swapping && refaults > 0))
        return MR_BAL_STATE_THRASHING;

    uint64_t low_water = percent_of(snapshot->total, config->low_water_centi);
    int cold_file_present = cold_file > 0;
    int anon_heavy = snapshot->inactive_anon > cold_file;
    int swap_occupied = snapshot->swap_total > snapshot->swap_free;

    if (snapshot->available >= low_water) {
        /* Plenty available. Only stall plus cold file is worth touching. */
        if (psi_some >= config->psi_trigger_centi && cold_file_present)
            return MR_BAL_STATE_COLD_FILE;
        return MR_BAL_STATE_HEALTHY;
    }

    /* Below low water. Which kind of below decides the action, and the two
     * candidate sources are ranked by what dominates the unreclaimable side:
     *   - cold file pages dominate -> file-first is the cheap, safe answer;
     *   - anonymous pages dominate  -> dropping file cache will not help, the
     *     working set itself is the problem.
     * Reporting a file-first action under an anonymous-pressure state (or the
     * reverse) is the incoherence this ordering exists to prevent. */
    if (anon_heavy) return MR_BAL_STATE_ANON_PRESSURE;
    if (cold_file_present) return MR_BAL_STATE_COLD_FILE;
    /* Neither dominates. If swap is already carrying anonymous pages, the
     * machine is paying for memory it cannot get back cheaply. */
    if (swap_occupied || psi_some >= config->psi_trigger_centi)
        return MR_BAL_STATE_ANON_PRESSURE;
    return MR_BAL_STATE_HEALTHY_CACHE;
}

static void fill_evidence(struct mr_bal_plan *out,
                          const struct mr_bal_snapshot *snapshot,
                          uint32_t psi_some, uint32_t psi_full,
                          uint64_t cold_file)
{
    out->available = snapshot->available;
    out->cold_file_bytes = cold_file;
    out->anon_inactive_bytes = snapshot->inactive_anon;
    out->psi_some_centi = psi_some;
    out->psi_full_centi = psi_full;
    out->swap_total = snapshot->swap_total;
    out->swap_used = snapshot->swap_total >= snapshot->swap_free
                         ? snapshot->swap_total - snapshot->swap_free : 0;
    out->swap_free = snapshot->swap_free;
    out->zram_device_count = 0;
    out->zram_full = 0;
}

int mr_bal_plan_build(const struct mr_bal_config *config,
                      struct mr_bal_planner *planner,
                      const struct mr_bal_snapshot *snapshot,
                      const struct mr_bal_delta *delta,
                      uint64_t now_ms,
                      struct mr_bal_plan *out)
{
    if (config == NULL || planner == NULL || snapshot == NULL || out == NULL) return -1;
    memset(out, 0, sizeof(*out));
    out->state = MR_BAL_STATE_UNKNOWN;
    out->action = MR_BAL_ACTION_NONE;
    out->reason = MR_BAL_REASON_NONE;

    uint32_t psi_some = psi_for(snapshot, delta, 0);
    uint32_t psi_full = psi_for(snapshot, delta, 1);
    uint64_t cold_file = 0;
    int64_t state = classify(config, snapshot, delta, psi_some, &cold_file);
    fill_evidence(out, snapshot, psi_some, psi_full, cold_file);

    /* ZRAM accounting: a full zram is a compressed device filling, not the
     * same thing as running out of disk swap. Report both, decide on neither. */
    for (uint32_t i = 0; i < snapshot->swap_devices; i++) {
        if (!snapshot->swap[i].is_zram) continue;
        out->zram_device_count++;
        if (snapshot->swap[i].size > 0 &&
            snapshot->swap[i].used * 100ull / snapshot->swap[i].size >= 95ull)
            out->zram_full = 1;
    }

    out->state = state;
    planner->last_state = (uint32_t)state;

    if (state == MR_BAL_STATE_UNKNOWN) {
        out->reason = MR_BAL_REASON_EVIDENCE_MISSING;
        return 0;
    }

    /* Track recovery for hysteresis before deciding anything. */
    uint64_t high_water = percent_of(snapshot->total, config->high_water_centi);
    if (snapshot->available >= high_water) {
        if (planner->above_high_since_ms == 0) planner->above_high_since_ms = now_ms;
        if (!planner->armed && now_ms - planner->above_high_since_ms >= config->recover_ms) {
            planner->armed = 1;
            planner->last_run_ms = 0;
        }
    } else {
        planner->above_high_since_ms = 0;
    }

    if (state == MR_BAL_STATE_HEALTHY) {
        out->reason = MR_BAL_REASON_ABOVE_HIGH_WATER;
        return 0;
    }
    if (state == MR_BAL_STATE_HEALTHY_CACHE) {
        /* Below low water but the deficit is all reclaimable file cache. The
         * cache is doing its job; dropping it would refill within seconds and
         * only add I/O. Say so instead of manufacturing a success. */
        out->reason = MR_BAL_REASON_NO_COLD_PAGES;
        return 0;
    }
    if (cold_file == 0 && state != MR_BAL_STATE_ANON_PRESSURE) {
        out->reason = MR_BAL_REASON_NO_COLD_PAGES;
        return 0;
    }
    /* Cooldown is checked before the armed flag. A run always disarms, so the
     * other order would report "above_high_water" during the very window the
     * user most needs to be told "just ran, cooling down". */
    if (planner->last_run_ms != 0 && now_ms >= planner->last_run_ms &&
        now_ms - planner->last_run_ms < config->cooldown_ms) {
        out->cooldown_active = 1;
        out->reason = MR_BAL_REASON_COOLDOWN;
        return 0;
    }
    if (!planner->armed) {
        out->reason = MR_BAL_REASON_ABOVE_HIGH_WATER;
        return 0;
    }
    if (psi_some >= config->psi_stop_centi) {
        out->reason = MR_BAL_REASON_STALL_RISING;
        return 0;
    }
    /* Dirty pages still owed to disk: reclaiming now turns into writeback
     * stalls rather than relief. Wait for the backlog to drain. */
    uint64_t dirty_backlog = snapshot->dirty + snapshot->writeback;
    if (dirty_backlog > percent_of(snapshot->total, 100u)) { /* >1% of RAM */
        out->reason = MR_BAL_REASON_DIRTY_BACKLOG;
        return 0;
    }

    /* The state already decided which source dominates. File-first is offered
     * only in the two states that say file pages are the useful side; an
     * anonymous-pressure state must never be answered by dropping file cache,
     * because the pages it would drop are not the pages causing the stall. */
    if (state == MR_BAL_STATE_COLD_FILE && cold_file > 0) {
        uint64_t budget = percent_of(cold_file, config->file_fraction_centi);
        if (budget > config->max_request_bytes) budget = config->max_request_bytes;
        /* One MiB floor: below that the syscall overhead is not worth it, and
         * reporting a 4 KiB "clean" would be the fake success we refuse. */
        if (budget < 1ull * MIB) {
            out->reason = MR_BAL_REASON_NO_COLD_PAGES;
            return 0;
        }
        out->action = MR_BAL_ACTION_FILE_FIRST;
        out->request_bytes = budget;
        out->reason = MR_BAL_REASON_NONE;
        return 0;
    }

    /* Anonymous pressure with no cold file pages. Only reachable when the
     * caller explicitly enabled it; otherwise the honest answer is "no". */
    if (state == MR_BAL_STATE_ANON_PRESSURE) {
        if (!config->anon_enabled) {
            out->reason = MR_BAL_REASON_ANON_DISABLED;
            return 0;
        }
        /* Swapping anonymous pages needs somewhere to put them. Free swap is a
         * necessary condition, not a sufficient one: the caller still has to
         * decide, per scope, whether pushing this working set out helps. */
        if (snapshot->swap_free == 0) {
            out->reason = MR_BAL_REASON_SWAP_FULL;
            return 0;
        }
        uint64_t budget = percent_of(snapshot->inactive_anon, config->file_fraction_centi);
        if (budget > config->max_request_bytes) budget = config->max_request_bytes;
        if (budget < 1ull * MIB) {
            out->reason = MR_BAL_REASON_NO_COLD_PAGES;
            return 0;
        }
        out->action = MR_BAL_ACTION_ANON_PAGEOUT;
        out->request_bytes = budget;
        return 0;
    }

    out->reason = MR_BAL_REASON_NO_COLD_PAGES;
    return 0;
}

void mr_bal_planner_note_run(struct mr_bal_planner *planner, uint64_t now_ms,
                             int64_t action)
{
    if (planner == NULL) return;
    planner->last_run_ms = now_ms;
    planner->last_action = (int32_t)action;
    /* Any real run disarms until the machine recovers above high water for
     * recover_ms. This is the hysteresis half: one run, then quiet. */
    planner->armed = 0;
    planner->above_high_since_ms = 0;
}

const char *mr_bal_state_name(int64_t state)
{
    switch (state) {
    case MR_BAL_STATE_UNKNOWN: return "unknown";
    case MR_BAL_STATE_HEALTHY: return "healthy";
    case MR_BAL_STATE_HEALTHY_CACHE: return "healthy_cache";
    case MR_BAL_STATE_COLD_FILE: return "cold_file";
    case MR_BAL_STATE_ANON_PRESSURE: return "anon_pressure";
    case MR_BAL_STATE_THRASHING: return "thrashing";
    default: return "invalid";
    }
}

const char *mr_bal_action_name(int64_t action)
{
    switch (action) {
    case MR_BAL_ACTION_NONE: return "none";
    case MR_BAL_ACTION_FILE_FIRST: return "file_first";
    case MR_BAL_ACTION_ANON_PAGEOUT: return "anon_pageout";
    default: return "invalid";
    }
}

const char *mr_bal_reason_name(int64_t reason)
{
    switch (reason) {
    case MR_BAL_REASON_NONE: return "none";
    case MR_BAL_REASON_EVIDENCE_MISSING: return "evidence_missing";
    case MR_BAL_REASON_BELOW_LOW_WATER: return "below_low_water";
    case MR_BAL_REASON_NO_COLD_PAGES: return "no_cold_pages";
    case MR_BAL_REASON_COOLDOWN: return "cooldown";
    case MR_BAL_REASON_STALL_RISING: return "stall_rising";
    case MR_BAL_REASON_SWAP_FULL: return "swap_full";
    case MR_BAL_REASON_BUDGET_SPENT: return "budget_spent";
    case MR_BAL_REASON_ANON_DISABLED: return "anon_disabled";
    case MR_BAL_REASON_DIRTY_BACKLOG: return "dirty_backlog";
    case MR_BAL_REASON_ABOVE_HIGH_WATER: return "above_high_water";
    default: return "invalid";
    }
}
