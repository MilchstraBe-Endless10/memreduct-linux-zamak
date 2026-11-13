/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File-first reclaim backend (Issue #12, balanced-clean, line B).
 *
 * Maps the Windows Mem Reduct actions onto what Linux actually offers, and
 * refuses the mappings that do not exist:
 *
 *   Windows working set trim   -> process_madvise(MADV_PAGEOUT) on a pinned
 *                                 target (crossproc_helper.c, unchanged here)
 *   Windows file cache clean   -> cgroup v2 memory.reclaim with swappiness=0
 *                                 against one exact app scope (this file)
 *   Windows standby list clean -> no Linux equivalent. Not emulated.
 *   Windows modified list WB   -> no Linux equivalent (writeback is per-page,
 *                                 not a list we can drop). Not emulated.
 *
 * The global /proc/sys/vm/drop_caches is deliberately NOT one of these. It
 * drops every cache on the machine including other users' and the page cache
 * the kernel is actively using, which is the opposite of "balanced".
 *
 * Two hard rules this file exists to enforce:
 *   1. swappiness=0 support is detected, never assumed. When the kernel
 *      rejects it the answer is "unsupported", never a silent retry with the
 *      plain byte count, because that plain form may push anonymous pages out.
 *   2. Once a run stops, no further batch is issued. The stop is sticky.
 */
#ifndef MEMREDUCT_BALANCE_RECLAIM_H
#define MEMREDUCT_BALANCE_RECLAIM_H

#include <stdint.h>

/* How the kernel answered the swappiness question for this scope. */
enum mr_bal_caps {
    MR_BAL_CAPS_UNKNOWN = 0,
    MR_BAL_CAPS_FILE_ONLY = 1,     /* "N swappiness=0" accepted: file pages only */
    MR_BAL_CAPS_NO_SWAPPINESS = 2, /* kernel/scope rejects the parameter */
    MR_BAL_CAPS_UNAVAILABLE = 3    /* no memory.reclaim at all */
};

/* Why a run stopped. Kept local so this backend stays independent of the
 * planner's enum: the two files are developed and reviewed separately and the
 * integrator joins them, so a shared enum would couple the branches. */
enum mr_bal_stop {
    MR_BAL_STOP_NONE = 0,
    MR_BAL_STOP_NO_COLD_PAGES = 1,
    MR_BAL_STOP_STALL_RISING = 2,
    MR_BAL_STOP_UNSUPPORTED = 3,
    MR_BAL_STOP_ERROR = 4
};

enum mr_bal_run_status {
    MR_BAL_RUN_OK = 0,
    MR_BAL_RUN_PARTIAL = 1,       /* some batches accepted, then we stopped */
    MR_BAL_RUN_STOPPED = 2,       /* a stop condition fired before any batch */
    MR_BAL_RUN_UNSUPPORTED = 3,   /* file-first cannot be done safely here */
    MR_BAL_RUN_NO_SCOPE = 4,      /* the scope fd is not a usable cgroup */
    MR_BAL_RUN_BAD_REQUEST = 5    /* caller asked for something unbounded */
};

struct mr_bal_run_config {
    /* Total request budget for this run. Never a promise of freed memory. */
    uint64_t budget_bytes;
    /* Per-batch size. Small batches are the whole point: a single 512 MiB
     * write is one large synchronous reclaim, N small ones can stop early. */
    uint64_t batch_bytes;
    uint32_t max_batches;
    /* Stop when PSI some-rate exceeds this, in hundredths of a percent. */
    uint32_t psi_stop_centi;
    /* Stop when this many bytes of the budget were accepted. */
    int32_t stop_on_budget;
};

struct mr_bal_run_result {
    int64_t status;
    int64_t caps;
    uint64_t requested_bytes;  /* what we asked the kernel to consider */
    uint64_t accepted_bytes;   /* what the kernel answered per batch, summed */
    uint32_t batches_attempted;
    uint32_t batches_accepted;
    uint32_t batches_rejected;
    int32_t error_code;        /* first errno, 0 when none */
    int32_t stopped_early;     /* 1 when a stop condition fired */
    int64_t stop_reason;       /* enum mr_bal_stop value describing the stop */
};

/* Probe the scope read-only: does memory.reclaim exist, and does it accept the
 * swappiness parameter? The probe writes zero bytes of request (a 0-byte write
 * is the kernel's own no-op) so it does not reclaim anything. */
int64_t mr_bal_caps_probe(int scope_fd, enum mr_bal_caps *out);

/* Fill the default run configuration for a requested byte budget. */
void mr_bal_run_config_for_budget(uint64_t request_bytes,
                                  struct mr_bal_run_config *out);

/* Run the batches. scope_fd must be an O_RDONLY directory fd of the exact
 * cgroup; this function opens memory.reclaim relative to it with O_NOFOLLOW
 * and never constructs a path from a caller string. */
int mr_bal_reclaim_run(int scope_fd, const struct mr_bal_run_config *config,
                       struct mr_bal_run_result *out);

#ifdef MEMREDUCT_TESTING
/* Test-only write hook: replaces the single write() so a fixture can answer
 * accept / EAGAIN / EPERM / short-write without a real control file. The hook
 * receives the exact text the production path would have written, so a test
 * can also assert the parameter order. Never compiled into a release build. */
typedef long (*mr_bal_write_hook)(int fd, const char *text, unsigned long length);
void mr_bal_set_write_hook(mr_bal_write_hook hook);
/* The errno a hook wants classified, or 0 to use whatever errno the hook left.
 * A ctypes callback cannot reliably set the C errno across the trampoline
 * (measured: use_errno=True still arrived as 0 on this build), so the hook
 * reports it through this channel instead. The production path never uses it. */
void mr_bal_test_set_write_errno(int value);
int mr_bal_test_write_errno(void);
/* How many writes the production path attempted through the hook. */
int64_t mr_bal_test_write_calls(void);
/* The last text handed to the hook, for order assertions. */
const char *mr_bal_test_last_write(void);
void mr_bal_test_clear_last_write(void);
#endif

const char *mr_bal_stop_name(int64_t stop);
const char *mr_bal_caps_name(int64_t caps);
const char *mr_bal_run_status_name(int64_t status);

#endif /* MEMREDUCT_BALANCE_RECLAIM_H */
