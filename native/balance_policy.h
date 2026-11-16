/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Conditional default balanced policy (Issue #12, Q14 line B).
 *
 * What this file is: the decision layer that stands between the default
 * "clean" button and the reclaim backend. It answers one question per round —
 * "may the next batch be issued, and against which scopes?" — and it is the
 * only thing allowed to answer it.
 *
 * What this file is NOT: it performs no I/O, opens no file, writes no cgroup.
 * Every input arrives in structs and every output is a plain value, so the
 * whole branch surface can be driven from an injected fixture without a real
 * cgroup anywhere near it.
 *
 * The scope rule (the point of the whole exercise)
 * ------------------------------------------------
 * The target scope is the set of applications the user explicitly enrolled.
 * It is NOT the machine (global drop_caches), and it is NOT the user session
 * as a whole. Measured on the target host, the session slice held anon 86 MiB
 * against file 24 MiB while a single app scope held 74 MiB of file pages, 71
 * MiB of them inactive: the reclaimable material is concentrated in a few
 * apps, and the unreclaimable material is concentrated in different ones.
 * Reaching for the session would mix the two.
 *
 * The cold-page rule
 * ------------------
 * File bytes are not cold bytes. A scope is a candidate only when its
 * INACTIVE file pages carry real weight; a scope whose file charge is mostly
 * active is a scope in use, and asking the kernel to take those pages back is
 * how a "clean" button turns into a refault generator. This is why eligibility
 * is computed from inactive/active file and refault history, never from the
 * total file counter.
 *
 * The anon rule
 * -------------
 * When growth comes from anonymous memory or from repeated swap-in, this
 * policy names the contributing scope and DOES NOT escalate. It never asks for
 * anonymous pageout, and it never turns "reclaim more" into a way to push the
 * curve down: repeated PAGEOUT of a live anon working set is exactly the
 * thrash it would be trying to hide.
 *
 * The depth rule
 * --------------
 * Deep clean stays a manual, explicitly chosen action. Nothing here arms it,
 * schedules it, or reaches it as a fallback.
 */
#ifndef MEMREDUCT_BALANCE_POLICY_H
#define MEMREDUCT_BALANCE_POLICY_H

#include <stdint.h>

/* Why the policy refused to issue a batch. Every one of these is a stop, not a
 * retry hint: the round ends and the caller reports the reason. */
enum mr_pol_stop {
    MR_POL_STOP_NONE = 0,
    MR_POL_STOP_TARGET_MET = 1,       /* the requested water mark is reached */
    MR_POL_STOP_REFAULT = 2,          /* pages we just dropped came straight back */
    MR_POL_STOP_PRESSURE_WORSE = 3,   /* PSI grew during the run */
    MR_POL_STOP_TELEMETRY_MISSING = 4,/* a source we need cannot be read */
    MR_POL_STOP_CANCELLED = 5,        /* the user cancelled */
    MR_POL_STOP_COOLDOWN = 6,         /* this scope refaulted recently */
    MR_POL_STOP_NO_TRIGGER = 7,       /* one of the three preconditions failed */
    MR_POL_STOP_BUDGET_SPENT = 8,     /* the per-round batch ceiling is reached */
    MR_POL_STOP_NO_TARGET = 9         /* nothing enrolled, or nothing suitable */
};

/* What the policy decided the round is about. Reported so the interface can
 * explain itself instead of showing a bare number. */
enum mr_pol_verdict {
    MR_POL_VERDICT_IDLE = 0,          /* no trigger: do nothing */
    MR_POL_VERDICT_RECLAIM = 1,       /* file-only, batched, against enrolled scopes */
    MR_POL_VERDICT_ANON_BOUND = 2,    /* growth is anonymous: name it, do not escalate */
    MR_POL_VERDICT_UNKNOWN = 3        /* telemetry missing: refuse to act */
};

/* One enrolled application scope, as the caller measured it. The policy never
 * reads these itself. */
struct mr_pol_scope {
    int32_t enrolled;        /* 1 = the user explicitly included this app */
    int32_t readable;        /* 0 = memory.stat could not be read */
    uint64_t file_bytes;     /* memory.stat "file" — billing, NOT coldness */
    uint64_t active_file;    /* memory.stat "active_file" */
    uint64_t inactive_file;  /* memory.stat "inactive_file" — the reclaim source */
    uint64_t anon_bytes;     /* memory.stat "anon" */
    uint64_t refault_file;   /* workingset_refault_file, cumulative */
    uint64_t cooldown_until_ms; /* set when this scope refaulted recently */
};

/* Machine-side evidence for one round. */
struct mr_pol_evidence {
    uint64_t now_ms;
    uint64_t mem_total;
    uint64_t mem_available;
    /* PSI some-stall, hundredths of a percent, over the sample window. */
    uint32_t psi_some_centi;
    /* How long the pressure has been continuously observed. The policy refuses
     * to act on a single spiky sample. */
    uint64_t pressure_seen_ms;
    /* Seconds since the last round in this process, 0 when none. */
    uint64_t since_last_run_ms;
};

/* Conservative defaults. Every one of these is a placeholder chosen so the
 * policy is quiet on a healthy machine, not a measured optimum: the calibrating
 * numbers come from line A's replay data and are expected to move these. The
 * struct is the calibration interface — a caller may overwrite any field. */
struct mr_pol_config {
    uint32_t low_water_centi;        /* available below this = not enough */
    uint32_t high_water_centi;       /* reaching this ends the run */
    uint32_t psi_trigger_centi;      /* sustained stall needed to start */
    uint32_t psi_worse_centi;        /* stall above this = stop, do not push */
    uint64_t pressure_dwell_ms;      /* pressure must persist this long */
    uint64_t cooldown_ms;            /* per-scope pause after a refault */
    uint64_t batch_bytes;            /* size of one reclaim write */
    uint32_t max_batches;            /* per-round batch ceiling */
    uint64_t max_request_bytes;      /* per-round request ceiling */
    uint32_t cold_fraction_centi;    /* share of inactive_file we may ask for */
    uint32_t refault_budget_centi;   /* refault/file ratio that marks a scope hot */
};

/* Per-scope memory the caller owns. Nothing here is global, so two windows
 * cannot share a cooldown. */
struct mr_pol_state {
    uint64_t last_run_ms;
    uint32_t batches_issued;
    uint64_t requested_this_round;
    uint64_t available_at_start;
    uint32_t psi_at_start;
    int32_t running;
};

struct mr_pol_decision {
    int64_t verdict;
    int64_t stop;
    /* The scope index to act on next, or -1. Only one scope per round: the
     * point of the batch loop is that it can stop between writes. */
    int64_t scope_index;
    uint64_t request_bytes;
    uint64_t cold_file_bytes;   /* the candidate's inactive_file */
    uint64_t anon_bytes;        /* the candidate's anon, for the report */
    uint64_t water_mark_bytes;  /* what "enough" means for this round */
    int32_t cooldown_active;
};

void mr_pol_config_default(struct mr_pol_config *out);
void mr_pol_state_init(struct mr_pol_state *state);

/* Start a round. Returns 1 when a batch may be issued, 0 otherwise; the
 * decision always carries the reason either way. */
int mr_pol_begin(const struct mr_pol_config *config,
                 struct mr_pol_state *state,
                 const struct mr_pol_evidence *evidence,
                 const struct mr_pol_scope *scopes, uint32_t scope_count,
                 struct mr_pol_decision *out);

/* Called after a batch finished, with what actually moved. Returns 1 when the
 * next batch may be issued. `refaulted` is the number of pages this run's own
 * scopes refaulted since the batch started; `avail_after` and `psi_after` are
 * the fresh readings. A scope index of the same value continues that scope;
 * -1 means "pick the next one". */
int mr_pol_continue(const struct mr_pol_config *config,
                    struct mr_pol_state *state,
                    const struct mr_pol_evidence *evidence,
                    const struct mr_pol_scope *scopes, uint32_t scope_count,
                    uint64_t refaulted, uint64_t avail_after,
                    uint32_t psi_after,
                    struct mr_pol_decision *out);

/* Mark a scope as refaulting so the next round leaves it alone until the
 * cooldown expires. Also the hook a caller uses when it observes a healthy
 * cache being rebuilt rather than genuinely cold pages. */
void mr_pol_cool_scope(struct mr_pol_scope *scope, uint64_t now_ms,
                       uint64_t cooldown_ms);

/* Why a scope was not chosen, for the picker's detail line. Returns a stable
 * name; never NULL. */
const char *mr_pol_scope_skip_reason(const struct mr_pol_config *config,
                                     const struct mr_pol_scope *scope,
                                     const struct mr_pol_evidence *evidence);

/* Stable names. Never NULL. */
const char *mr_pol_stop_name(int64_t stop);
const char *mr_pol_verdict_name(int64_t verdict);

#endif /* MEMREDUCT_BALANCE_POLICY_H */
