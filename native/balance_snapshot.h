/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Read-only memory pressure snapshot for the balanced reclaim policy
 * (Issue #12, balanced-clean). Nothing in this header writes to the kernel.
 *
 * The point of the snapshot is to answer a question a single MemAvailable
 * number cannot: is this machine healthy, or is it thrashing, or does it
 * merely have a cold file-page cache worth dropping? Those are different
 * states and they imply different actions, including "do nothing".
 *
 * Sources, all read-only:
 *   /proc/meminfo      MemTotal, MemFree, MemAvailable, Cached, SReclaimable,
 *                      Active/Inactive anon and file, SwapTotal/SwapFree
 *   /proc/pressure/memory  "some"/"full" total microseconds (PSI)
 *   /proc/vmstat       pswpin/pswpout, pgscan/pgsteal, workingset_refault_*,
 *                      pgmajfault
 *   /proc/swaps        per-device swap size and used
 *   /sys/block/<dev>/   zram capacity, so a full ZRAM is not read as full swap
 */
#ifndef MEMREDUCT_BALANCE_SNAPSHOT_H
#define MEMREDUCT_BALANCE_SNAPSHOT_H

#include <stddef.h>
#include <stdint.h>

#define MR_BAL_MAX_SWAP_DEVICES 8u
#define MR_BAL_PATH_BYTES 256u

/* Every field is bytes or a raw kernel counter. No derived "freeable" number
 * lives here: the planner decides, and it says which evidence it used. */
struct mr_bal_snapshot {
    uint64_t total;
    uint64_t free_ram;
    uint64_t available;
    uint64_t cached;
    uint64_t sreclaimable;
    uint64_t active_anon;
    uint64_t inactive_anon;
    uint64_t active_file;
    uint64_t inactive_file;
    uint64_t dirty;
    uint64_t writeback;
    uint64_t swap_total;
    uint64_t swap_free;
    /* PSI, cumulative microseconds and the three averages in hundredths of a
     * percent, so a policy can use either an absolute or a rate threshold. */
    uint64_t psi_some_total_us;
    uint64_t psi_full_total_us;
    uint32_t psi_some_avg10_centi;
    uint32_t psi_full_avg10_centi;
    /* vmstat counters, only the ones the classification actually uses. */
    uint64_t pswpin;
    uint64_t pswpout;
    uint64_t pgmajfault;
    uint64_t refault_anon;
    uint64_t refault_file;
    uint64_t pgscan_direct;
    uint64_t pgsteal_direct;
    /* Swap devices. zram_* are the same device's compressed footprint. */
    uint32_t swap_devices;
    struct {
        char name[MR_BAL_PATH_BYTES];
        uint64_t size;
        uint64_t used;
        int32_t is_zram;
        uint64_t zram_orig_data_size;
        uint64_t zram_mem_used;
    } swap[MR_BAL_MAX_SWAP_DEVICES];
    uint32_t missing;  /* bitwise: which sources could not be read */
};

/* Bit positions for mr_bal_snapshot.missing. A missing source is reported, not
 * silently zeroed: a policy that cannot see PSI must not conclude "no pressure". */
enum mr_bal_missing {
    MR_BAL_MISSING_MEMINFO = 1u << 0,
    MR_BAL_MISSING_PSI = 1u << 1,
    MR_BAL_MISSING_VMSTAT = 1u << 2,
    MR_BAL_MISSING_SWAPS = 1u << 3
};

/* Read one snapshot from the given roots. proc_root and sys_root exist so a
 * test can point at a fixture tree; production passes "/proc" and "/sys". */
int mr_bal_snapshot_read(const char *proc_root, const char *sys_root,
                         struct mr_bal_snapshot *out);

/* Two snapshots of the same machine, for rate-based decisions. */
struct mr_bal_delta {
    uint64_t elapsed_ms;
    uint64_t psi_some_us;
    uint64_t psi_full_us;
    uint64_t pswpin;
    uint64_t pswpout;
    uint64_t refault_anon;
    uint64_t refault_file;
    int64_t available;
};

int mr_bal_delta_between(const struct mr_bal_snapshot *before,
                         const struct mr_bal_snapshot *after,
                         uint64_t elapsed_ms,
                         struct mr_bal_delta *out);

/* Rate helpers, all integer, all saturating at UINT64_MAX. */
uint64_t mr_bal_psi_some_rate_centi(const struct mr_bal_delta *delta);
uint64_t mr_bal_psi_full_rate_centi(const struct mr_bal_delta *delta);

#endif /* MEMREDUCT_BALANCE_SNAPSHOT_H */
