/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MEMREDUCT_SELF_RECLAIM_H
#define MEMREDUCT_SELF_RECLAIM_H

#include <stdint.h>

/* Outcome of an unprivileged workingset reclaim for this process only.
 * OK means at least one batch was accepted; it never means "freed N bytes",
 * because bytes_paged_out is the length the kernel accepted, not a measurement
 * of what stayed out. */
enum mr_self_reclaim_status {
    MR_SELF_RECLAIM_OK = 0,
    MR_SELF_RECLAIM_UNSUPPORTED = 1,
    MR_SELF_RECLAIM_PIDFD_FAILED = 2,
    MR_SELF_RECLAIM_MAPS_FAILED = 3,
    MR_SELF_RECLAIM_FAILED = 4
};

struct mr_self_reclaim_result {
    int64_t status;
    uint64_t bytes_paged_out;
    uint64_t failed_batches;
    int error_code;
};

/* Ask the kernel to page out this process's own private mappings. Cannot touch
 * any other process; that needs CAP_SYS_NICE and a privileged helper. */
struct mr_self_reclaim_result mr_self_reclaim_run(void);

/* Scalar wrappers for the Zamak extern bridge. Call mr_self_reclaim() first,
 * then read the other three; they describe that same run. */
int64_t mr_self_reclaim(void);
int64_t mr_self_reclaim_bytes(void);
int64_t mr_self_reclaim_failed_batches(void);
int64_t mr_self_reclaim_error(void);

#endif
