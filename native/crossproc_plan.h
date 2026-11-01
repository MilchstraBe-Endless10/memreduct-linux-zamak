/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Read-only planning/discovery for a bounded, unprivileged cross-process
 * workingset advisory prototype (Issue #12, ADR-0007).
 *
 * This header is the whole caller-facing contract. It deliberately has no way
 * to name a PID, a path or an address range from the caller: the target set is
 * produced by a fixed scan policy over procfs, and the only inputs are budget
 * limits. A writable action lives in crossproc_advise.h and is compile-time
 * and approval gated; nothing here changes memory.
 */
#ifndef MEMREDUCT_CROSSPROC_PLAN_H
#define MEMREDUCT_CROSSPROC_PLAN_H

#include <stddef.h>
#include <stdint.h>

/* Fixed action enum. The planner never accepts an arbitrary advice number. */
enum mr_crossproc_advice {
    MR_CROSSPROC_ADVICE_NONE = 0,
    MR_CROSSPROC_ADVICE_COLD = 1,    /* MADV_COLD: deactivate, keep resident */
    MR_CROSSPROC_ADVICE_PAGEOUT = 2  /* MADV_PAGEOUT: may force writeback + swap */
};

/* Stable result states. Never "freed N bytes": the kernel accepts an advice
 * request and does not report how much of it stayed out. */
enum mr_crossproc_status {
    MR_CROSSPROC_OK = 0,
    MR_CROSSPROC_UNSUPPORTED = 1,      /* pidfd_open / process_madvise absent */
    MR_CROSSPROC_PROC_UNAVAILABLE = 2, /* procfs root unreadable */
    MR_CROSSPROC_BUDGET_EXHAUSTED = 3, /* stopped by a limit, plan still usable */
    MR_CROSSPROC_PERMISSION_DENIED = 4,/* caller lacks the authority it needs */
    MR_CROSSPROC_NO_CANDIDATES = 5,    /* scan found nothing eligible */
    MR_CROSSPROC_INVALID_ARGUMENT = 6,
    MR_CROSSPROC_REENTRANT = 7         /* a plan is already being built */
};

/* Why a discovered process or mapping was not planned. Each is reported, never
 * silently dropped, so the GUI can say what was left alone. */
enum mr_crossproc_skip {
    MR_CROSSPROC_SKIP_NONE = 0,
    MR_CROSSPROC_SKIP_PID1 = 1,
    MR_CROSSPROC_SKIP_SELF = 2,
    MR_CROSSPROC_SKIP_KERNEL_THREAD = 3,
    MR_CROSSPROC_SKIP_OTHER_UID = 4,
    MR_CROSSPROC_SKIP_EXITED = 5,
    MR_CROSSPROC_SKIP_PID_REUSED = 6,
    MR_CROSSPROC_SKIP_SHARED = 7,
    MR_CROSSPROC_SKIP_WRITABLE = 8,    /* file-backed writable: dirty pages */
    MR_CROSSPROC_SKIP_SPECIAL = 9,     /* vdso/vvar/stack: not adviseable */
    MR_CROSSPROC_SKIP_TOO_LARGE = 10,  /* single region over the byte budget */
    MR_CROSSPROC_SKIP_BUDGET = 11,     /* target/region/budget already spent */
    MR_CROSSPROC_SKIP_UNREADABLE = 12, /* procfs entry disappeared or unreadable */
    MR_CROSSPROC_SKIP_SYSTEM = 13      /* reserved: never advise system mappings */
};

/* Hard ceilings. They are compile-time constants, not caller arguments, so an
 * external caller cannot ask for an unbounded global sweep. */
#define MR_CROSSPROC_MAX_TARGETS 8u
#define MR_CROSSPROC_MAX_REGIONS 16u      /* per target */
#define MR_CROSSPROC_MAX_REGION_BYTES (64ull * 1024ull * 1024ull)
#define MR_CROSSPROC_MAX_TOTAL_BYTES (256ull * 1024ull * 1024ull)
#define MR_CROSSPROC_MAX_IOVECS 32u       /* one process_madvise batch */
#define MR_CROSSPROC_TIME_BUDGET_MS 250u
#define MR_CROSSPROC_MAX_PROC_ENTRIES 256u /* bounded /proc readdir, never a sweep */
/* Room for "<proc_root>/<pid>/stat". The root is an absolute path and a test
 * fixture tree alone can exceed 64 bytes, so this must not be a short buffer:
 * snprintf() would truncate, every open() would fail, and every process would
 * be classified unreadable while the code still looked correct. */
#define MR_CROSSPROC_PATH_BYTES 4096u

struct mr_crossproc_limits {
    uint32_t max_targets;    /* clamped to MR_CROSSPROC_MAX_TARGETS */
    uint32_t max_regions;    /* per target, clamped to MR_CROSSPROC_MAX_REGIONS */
    uint64_t max_total_bytes;
    uint32_t budget_ms;      /* clamped to MR_CROSSPROC_TIME_BUDGET_MS */
    int32_t advice;          /* enum mr_crossproc_advice; only COLD/PAGEOUT */
};

/* One planned range. bytes is the mapping length the kernel would be asked
 * about, not an amount of memory that will be released. */
struct mr_crossproc_region {
    uint64_t start;
    uint64_t end;
    int32_t advice;
    int32_t reserved;
};

/* A planned target, pinned by pidfd. start_time is /proc/<pid>/stat field 22;
 * a second read that differs means the pid was reused and the plan is void. */
struct mr_crossproc_target {
    int32_t pid;
    int32_t pidfd;
    uint64_t start_time;
    uint32_t region_offset; /* index into the plan's region array */
    uint32_t region_count;
    uint64_t bytes;
};

struct mr_crossproc_plan {
    int64_t status;
    int32_t advice;
    int32_t error_code;
    uint32_t targets;
    uint32_t regions;
    uint64_t bytes_planned;   /* sum of mapping lengths, not freed bytes */
    uint32_t scanned;         /* procfs pid entries examined (bounded) */
    uint32_t skipped_pid1;
    uint32_t skipped_self;
    uint32_t skipped_kernel_thread;
    uint32_t skipped_other_uid;
    uint32_t skipped_exited;
    uint32_t skipped_pid_reused;
    uint32_t skipped_shared;
    uint32_t skipped_writable;
    uint32_t skipped_special;
    uint32_t skipped_too_large;
    uint32_t skipped_budget;
    uint32_t skipped_unreadable;
    struct mr_crossproc_target entries[MR_CROSSPROC_MAX_TARGETS];
    /* Flat store of the planned ranges; target.region_offset indexes it.
     * Named separately from the `regions` counter above: one is a total, the
     * other is storage, and conflating them silently corrupts the plan. */
    struct mr_crossproc_region region_store[MR_CROSSPROC_MAX_TARGETS * MR_CROSSPROC_MAX_REGIONS];
};

/* Build a plan. Read-only: opens pidfds, re-reads maps, allocates nothing. */
int mr_crossproc_plan_build(const struct mr_crossproc_limits *limits,
                            struct mr_crossproc_plan *plan);

/* Dispose of the pidfds held by a finished plan (never touches the targets). */
void mr_crossproc_plan_release(struct mr_crossproc_plan *plan);

/* Total regions across targets, for callers that only need the count. */
uint32_t mr_crossproc_plan_region_count(const struct mr_crossproc_plan *plan);

/* Copy region i of target t. Returns 0 on success, -1 when out of range. */
int mr_crossproc_plan_region_at(const struct mr_crossproc_plan *plan,
                                uint32_t target_index, uint32_t region_index,
                                struct mr_crossproc_region *out);

/* Scalar bridge for Zamak: run once, then read the fields of that same run. */
int64_t mr_crossproc_plan_status(void);
int64_t mr_crossproc_plan_targets(void);
int64_t mr_crossproc_plan_regions(void);
int64_t mr_crossproc_plan_bytes(void);
int64_t mr_crossproc_plan_error(void);
/* Per-reason skip counts for the same run, so a status is explainable.
 * reason is a mr_crossproc_skip value; -1 means "unknown reason". */
int64_t mr_crossproc_plan_skipped(int64_t reason);

#ifdef MEMREDUCT_TESTING
/* Test-only injection point: replaces pidfd_open so a fixture tree of fake PIDs
 * can be pinned without any of those PIDs existing on the host. The hook must
 * return a descriptor that close(2) accepts, or -1 with errno set. Never
 * compiled into a production build. */
typedef int (*mr_crossproc_pidfd_hook)(int32_t pid);
void mr_crossproc_set_pidfd_hook(mr_crossproc_pidfd_hook hook);
/* How many times the hook was consulted; lets a test prove the planner only
 * opens pidfds for processes it already accepted. */
int32_t mr_crossproc_test_pidfd_calls(void);

/* Test-only injection point for the ownership question. A fixture tree cannot
 * be chowned to another user without privileges, so this answers the same
 * question the planner's single stat() answers: owner uid of pid, or -1 when it
 * cannot be determined. Never compiled into a production build. */
typedef int32_t (*mr_crossproc_uid_hook)(int32_t pid);
void mr_crossproc_set_uid_hook(mr_crossproc_uid_hook hook);

/* Inspection of an arbitrary plan, bypassing the single static bridge slot. */
int64_t mr_crossproc_plan_status_of(const struct mr_crossproc_plan *plan);
int64_t mr_crossproc_plan_targets_of(const struct mr_crossproc_plan *plan);
int64_t mr_crossproc_plan_regions_of(const struct mr_crossproc_plan *plan);
int64_t mr_crossproc_plan_bytes_of(const struct mr_crossproc_plan *plan);
int64_t mr_crossproc_plan_scanned_of(const struct mr_crossproc_plan *plan);
int64_t mr_crossproc_plan_skipped_of(const struct mr_crossproc_plan *plan, int64_t reason);
int64_t mr_crossproc_plan_region_start(const struct mr_crossproc_plan *plan,
                                       uint32_t target_index, uint32_t region_index);
int64_t mr_crossproc_plan_region_end(const struct mr_crossproc_plan *plan,
                                     uint32_t target_index, uint32_t region_index);
int64_t mr_crossproc_plan_target_pid(const struct mr_crossproc_plan *plan, uint32_t index);
#endif

#endif /* MEMREDUCT_CROSSPROC_PLAN_H */
