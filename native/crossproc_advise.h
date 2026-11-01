/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MEMREDUCT_CROSSPROC_ADVISE_H
#define MEMREDUCT_CROSSPROC_ADVISE_H

#include <stdint.h>

#include <sys/uio.h>

#include "crossproc_plan.h"

/* Writable half of the Issue #12 prototype: process_madvise() against a
 * planned target. It exists to be reviewed and tested, not to be used.
 *
 * Two independent gates, both required:
 *   1. build time  — MEMREDUCT_ENABLE_PRODUCTION_ADVISE must be defined or the
 *                    whole writable implementation compiles to a stub;
 *   2. run time    — mr_crossproc_arm_approval() must have been called for this
 *                    exact plan before any advice is issued. Nothing in the
 *                    application calls it today.
 */

/* Results of one advisory run. BYTES_ADVISED is a length the kernel accepted,
 * never an amount of memory that stayed out. */
enum mr_crossproc_advise_status {
    MR_CROSSPROC_ADVISE_DISABLED = 0,      /* not built in: the default */
    MR_CROSSPROC_ADVISE_NOT_APPROVED = 1,  /* built, but the user gate is shut */
    MR_CROSSPROC_ADVISE_OK = 2,            /* every batch accepted in full */
    MR_CROSSPROC_ADVISE_PARTIAL = 3,       /* kernel covered only part */
    MR_CROSSPROC_ADVISE_NO_PERMISSION = 4, /* EPERM on this target */
    MR_CROSSPROC_ADVISE_TARGET_GONE = 5,   /* ESRCH */
    MR_CROSSPROC_ADVISE_BAD_REQUEST = 6,   /* EINVAL */
    MR_CROSSPROC_ADVISE_UNAVAILABLE = 7,   /* ENOSYS */
    MR_CROSSPROC_ADVISE_FAILED = 8,        /* any other errno */
    MR_CROSSPROC_ADVISE_REENTRANT = 9,     /* a run is already in progress */
    MR_CROSSPROC_ADVISE_EMPTY_PLAN = 10    /* nothing to advise */
};

struct mr_crossproc_advise_result {
    int64_t status;
    uint64_t bytes_advised;   /* syscall return sum, not freed bytes */
    uint32_t batches_ok;
    uint32_t batches_failed;
    uint32_t targets_done;
    int32_t error_code;       /* first errno seen, 0 when none */
};

/* Arm the one-shot user approval for the current plan. Returns 1 when armed,
 * 0 when the writable half is not built in. The arm is consumed by the next
 * run and cleared on any failure, so it can never cover a second action. */
int64_t mr_crossproc_arm_approval(const struct mr_crossproc_plan *plan);

/* Disarm without running. Returns the previous state. */
int64_t mr_crossproc_clear_approval(void);

/* Run the advice for a plan. Never retries, never signals the target. */
struct mr_crossproc_advise_result mr_crossproc_advise_run(struct mr_crossproc_plan *plan);

/* Whether this build contains the writable half at all. */
int64_t mr_crossproc_advise_enabled(void);

#ifdef MEMREDUCT_TESTING
/* Test-only inspection of a finished run, so assertions do not go through the
 * single static bridge slot. */
typedef long (*mr_crossproc_backend)(int pidfd, struct iovec *iov, size_t vlen,
                                     int advice, unsigned int flags);
void mr_crossproc_set_backend(mr_crossproc_backend backend);
int mr_crossproc_test_advise_into(struct mr_crossproc_plan *plan,
                                  struct mr_crossproc_advise_result *out);
int64_t mr_crossproc_test_result_status(const struct mr_crossproc_advise_result *result);
int64_t mr_crossproc_test_result_bytes(const struct mr_crossproc_advise_result *result);
int64_t mr_crossproc_test_result_ok(const struct mr_crossproc_advise_result *result);
int64_t mr_crossproc_test_result_failed(const struct mr_crossproc_advise_result *result);
int64_t mr_crossproc_test_result_targets(const struct mr_crossproc_advise_result *result);
int64_t mr_crossproc_test_result_error(const struct mr_crossproc_advise_result *result);
#endif

/* Scalar bridge for Zamak: run once, then read the fields of that same run. */
int64_t mr_crossproc_advise(void);
int64_t mr_crossproc_advise_bytes(void);
int64_t mr_crossproc_advise_batches_ok(void);
int64_t mr_crossproc_advise_batches_failed(void);
int64_t mr_crossproc_advise_error(void);

#endif /* MEMREDUCT_CROSSPROC_ADVISE_H */
