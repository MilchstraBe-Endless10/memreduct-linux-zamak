/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Writable half of the cross-process workingset advisory prototype
 * (Issue #12, ADR-0007).
 *
 * This file is the complete, auditable process_madvise() implementation and it
 * refuses to run by default. Two independent gates must both be open:
 *
 *   1. build time: without MEMREDUCT_ENABLE_PRODUCTION_ADVISE the only thing
 *      compiled is a stub that answers MR_CROSSPROC_ADVISE_DISABLED.
 *   2. run time: mr_crossproc_arm_approval() must have armed the plan that is
 *      about to run, and the arm is consumed by that single run. No application
 *      code arms it; only tests do, against their own throwaway child.
 *
 * Why process_madvise and not a signal or a killer: the syscall only asks the
 * kernel to consider pages for reclaim. It cannot terminate, pause, or otherwise
 * interrupt a process. The result is a length the kernel covered, never an
 * amount of memory that stayed out.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <unistd.h>

#include "crossproc_advise.h"

#ifndef __NR_process_madvise
#  if defined(__x86_64__) || defined(__aarch64__)
#    define __NR_process_madvise 440
#  else
#    define __NR_process_madvise (-1)
#  endif
#endif

#ifndef MADV_COLD
#  define MADV_COLD 20
#endif
#ifndef MADV_PAGEOUT
#  define MADV_PAGEOUT 21
#endif

/* Test-only injection point. The real syscall is the production default; a test
 * build may replace it so the full call shape, the errno mapping, and partial
 * results are exercised without advising anything on the host. */
#ifdef MEMREDUCT_TESTING
typedef long (*mr_crossproc_backend)(int pidfd, struct iovec *iov, size_t vlen,
                                     int advice, unsigned int flags);
static mr_crossproc_backend test_backend;

static int test_backend_errno;
void mr_crossproc_test_clear_backend_errno(void);

int mr_crossproc_test_backend_errno(void)
{
    return test_backend_errno;
}

void mr_crossproc_test_clear_backend_errno(void)
{
    test_backend_errno = 0;
}

void mr_crossproc_test_set_backend_errno(int value)
{
    test_backend_errno = value;
}

void mr_crossproc_set_backend(mr_crossproc_backend backend)
{
    test_backend = backend;
}
#endif

static long call_process_madvise(int pidfd, struct iovec *iov, size_t vlen,
                                 int advice, unsigned int flags)
{
#ifdef MEMREDUCT_TESTING
    if (test_backend != NULL) {
        long covered = test_backend(pidfd, iov, vlen, advice, flags);
        if (covered < 0) {
            /* A ctypes/Python backend cannot set the C errno that survives the
             * trampoline, so the injected backend reports the number it wants
             * classified through its own channel. Without this the errno tests
             * silently exercised the default branch and EPERM looked like a
             * generic failure. The production path below is untouched: a real
             * syscall still sets errno itself. */
            int reported = mr_crossproc_test_backend_errno();
            if (reported != 0) {
                mr_crossproc_test_clear_backend_errno();
                errno = reported; /* the documented contract of the test hook */
                return -1;
            }
        }
        return covered;
    }
#endif
#if __NR_process_madvise < 0
    errno = ENOSYS;
    return -1;
#else
    return syscall(__NR_process_madvise, pidfd, iov, vlen, advice, flags);
#endif
}

/* At most one advisory run may be in flight. The arming slot below is a single
 * static value, so a second concurrent run would otherwise be able to run on a
 * different plan than the one the user approved. */
static int run_in_progress;
static int approval_armed;

int64_t mr_crossproc_advise_enabled(void)
{
#ifdef MEMREDUCT_ENABLE_PRODUCTION_ADVISE
    return 1;
#else
    return 0;
#endif
}

int64_t mr_crossproc_arm_approval(const struct mr_crossproc_plan *plan)
{
#ifdef MEMREDUCT_ENABLE_PRODUCTION_ADVISE
    if (plan == NULL || plan->targets == 0 ||
        (plan->status != MR_CROSSPROC_OK &&
         plan->status != MR_CROSSPROC_BUDGET_EXHAUSTED)) {
        approval_armed = 0;
        return 0;
    }
    /* The approval covers exactly this plan's identity set. Re-checking the
     * pidfds here would duplicate the planner; the run re-checks them anyway. */
    approval_armed = 1;
    return 1;
#else
    (void)plan;
    approval_armed = 0;
    return 0;
#endif
}

int64_t mr_crossproc_clear_approval(void)
{
    int64_t previous = approval_armed;
    approval_armed = 0;
    return previous;
}

static void classify_errno(struct mr_crossproc_advise_result *result, int saved_errno)
{

    if (result->error_code == 0) {
        result->error_code = saved_errno;
    }
    /* A failure always wins over a previous OK: reporting OK because an earlier
     * batch worked would describe advice that this run did not apply. */
    result->status = MR_CROSSPROC_ADVISE_FAILED;
    switch (saved_errno) {
        case EPERM:
        case EACCES:
            result->status = MR_CROSSPROC_ADVISE_NO_PERMISSION;
            break;
        case ESRCH:
        case EBADF:
            /* ESRCH: the process is gone. EBADF: the pidfd is no longer usable,
             * which happens for the same reason. Neither is retried. */
            result->status = MR_CROSSPROC_ADVISE_TARGET_GONE;
            break;
        case EINVAL:
            result->status = MR_CROSSPROC_ADVISE_BAD_REQUEST;
            break;
        case ENOSYS:
            result->status = MR_CROSSPROC_ADVISE_UNAVAILABLE;
            break;
        default:
            result->status = MR_CROSSPROC_ADVISE_FAILED;
            break;
    }
}

/* Issue one batch. A short positive return is a partial application: the kernel
 * stopped inside the vector. It is reported, never retried, because repeating a
 * partially applied advice would double the cost for no promised gain. */
static int issue_batch(struct mr_crossproc_advise_result *result, int pidfd,
                       struct iovec *batch, size_t count, int32_t advice)
{
    int kernel_advice = advice == MR_CROSSPROC_ADVICE_PAGEOUT ? MADV_PAGEOUT : MADV_COLD;
    uint64_t requested = 0;
    for (size_t index = 0; index < count; index++) {
        requested += (uint64_t)batch[index].iov_len;
    }

    errno = 0;
    long covered = call_process_madvise(pidfd, batch, count, kernel_advice, 0u);
    if (covered < 0) {
        classify_errno(result, errno);
        result->batches_failed++;
        return -1;
    }
    if (covered > 0) {
        result->bytes_advised += (uint64_t)covered;
        result->batches_ok++;
    }
    if ((uint64_t)covered < requested) {
        /* Partial coverage: keep what the kernel accepted and say so. */
        if (result->status == MR_CROSSPROC_ADVISE_OK) {
            result->status = MR_CROSSPROC_ADVISE_PARTIAL;
        }
    }
    return 0;
}

static void advise_target(struct mr_crossproc_advise_result *result,
                          const struct mr_crossproc_plan *plan,
                          const struct mr_crossproc_target *target)
{
    struct iovec batch[MR_CROSSPROC_MAX_IOVECS];
    size_t count = 0;

    for (uint32_t index = 0; index < target->region_count; index++) {
        uint32_t absolute = target->region_offset + index;
        if (absolute >= plan->regions) {
            break;
        }
        const struct mr_crossproc_region *region = &plan->region_store[absolute];
        uint64_t length = region->end - region->start;
        if (length == 0) {
            continue;
        }
        batch[count].iov_base = (void *)(uintptr_t)region->start;
        batch[count].iov_len = (size_t)length;
        count++;
        if (count == MR_CROSSPROC_MAX_IOVECS) {
            if (issue_batch(result, target->pidfd, batch, count, region->advice) != 0) {
                return;
            }
            count = 0;
        }
    }
    if (count > 0) {
        const struct mr_crossproc_region *last =
            &plan->region_store[target->region_offset + target->region_count - 1];
        /* The flush path used to discard this result. A failure on the final
         * batch was therefore invisible: the earlier batches had already set
         * status to OK, and the caller reported success for advice that never
         * happened. Report it like the mid-loop path does. */
        (void)issue_batch(result, target->pidfd, batch, count, last->advice);
    }
}

struct mr_crossproc_advise_result mr_crossproc_advise_run(struct mr_crossproc_plan *plan)
{
    struct mr_crossproc_advise_result result;
    memset(&result, 0, sizeof(result));

    if (!mr_crossproc_advise_enabled()) {
        result.status = MR_CROSSPROC_ADVISE_DISABLED;
        return result;
    }
    if (run_in_progress) {
        result.status = MR_CROSSPROC_ADVISE_REENTRANT;
        return result;
    }
    if (plan == NULL || plan->targets == 0) {
        result.status = MR_CROSSPROC_ADVISE_EMPTY_PLAN;
        return result;
    }

    run_in_progress = 1;

    /* The user gate. The arm could have been issued for a previous plan, so it
     * is consumed here and cleared on every path out, including failures. */
    if (!approval_armed) {
        run_in_progress = 0;
        result.status = MR_CROSSPROC_ADVISE_NOT_APPROVED;
        return result;
    }
    approval_armed = 0;

    result.status = MR_CROSSPROC_ADVISE_OK;
    for (uint32_t index = 0; index < plan->targets && index < MR_CROSSPROC_MAX_TARGETS; index++) {
        const struct mr_crossproc_target *target = &plan->entries[index];
        if (target->pidfd < 0 || target->region_count == 0) {
            continue;
        }
        advise_target(&result, plan, target);
        result.targets_done++;
        if (result.status == MR_CROSSPROC_ADVISE_NO_PERMISSION) {
            /* A permission refusal says something about the whole run, not just
             * this target: stop instead of hammering the remaining ones. */
            break;
        }
    }

    if (result.status == MR_CROSSPROC_ADVISE_OK &&
        result.batches_ok == 0 && result.batches_failed > 0) {
        result.status = MR_CROSSPROC_ADVISE_FAILED;
    }

    run_in_progress = 0;
    return result;
}

#ifdef MEMREDUCT_TESTING
/* Test builds inspect a finished result directly instead of the static slot. */
int64_t mr_crossproc_test_result_status(const struct mr_crossproc_advise_result *result)
{
    return result == NULL ? -1 : result->status;
}

int64_t mr_crossproc_test_result_bytes(const struct mr_crossproc_advise_result *result)
{
    return result == NULL ? -1 : (int64_t)result->bytes_advised;
}

int64_t mr_crossproc_test_result_ok(const struct mr_crossproc_advise_result *result)
{
    return result == NULL ? -1 : (int64_t)result->batches_ok;
}

int64_t mr_crossproc_test_result_failed(const struct mr_crossproc_advise_result *result)
{
    return result == NULL ? -1 : (int64_t)result->batches_failed;
}

int64_t mr_crossproc_test_result_targets(const struct mr_crossproc_advise_result *result)
{
    return result == NULL ? -1 : (int64_t)result->targets_done;
}

int64_t mr_crossproc_test_result_error(const struct mr_crossproc_advise_result *result)
{
    return result == NULL ? -1 : (int64_t)result->error_code;
}

/* Run by value: ctypes cannot receive the struct from a C return, so the test
 * allocates the storage and hands it in. */
int mr_crossproc_test_advise_into(struct mr_crossproc_plan *plan,
                                  struct mr_crossproc_advise_result *out)
{
    if (out == NULL) {
        return -1;
    }
    *out = mr_crossproc_advise_run(plan);
    return 0;
}
#endif /* MEMREDUCT_TESTING */

/* Scalar bridge for Zamak. It plans with the fixed default limits and the
 * COLD advice (the least destructive of the two), and it still refuses unless
 * the approval gate was armed. Reading a plan never happens implicitly here. */
static struct mr_crossproc_advise_result last_result;

int64_t mr_crossproc_advise(void)
{
    const struct mr_crossproc_limits limits = {
        MR_CROSSPROC_MAX_TARGETS, MR_CROSSPROC_MAX_REGIONS,
        MR_CROSSPROC_MAX_TOTAL_BYTES, MR_CROSSPROC_TIME_BUDGET_MS,
        MR_CROSSPROC_ADVICE_COLD
    };
    struct mr_crossproc_plan plan;
    memset(&plan, 0, sizeof(plan));

    if (!mr_crossproc_advise_enabled()) {
        memset(&last_result, 0, sizeof(last_result));
        last_result.status = MR_CROSSPROC_ADVISE_DISABLED;
        return last_result.status;
    }
    if (!approval_armed) {
        /* Do not even enumerate: an unapproved call must not touch procfs. */
        memset(&last_result, 0, sizeof(last_result));
        last_result.status = MR_CROSSPROC_ADVISE_NOT_APPROVED;
        return last_result.status;
    }
    (void)mr_crossproc_plan_build(&limits, &plan);
    last_result = mr_crossproc_advise_run(&plan);
    mr_crossproc_plan_release(&plan);
    return last_result.status;
}

int64_t mr_crossproc_advise_bytes(void)
{
    return (int64_t)last_result.bytes_advised;
}

int64_t mr_crossproc_advise_batches_ok(void)
{
    return (int64_t)last_result.batches_ok;
}

int64_t mr_crossproc_advise_batches_failed(void)
{
    return (int64_t)last_result.batches_failed;
}

int64_t mr_crossproc_advise_error(void)
{
    return (int64_t)last_result.error_code;
}
