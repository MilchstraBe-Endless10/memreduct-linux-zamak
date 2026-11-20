/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Unprivileged workingset reclaim for this process only.
 *
 * ADR-0006 splits "clean memory" into separate actions. This file implements the
 * one that needs no privilege at all: asking the kernel to page out the calling
 * process's own anonymous memory with process_madvise(MADV_PAGEOUT).
 *
 * It deliberately cannot touch any other process. Measured behaviour on this
 * machine: the same call against a different process, even one owned by the same
 * user, fails with EPERM. Targeting anyone else requires CAP_SYS_NICE and goes
 * through the privileged helper instead, never through this path.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <unistd.h>

#include "self_reclaim.h"

#ifndef __NR_process_madvise
#  if defined(__x86_64__)
#    define __NR_process_madvise 440
#  elif defined(__aarch64__)
#    define __NR_process_madvise 440
#  else
#    define __NR_process_madvise (-1)
#  endif
#endif

#ifndef __NR_pidfd_open
#  define __NR_pidfd_open 434
#endif

/* MADV_PAGEOUT exists since Linux 5.4; the header may predate it. */
#ifndef MADV_PAGEOUT
#  define MADV_PAGEOUT 21
#endif

/* Reclaiming in one syscall over the whole address space would need the caller
 * to know every mapping. Walking /proc/self/maps is the documented recipe, and
 * a bounded batch keeps a single unreadable mapping from aborting the run. */
#define MR_SELF_BATCH 32

struct mr_self_reclaim_result mr_self_reclaim_run(void)
{
    struct mr_self_reclaim_result result;
    memset(&result, 0, sizeof(result));

#if __NR_process_madvise < 0
    result.status = MR_SELF_RECLAIM_UNSUPPORTED;
    return result;
#else
    int pidfd = (int)syscall(__NR_pidfd_open, (pid_t)getpid(), 0u);
    if (pidfd < 0) {
        result.status = MR_SELF_RECLAIM_PIDFD_FAILED;
        result.error_code = errno;
        return result;
    }

    FILE *maps = fopen("/proc/self/maps", "re");
    if (maps == NULL) {
        result.status = MR_SELF_RECLAIM_MAPS_FAILED;
        result.error_code = errno;
        close(pidfd);
        return result;
    }

    struct iovec batch[MR_SELF_BATCH];
    size_t batch_len = 0;
    unsigned long start = 0;
    unsigned long end = 0;
    char perms[8];
    char line[512];

    /* Only private anonymous or private file-backed mappings are candidates;
     * shared mappings belong to other processes and must not be pushed out from
     * under them. */
    while (fgets(line, sizeof(line), maps) != NULL) {
        perms[0] = '\0';
        if (sscanf(line, "%lx-%lx %7s", &start, &end, perms) != 3) {
            continue;
        }
        if (perms[0] != 'r' || perms[3] != 'p') {
            continue;
        }
        if (end <= start) {
            continue;
        }
        batch[batch_len].iov_base = (void *)(uintptr_t)start;
        batch[batch_len].iov_len = (size_t)(end - start);
        batch_len++;

        if (batch_len == MR_SELF_BATCH) {
            long moved = syscall(__NR_process_madvise, pidfd, batch, batch_len,
                                 MADV_PAGEOUT, 0u);
            if (moved > 0) {
                result.bytes_paged_out += (uint64_t)moved;
            } else {
                result.failed_batches++;
                if (moved < 0 && result.error_code == 0) {
                    result.error_code = errno;
                }
            }
            batch_len = 0;
        }
    }
    fclose(maps);

    if (batch_len > 0) {
        long moved = syscall(__NR_process_madvise, pidfd, batch, batch_len,
                             MADV_PAGEOUT, 0u);
        if (moved > 0) {
            result.bytes_paged_out += (uint64_t)moved;
        } else {
            result.failed_batches++;
            if (moved < 0 && result.error_code == 0) {
                result.error_code = errno;
            }
        }
    }
    close(pidfd);

    if (result.bytes_paged_out == 0 && result.failed_batches > 0) {
        result.status = MR_SELF_RECLAIM_FAILED;
    } else {
        result.status = MR_SELF_RECLAIM_OK;
    }
    /* A partial run is still a run: the caller reports bytes_paged_out and the
     * failed batch count rather than claiming success. */
    return result;
#endif
}

/* Zamak externs can only return scalars, so the struct is brokered through a
 * single static slot. The GUI calls run, then reads each field. Not reentrant
 * by design: there is exactly one caller and one action at a time. */
static struct mr_self_reclaim_result last_result;

int64_t mr_self_reclaim(void)
{
    last_result = mr_self_reclaim_run();
    return last_result.status;
}

int64_t mr_self_reclaim_bytes(void)
{
    return (int64_t)last_result.bytes_paged_out;
}

int64_t mr_self_reclaim_failed_batches(void)
{
    return (int64_t)last_result.failed_batches;
}

int64_t mr_self_reclaim_error(void)
{
    return (int64_t)last_result.error_code;
}
