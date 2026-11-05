/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Unprivileged candidate enumeration for the cross-process reclaim surface
 * (Issue #12, M1.1).
 *
 * This file produces a LIST ONLY. It cannot advise, signal, read or write
 * another process's memory, and it holds no privilege. Everything it reports is
 * a candidate for the user to choose from; the privileged helper re-derives all
 * of it independently and is free to refuse.
 *
 * The numbers here are mapping lengths, not savings. A row says "this process
 * has N bytes of private readable mapping that a reclaim request could cover".
 * It does not say that reclaiming them frees N bytes, because on Linux nothing
 * can say that in advance.
 */
#ifndef MEMREDUCT_CROSSPROC_CANDIDATES_H
#define MEMREDUCT_CROSSPROC_CANDIDATES_H

#include <stdint.h>

/* Hard ceilings, mirroring the helper's own. Enumeration must never be able to
 * turn into a system-wide sweep, so both the entry count and the work spent per
 * entry are bounded. */
#define MR_CAND_MAX 256u
#define MR_CAND_NAME_BYTES 64u
#define MR_CAND_PATH_BYTES 256u
#define MR_CAND_PROC_PATH_BYTES 4096u
#define MR_CAND_CMDLINE_BYTES 8192u
#define MR_CAND_MAX_PROC_ENTRIES 4096u

/* Why a process was not offered as a candidate. Reported, never silently
 * dropped, so the UI can explain an empty list. */
enum mr_cand_skip {
    MR_CAND_SKIP_NONE = 0,
    MR_CAND_SKIP_PID1 = 1,
    MR_CAND_SKIP_SELF = 2,
    MR_CAND_SKIP_KERNEL_THREAD = 3,
    MR_CAND_SKIP_OTHER_UID = 4,
    MR_CAND_SKIP_UNREADABLE = 5,
    MR_CAND_SKIP_PROTECTED = 6,     /* session/compositor/auth agents */
    MR_CAND_SKIP_ANCESTOR = 7,      /* this program's own parent chain */
    MR_CAND_SKIP_NO_REGIONS = 8,    /* nothing private and readable */
    MR_CAND_SKIP_ZOMBIE = 9,
    MR_CAND_SKIP_LIMIT = 10         /* candidate-list ceiling was reached */
};

/* One offered process. `reclaimable_bytes` is the sum of mapping lengths that
 * pass the same filter the helper applies; it is what a request COULD cover. */
struct mr_cand_entry {
    int32_t pid;
    uint64_t start_time;
    uint64_t uid;
    uint64_t reclaimable_bytes;
    uint64_t identity_token;
    uint64_t scope_token;
    uint64_t region_digest;
    uint32_t regions;
    char state;
    char name[MR_CAND_NAME_BYTES];
    char app_name[MR_CAND_NAME_BYTES];
    char exe[MR_CAND_PATH_BYTES];
};

struct mr_cand_list {
    uint32_t count;
    uint32_t scanned;
    struct mr_cand_entry entries[MR_CAND_MAX];
    uint32_t skipped_self;
    uint32_t skipped_kernel_thread;
    uint32_t skipped_other_uid;
    uint32_t skipped_unreadable;
    uint32_t skipped_protected;
    uint32_t skipped_ancestor;
    uint32_t skipped_no_regions;
    uint32_t skipped_zombie;
    uint32_t skipped_limit;
};

/* Scan the fixed production procfs root and fill `out`. Returns 0 on success,
 * -1 when procfs itself is unreadable. Never blocks on a target: nothing here
 * opens a pidfd, sends a signal, or waits for another process. */
int mr_cand_scan(struct mr_cand_list *out);

/* Read one entry of the last scan. Returns 0 or -1 when out of range. */
int mr_cand_at(const struct mr_cand_list *list, uint32_t index,
               struct mr_cand_entry *out);

/* Is this a process the product must never offer? Session-critical services
 * whose slowdown degrades the whole desktop, and the authentication agents that
 * would have to authorize the action itself. */
int mr_cand_name_is_protected(const char *name);

/* ---- Scalar bridge for Zamak -------------------------------------------
 *
 * A scan fills one static slot; these read it field by field. Call
 * mr_cand_scan_run() first, then index 0..mr_cand_count()-1.
 *
 * Nothing here is a promise about reclaim: mr_cand_reclaimable_bytes() is the
 * sum of mapping lengths a request could cover, and the helper may still refuse
 * the target or cover less.
 * ------------------------------------------------------------------------- */
int64_t mr_cand_scan_run(void);   /* 0 = scan completed, 1 = procfs unreadable */
int64_t mr_cand_count(void);
int64_t mr_cand_scanned(void);
/* reason is an enum mr_cand_skip value; 0 for an unknown one. */
int64_t mr_cand_skipped(int64_t reason);
int64_t mr_cand_pid(int64_t index);
int64_t mr_cand_start_time(int64_t index);
int64_t mr_cand_uid(int64_t index);
int64_t mr_cand_state(int64_t index);
int64_t mr_cand_reclaimable_bytes(int64_t index);
int64_t mr_cand_regions(int64_t index);
int64_t mr_cand_identity_token(int64_t index);
int64_t mr_cand_scope_token(int64_t index);
int64_t mr_cand_region_digest(int64_t index);
/* Pin the currently displayed scan row as the user's immutable selection.
 * A later scan cannot change this snapshot. */
int64_t mr_cand_snapshot_select(int64_t index);
int64_t mr_cand_snapshot_bind_request(int64_t request_id);
int64_t mr_cand_snapshot_request_id(void);
void mr_cand_snapshot_clear(void);
/* Re-read /proc and compare the pinned PID/start/UID/executable/cgroup/command
 * identity, request ID, scope digest and exact eligible mapping digest. Zero
 * means the exact snapshot still matches; every other value means fail closed. */
int64_t mr_cand_snapshot_verify(int64_t request_id, int64_t pid,
                                int64_t start_time, int64_t uid,
                                int64_t identity_token, int64_t scope_token,
                                int64_t region_digest, int64_t bytes,
                                int64_t regions);
/* Owned by the scan slot: valid until the next scan, never freed by the caller. */
const char *mr_cand_name(int64_t index);
/* Basename of the verified /proc/<pid>/exe target; never returns the path. */
const char *mr_cand_app_name(int64_t index);

#endif /* MEMREDUCT_CROSSPROC_CANDIDATES_H */
