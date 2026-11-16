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

/* Replaceable write seam.
 *
 * Every reclaim write in this tree goes through this pointer rather than
 * calling write() directly. That is what makes the refusal/guard/cancel
 * branches testable without putting pressure on a real desktop: a test
 * installs a counter, drives the pipeline into the branch it cares about, and
 * asserts that the counter did not advance. A test that had to touch the real
 * control file could only ever observe "something happened", never "nothing
 * happened" — and "no further write" is precisely what these branches promise.
 *
 * NULL means the real write(). */
typedef long (*mr_bal_write_hook)(int fd, const char *text, unsigned long length);
void mr_bal_set_write_hook(mr_bal_write_hook hook);
/* The single write entry point for this tree. balance_exec.c uses it too, so
 * every reclaim write is observable through one counter.
 *
 * Returns bytes written on success, or -1 with errno set. Callers that need to
 * classify the failure must read errno immediately, before any other call. */
ssize_t mr_bal_write(int fd, const char *text, size_t length);
/* The errno of the most recent mr_bal_write failure, or 0. Exists because the
 * write itself returns -1 and the caller cannot always read its own errno
 * reliably across an FFI boundary; classifying EAGAIN must not depend on that. */
int mr_bal_write_errno(void);
/* Forget the previous failure. A caller about to make one write that it needs
 * to classify must clear first, or it will read a failure that belongs to an
 * earlier call. */
void mr_bal_write_errno_clear(void);
/* The errno a hook wants classified, or 0 to use whatever errno the hook left.
 * Needed because the hook returns a long and the caller cannot see the errno a
 * foreign function set on its own thread state reliably. */
void mr_bal_test_set_write_errno(int value);
/* How many writes the production path attempted through the hook. */
long mr_bal_test_write_calls(void);
/* The last text handed to the hook, for order assertions. */
const char *mr_bal_test_last_write(void);
void mr_bal_test_clear_last_write(void);

/* ---- action record: the write itself, not only its errno --------------
 *
 * mr_bal_write_errno() answers "why did it fail"; these answer "what did we
 * hand the kernel, and what did it hand back". They are recorded at the same
 * single write entry point every reclaim write in this tree uses, so they can
 * never describe a write the seam did not see.
 *
 * "Not observed" is spelled -1 / 0 / "not_observed" and is a real answer: it
 * means no write reached the seam, which is exactly what a refusal upstream of
 * the write produces.
 */
/* Requested length in bytes of the most recent write, or -1 when none was
 * attempted in this process. */
long mr_bal_write_last_length(void);
/* What the seam returned: bytes written, or -1 on failure. -1 also when
 * nothing was attempted, so pair it with mr_bal_write_attempted(). */
long mr_bal_write_last_result(void);
/* 1 once any write has gone through the seam since process start. */
int mr_bal_write_attempted(void);
/* ---- bounded per-write log ------------------------------------------- *
 *
 * The record a user copies names every write the round made, so the seam keeps
 * a ring of them (MR_BAL_WRITE_LOG=64, more than the 32-batch ceiling plus the
 * probe). Entries are addressed by absolute index, monotonic for the life of
 * the process; an overwritten index answers "not observed" instead of quietly
 * returning a newer write's data.
 */
/* Total writes the seam has ever seen. Monotonic. */
long mr_bal_write_log_count(void);
/* Text / length / result / errno of absolute entry `index`. Text is "" and the
 * numbers are -1 / 0 when the index is outside the live window. */
const char *mr_bal_write_log_text(long index);
long mr_bal_write_log_length(long index);
long mr_bal_write_log_result(long index);

/* When a write returned. Monotonic milliseconds are for lining the write up
 * with a sample another process took; the wall-clock value only says when that
 * was. 0 means not observed. */
unsigned long long mr_bal_write_log_mono_ms(long index);
long long mr_bal_write_log_wall_unix(long index);
unsigned long long mr_bal_reclaim_mono_ms(void);
long long mr_bal_reclaim_wall_unix(void);
int mr_bal_write_log_errno(long index);
/* Stable class of absolute entry `index`, same names as
 * mr_bal_write_class(); "not_observed" outside the live window. */
const char *mr_bal_write_log_class(long index);

/* Stable class of the most recent write: "accepted" | "short_write" |
 * "eagain" | "einval" | "enomem" | "ebusy" | "eperm" | "eacces" | "enoent" |
 * "eio" | "failed_no_errno" | "failed_other" | "not_observed". Never NULL. */
const char *mr_bal_write_class(void);

const char *mr_bal_stop_name(int64_t stop);
const char *mr_bal_caps_name(int64_t caps);
const char *mr_bal_run_status_name(int64_t status);

#endif /* MEMREDUCT_BALANCE_RECLAIM_H */
