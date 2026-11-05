/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Contract for the cross-process reclaim helper (Issue #12, M1.1).
 *
 * This header is shared by three parties and by nothing else:
 *   1. native/crossproc_helper.c  — the privileged helper itself;
 *   2. native/async_crossproc.c   — the unprivileged bridge that spawns it;
 *   3. tests/                     — Python and C regressions.
 *
 * It deliberately describes no path argument, no address range, no advice
 * number and no signal. The helper accepts one verb and seven decimal numbers,
 * and everything that decides whether memory is touched is re-derived by the
 * helper from the kernel.
 */
#ifndef MEMREDUCT_CROSSPROC_HELPER_H
#define MEMREDUCT_CROSSPROC_HELPER_H

#include <stdint.h>

/* Fixed verb. The GUI cannot ask for another action through this helper, and
 * the helper cannot be asked to do anything the page-cache helper does. */
#define MR_XPROC_VERB "reclaim"

/* Fixed install path. The Polkit action pins exactly this path; install and
 * uninstall scripts use the same literal so there is one source of truth. */
#define MR_XPROC_HELPER_PATH "/usr/libexec/memreduct-zamak-crossproc-helper"

/* Fixed Polkit action id. A distinct object from the page-cache action: a rules
 * file that allows one must not silently allow the other. */
#define MR_XPROC_ACTION_ID \
    "io.github.milchstrasse_endless10.memreduct.zamak.crossproc"

/* Ceilings. A caller may request less than these; never more. Kept small on
 * purpose: the first real target class is an ordinary user application, not an
 * entire desktop session. */
#define MR_XPROC_MAX_IOVECS 32u
#define MR_XPROC_MAX_BYTES (64ull * 1024ull * 1024ull)
/* The size the GUI offers by default for a first, conservative action. */
#define MR_XPROC_DEFAULT_BYTES (16ull * 1024ull * 1024ull)
/* The size used by the controlled first-run test against a spawned child. */
#define MR_XPROC_FIRST_RUN_BYTES (1ull * 1024ull * 1024ull)

/* Stable result states. The names never contain "freed" and never will: the
 * kernel accepts an advice request and does not report what stayed out. */
enum mr_xproc_status {
    MR_XPROC_STATUS_UNKNOWN = 0,        /* nothing conclusive; do not retry */
    MR_XPROC_STATUS_ACCEPTED = 1,       /* every requested byte was advised */
    MR_XPROC_STATUS_PARTIAL = 2,        /* some of the request was advised */
    MR_XPROC_STATUS_REFUSED = 3,        /* the helper refused before any call */
    MR_XPROC_STATUS_NO_PERMISSION = 4,  /* the kernel refused: EPERM/EACCES */
    MR_XPROC_STATUS_TARGET_GONE = 5,    /* ESRCH/EBADF: the target vanished */
    MR_XPROC_STATUS_BAD_REQUEST = 6,    /* EINVAL */
    MR_XPROC_STATUS_UNSUPPORTED = 7,    /* ENOSYS: no process_madvise here */
    MR_XPROC_STATUS_NO_REGIONS = 8      /* nothing eligible to ask about */
};

/* Exit codes. Distinct on purpose:
 *   0  the kernel accepted (fully or partly) — see status for which
 *   1  refused, denied, gone, or a bad request — see reason
 *   2  usage error: wrong argument count, wrong verb, unparseable number
 *   3  this kernel cannot do it at all
 *   4  the outcome is genuinely unknown; do not retry automatically
 *   77 the build is a dry run and will never touch another process
 * Never derive an outcome from the exit code alone: 1 covers several very
 * different situations and the reason line is what distinguishes them. */
#define MR_XPROC_EXIT_OK 0
#define MR_XPROC_EXIT_REFUSED 1
#define MR_XPROC_EXIT_USAGE 2
#define MR_XPROC_EXIT_UNSUPPORTED 3
#define MR_XPROC_EXIT_UNKNOWN 4
#define MR_XPROC_EXIT_DRY_RUN 77

/* Output contract: the helper prints an "MR_CROSSPROC_RESULT" marker line and
 * then one "key=value" line per field, plus a final
 * "bytes_advised_is_not_bytes_freed=true". Values are ASCII with no spaces.
 * Anything before the marker is not part of the result. */

/* Reason strings the bridge must expect. This list is the union of every
 * refusal the helper can produce, plus the two non-refusal reasons. It exists so
 * a test can assert that the parser handles each one and never invents a reason
 * it was not given. */
#define MR_XPROC_REASON_NONE "none"

#endif /* MEMREDUCT_CROSSPROC_HELPER_H */
