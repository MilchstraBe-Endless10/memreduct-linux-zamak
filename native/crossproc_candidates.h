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
#define MR_CAND_MAX 32u
#define MR_CAND_NAME_BYTES 64u
#define MR_CAND_PATH_BYTES 256u
#define MR_CAND_MAX_PROC_ENTRIES 512u

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
    MR_CAND_SKIP_LIMIT = 10         /* the entry ceiling was reached */
};

/* One offered process. `reclaimable_bytes` is the sum of mapping lengths that
 * pass the same filter the helper applies; it is what a request COULD cover. */
struct mr_cand_entry {
    int32_t pid;
    uint64_t start_time;
    uint64_t uid;
    uint64_t reclaimable_bytes;
    uint32_t regions;
    char name[MR_CAND_NAME_BYTES];
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

#endif /* MEMREDUCT_CROSSPROC_CANDIDATES_H */
