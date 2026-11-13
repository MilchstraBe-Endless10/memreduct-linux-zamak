/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Read-only memory pressure sampler (Issue #12, balanced-clean, line A).
 *
 * This translation unit opens files read-only and never writes anywhere. It
 * exists so the planner can distinguish four states that a single "used %"
 * number conflates: healthy cache, reclaimable cold file pages, an oversized
 * anonymous active set, and continuous swapping.
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "balance_snapshot.h"

static int read_file(const char *path, char *buf, size_t cap)
{
    FILE *fh = fopen(path, "r");
    if (fh == NULL) return -1;
    size_t got = fread(buf, 1, cap - 1, fh);
    int failed = ferror(fh);
    fclose(fh);
    if (failed) return -1;
    buf[got] = '\0';
    return 0;
}

/* Parse "<key> <value>" and return 1 when the key matched. */
static int line_key_u64(const char *line, const char *key, uint64_t *out)
{
    size_t klen = strlen(key);
    if (strncmp(line, key, klen) != 0) return 0;
    char sep = line[klen];
    if (sep != ' ' && sep != '\t' && sep != ':') return 0;
    const char *p = line + klen;
    while (*p == ' ' || *p == '\t' || *p == ':') p++;
    uint64_t value = 0;
    int digits = 0;
    while (*p >= '0' && *p <= '9') {
        if (value > (UINT64_MAX - 9u) / 10u) return 0;
        value = value * 10u + (uint64_t)(*p - '0');
        p++;
        digits++;
    }
    if (digits == 0) return 0;
    *out = value;
    return 1;
}

/* meminfo values are in kilobytes; convert to bytes with saturation. */
static uint64_t kib_to_bytes(uint64_t kib)
{
    if (kib > UINT64_MAX / 1024u) return UINT64_MAX;
    return kib * 1024u;
}

static int parse_meminfo(const char *text, struct mr_bal_snapshot *out)
{
    /* Every field is optional at parse time; a missing one stays 0 and the
     * caller learns about it through the MemAvailable==0 cross-check below. */
    const struct {
        const char *key;
        size_t offset;
    } fields[] = {
        {"MemTotal", offsetof(struct mr_bal_snapshot, total)},
        {"MemFree", offsetof(struct mr_bal_snapshot, free_ram)},
        {"MemAvailable", offsetof(struct mr_bal_snapshot, available)},
        {"Cached", offsetof(struct mr_bal_snapshot, cached)},
        {"SReclaimable", offsetof(struct mr_bal_snapshot, sreclaimable)},
        {"Active(anon)", offsetof(struct mr_bal_snapshot, active_anon)},
        {"Inactive(anon)", offsetof(struct mr_bal_snapshot, inactive_anon)},
        {"Active(file)", offsetof(struct mr_bal_snapshot, active_file)},
        {"Inactive(file)", offsetof(struct mr_bal_snapshot, inactive_file)},
        {"Dirty", offsetof(struct mr_bal_snapshot, dirty)},
        {"Writeback", offsetof(struct mr_bal_snapshot, writeback)},
        {"SwapTotal", offsetof(struct mr_bal_snapshot, swap_total)},
        {"SwapFree", offsetof(struct mr_bal_snapshot, swap_free)}
    };
    int saw_available = 0;
    const char *line = text;
    while (*line != '\0') {
        const char *eol = strchr(line, '\n');
        size_t len = eol == NULL ? strlen(line) : (size_t)(eol - line);
        char buf[128];
        if (len < sizeof(buf)) {
            memcpy(buf, line, len);
            buf[len] = '\0';
            for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
                uint64_t value = 0;
                if (!line_key_u64(buf, fields[i].key, &value)) continue;
                *(uint64_t *)((char *)out + fields[i].offset) = kib_to_bytes(value);
                if (strcmp(fields[i].key, "MemAvailable") == 0) saw_available = 1;
                break;
            }
        }
        if (eol == NULL) break;
        line = eol + 1;
    }
    return saw_available ? 0 : -1;
}

/* /proc/pressure/memory:
 *   some avg10=0.90 avg60=1.16 avg300=1.15 total=1149123577
 *   full avg10=0.75 ...
 * Averages are percentages with two decimals; store hundredths so the policy
 * can compare integers and never touch floating point. */
static void parse_psi(const char *text, struct mr_bal_snapshot *out)
{
    const char *line = text;
    while (*line != '\0') {
        const char *eol = strchr(line, '\n');
        size_t len = eol == NULL ? strlen(line) : (size_t)(eol - line);
        char buf[192];
        if (len < sizeof(buf)) {
            memcpy(buf, line, len);
            buf[len] = '\0';
            int is_full = strncmp(buf, "full ", 5) == 0;
            int is_some = strncmp(buf, "some ", 5) == 0;
            if (is_full || is_some) {
                const char *avg = strstr(buf, "avg10=");
                const char *tot = strstr(buf, "total=");
                if (avg != NULL) {
                    avg += 6;
                    uint32_t whole = 0, frac = 0;
                    while (*avg >= '0' && *avg <= '9') { whole = whole * 10u + (uint32_t)(*avg - '0'); avg++; }
                    if (*avg == '.') {
                        avg++;
                        int digits = 0;
                        while (*avg >= '0' && *avg <= '9' && digits < 2) {
                            frac = frac * 10u + (uint32_t)(*avg - '0');
                            avg++;
                            digits++;
                        }
                        while (digits < 2) { frac *= 10u; digits++; }
                    }
                    uint32_t centi = whole * 100u + frac;
                    if (is_full) out->psi_full_avg10_centi = centi;
                    else out->psi_some_avg10_centi = centi;
                }
                if (tot != NULL) {
                    tot += 6;
                    uint64_t value = 0;
                    while (*tot >= '0' && *tot <= '9') { value = value * 10u + (uint64_t)(*tot - '0'); tot++; }
                    if (is_full) out->psi_full_total_us = value;
                    else out->psi_some_total_us = value;
                }
            }
        }
        if (eol == NULL) break;
        line = eol + 1;
    }
}

static void parse_vmstat(const char *text, struct mr_bal_snapshot *out)
{
    const struct {
        const char *key;
        size_t offset;
    } fields[] = {
        {"pswpin", offsetof(struct mr_bal_snapshot, pswpin)},
        {"pswpout", offsetof(struct mr_bal_snapshot, pswpout)},
        {"pgmajfault", offsetof(struct mr_bal_snapshot, pgmajfault)},
        {"workingset_refault_anon", offsetof(struct mr_bal_snapshot, refault_anon)},
        {"workingset_refault_file", offsetof(struct mr_bal_snapshot, refault_file)},
        {"pgscan_direct", offsetof(struct mr_bal_snapshot, pgscan_direct)},
        {"pgsteal_direct", offsetof(struct mr_bal_snapshot, pgsteal_direct)}
    };
    const char *line = text;
    while (*line != '\0') {
        const char *eol = strchr(line, '\n');
        size_t len = eol == NULL ? strlen(line) : (size_t)(eol - line);
        char buf[128];
        if (len < sizeof(buf)) {
            memcpy(buf, line, len);
            buf[len] = '\0';
            for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
                uint64_t value = 0;
                if (line_key_u64(buf, fields[i].key, &value))
                    *(uint64_t *)((char *)out + fields[i].offset) = value;
            }
        }
        if (eol == NULL) break;
        line = eol + 1;
    }
}

/* /proc/swaps:
 *   Filename   Type      Size       Used   Priority
 *   /dev/zram0 partition 16777212   0      100
 * Sizes are KiB. ZRAM is detected by device name, and its real cost read from
 * /sys/block/<dev>/ since a full zram is not the same congestion as a full disk. */
static void parse_swaps(const char *text, struct mr_bal_snapshot *out, const char *sys_root)
{
    const char *line = strchr(text, '\n'); /* skip the header */
    if (line == NULL) return;
    line++;
    while (*line != '\0' && out->swap_devices < MR_BAL_MAX_SWAP_DEVICES) {
        const char *eol = strchr(line, '\n');
        size_t len = eol == NULL ? strlen(line) : (size_t)(eol - line);
        char buf[320];
        if (len < sizeof(buf)) {
            memcpy(buf, line, len);
            buf[len] = '\0';
            char name[MR_BAL_PATH_BYTES];
            char type[32];
            unsigned long long size_kib = 0, used_kib = 0;
            if (sscanf(buf, "%255s %31s %llu %llu", name, type, &size_kib, &used_kib) == 4) {
                uint32_t idx = out->swap_devices;
                memset(&out->swap[idx], 0, sizeof(out->swap[idx]));
                snprintf(out->swap[idx].name, sizeof(out->swap[idx].name), "%s", name);
                out->swap[idx].size = kib_to_bytes((uint64_t)size_kib);
                out->swap[idx].used = kib_to_bytes((uint64_t)used_kib);
                const char *base = strrchr(name, '/');
                base = base == NULL ? name : base + 1;
                if (strncmp(base, "zram", 4) == 0) {
                    out->swap[idx].is_zram = 1;
                    char path[MR_BAL_PATH_BYTES * 2];
                    char raw[64];
                    snprintf(path, sizeof(path), "%s/block/%s/orig_data_size", sys_root, base);
                    if (read_file(path, raw, sizeof(raw)) == 0)
                        out->swap[idx].zram_orig_data_size = strtoull(raw, NULL, 10);
                    snprintf(path, sizeof(path), "%s/block/%s/mem_used_total", sys_root, base);
                    if (read_file(path, raw, sizeof(raw)) == 0)
                        out->swap[idx].zram_mem_used = strtoull(raw, NULL, 10);
                }
                out->swap_devices++;
            }
        }
        if (eol == NULL) break;
        line = eol + 1;
    }
}

int mr_bal_snapshot_read(const char *proc_root, const char *sys_root,
                         struct mr_bal_snapshot *out)
{
    if (out == NULL) return -1;
    memset(out, 0, sizeof(*out));
    const char *proc = proc_root == NULL ? "/proc" : proc_root;
    const char *sys = sys_root == NULL ? "/sys" : sys_root;

    char path[MR_BAL_PATH_BYTES * 2];
    /* 64 KiB covers meminfo and vmstat comfortably on any current kernel. */
    size_t cap = 65536;
    char *buf = malloc(cap);
    if (buf == NULL) return -1;

    snprintf(path, sizeof(path), "%s/meminfo", proc);
    if (read_file(path, buf, cap) != 0 || parse_meminfo(buf, out) != 0)
        out->missing |= MR_BAL_MISSING_MEMINFO;

    snprintf(path, sizeof(path), "%s/pressure/memory", proc);
    if (read_file(path, buf, cap) != 0) out->missing |= MR_BAL_MISSING_PSI;
    else parse_psi(buf, out);

    snprintf(path, sizeof(path), "%s/vmstat", proc);
    if (read_file(path, buf, cap) != 0) out->missing |= MR_BAL_MISSING_VMSTAT;
    else parse_vmstat(buf, out);

    snprintf(path, sizeof(path), "%s/swaps", proc);
    if (read_file(path, buf, cap) != 0) out->missing |= MR_BAL_MISSING_SWAPS;
    else parse_swaps(buf, out, sys);

    free(buf);
    return 0;
}

static uint64_t delta_u64(uint64_t before, uint64_t after)
{
    return after >= before ? after - before : 0;
}

int mr_bal_delta_between(const struct mr_bal_snapshot *before,
                         const struct mr_bal_snapshot *after,
                         uint64_t elapsed_ms,
                         struct mr_bal_delta *out)
{
    if (before == NULL || after == NULL || out == NULL) return -1;
    memset(out, 0, sizeof(*out));
    out->elapsed_ms = elapsed_ms;
    out->psi_some_us = delta_u64(before->psi_some_total_us, after->psi_some_total_us);
    out->psi_full_us = delta_u64(before->psi_full_total_us, after->psi_full_total_us);
    out->pswpin = delta_u64(before->pswpin, after->pswpin);
    out->pswpout = delta_u64(before->pswpout, after->pswpout);
    out->refault_anon = delta_u64(before->refault_anon, after->refault_anon);
    out->refault_file = delta_u64(before->refault_file, after->refault_file);
    /* available is signed: it can drop below the previous reading. */
    if (after->available >= before->available)
        out->available = (int64_t)(after->available - before->available);
    else
        out->available = -(int64_t)(before->available - after->available);
    return 0;
}

/* PSI stall time as a percentage of wall time, in hundredths of a percent.
 * 100% some = the denominator is fully stalled in at least one task. */
uint64_t mr_bal_psi_some_rate_centi(const struct mr_bal_delta *delta)
{
    if (delta == NULL || delta->elapsed_ms == 0) return 0;
    /* us stalled / (elapsed_ms * 1000 us) * 10000 centi-percent */
    uint64_t numer = delta->psi_some_us;
    uint64_t denom = delta->elapsed_ms;
    if (numer > UINT64_MAX / 10000u) return UINT64_MAX;
    return (numer * 10000u) / (denom * 1000u == 0 ? 1u : denom * 1000u);
}

uint64_t mr_bal_psi_full_rate_centi(const struct mr_bal_delta *delta)
{
    if (delta == NULL || delta->elapsed_ms == 0) return 0;
    uint64_t numer = delta->psi_full_us;
    if (numer > UINT64_MAX / 10000u) return UINT64_MAX;
    return (numer * 10000u) / (delta->elapsed_ms * 1000u);
}
