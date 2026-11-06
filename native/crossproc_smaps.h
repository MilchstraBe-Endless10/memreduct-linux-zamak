/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Shared read-only parser for the per-mapping resident data in procfs smaps.
 * Both the unprivileged picker and privileged helper use this exact mapping
 * filter so a displayed resident-page estimate cannot use broader rules than
 * the operation itself.
 */
#ifndef MEMREDUCT_CROSSPROC_SMAPS_H
#define MEMREDUCT_CROSSPROC_SMAPS_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum mr_xproc_smaps_eligibility {
    MR_XPROC_SMAPS_ELIGIBLE = 0,
    MR_XPROC_SMAPS_SHARED = 1,
    MR_XPROC_SMAPS_UNREADABLE = 2,
    MR_XPROC_SMAPS_WRITABLE_FILE = 3,
    MR_XPROC_SMAPS_SPECIAL = 4
};

struct mr_xproc_smaps_region {
    uint64_t start;
    uint64_t end;
    uint64_t offset;
    uint64_t inode;
    char permissions[8];
    char device[32];
    char name[512];
    uint64_t private_resident_bytes;
};

typedef int (*mr_xproc_smaps_visitor)(const struct mr_xproc_smaps_region *, void *);

static inline int mr_xproc_smaps_eligibility(const struct mr_xproc_smaps_region *region)
{
    if (region->permissions[3] != 'p') return MR_XPROC_SMAPS_SHARED;
    if (region->permissions[0] != 'r') return MR_XPROC_SMAPS_UNREADABLE;
    if (region->inode != 0u && region->permissions[1] == 'w') {
        return MR_XPROC_SMAPS_WRITABLE_FILE;
    }
    if (region->name[0] == '[') return MR_XPROC_SMAPS_SPECIAL;
    return MR_XPROC_SMAPS_ELIGIBLE;
}

static inline int mr_xproc_smaps_parse_kib(const char *line, const char *key,
                                            uint64_t *value_out)
{
    size_t key_length = strlen(key);
    if (strncmp(line, key, key_length) != 0) return 0;
    const char *cursor = line + key_length;
    while (*cursor == ' ' || *cursor == '\t') cursor++;
    if (*cursor < '0' || *cursor > '9') return -1;
    uint64_t value = 0;
    do {
        unsigned int digit = (unsigned int)(*cursor - '0');
        if (value > (UINT64_MAX - digit) / 10u) return -1;
        value = value * 10u + digit;
        cursor++;
    } while (*cursor >= '0' && *cursor <= '9');
    while (*cursor == ' ' || *cursor == '\t') cursor++;
    if (cursor[0] != 'k' || cursor[1] != 'B') return -1;
    cursor += 2;
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\n') cursor++;
    if (*cursor != '\0' || value > UINT64_MAX / 1024u) return -1;
    *value_out = value * 1024u;
    return 1;
}

static inline int mr_xproc_smaps_finish_region(struct mr_xproc_smaps_region *region,
                                                int have_clean, uint64_t clean_bytes,
                                                int have_dirty, uint64_t dirty_bytes,
                                                mr_xproc_smaps_visitor visitor,
                                                void *context)
{
    if (region->end == 0u) return 0;
    if (!have_clean || !have_dirty || clean_bytes > UINT64_MAX - dirty_bytes) return -1;
    region->private_resident_bytes = clean_bytes + dirty_bytes;
    return visitor == NULL ? 0 : visitor(region, context);
}

/* The caller opens the fixed /proc/<pid>/smaps path and owns the FILE. A
 * missing/duplicate/malformed Private_* field fails closed instead of silently
 * turning an unreadable estimate into zero. */
static inline int mr_xproc_smaps_scan(FILE *stream,
                                      mr_xproc_smaps_visitor visitor,
                                      void *context)
{
    if (stream == NULL) return -1;
    struct mr_xproc_smaps_region region;
    memset(&region, 0, sizeof(region));
    int have_clean = 0, have_dirty = 0;
    uint64_t clean_bytes = 0, dirty_bytes = 0;
    char line[4096];

    while (fgets(line, sizeof(line), stream) != NULL) {
        size_t line_length = strlen(line);
        if (line_length > 0u && line[line_length - 1u] != '\n' && !feof(stream)) return -1;

        unsigned long long start = 0, end = 0, offset = 0, inode = 0;
        char permissions[8] = {0}, device[32] = {0}, name[512] = {0};
        int fields = sscanf(line, "%llx-%llx %7s %llx %31s %llu %511s",
                            &start, &end, permissions, &offset, device, &inode, name);
        if (fields >= 6) {
            if (mr_xproc_smaps_finish_region(&region, have_clean, clean_bytes,
                                              have_dirty, dirty_bytes,
                                              visitor, context) != 0 || end <= start) return -1;
            memset(&region, 0, sizeof(region));
            region.start = (uint64_t)start;
            region.end = (uint64_t)end;
            region.offset = (uint64_t)offset;
            region.inode = (uint64_t)inode;
            memcpy(region.permissions, permissions, strlen(permissions) + 1u);
            memcpy(region.device, device, strlen(device) + 1u);
            if (fields >= 7) memcpy(region.name, name, strlen(name) + 1u);
            have_clean = 0;
            have_dirty = 0;
            clean_bytes = 0;
            dirty_bytes = 0;
            continue;
        }

        if (region.end == 0u) continue;
        uint64_t parsed = 0;
        int clean = mr_xproc_smaps_parse_kib(line, "Private_Clean:", &parsed);
        if (clean < 0 || (clean > 0 && have_clean)) return -1;
        if (clean > 0) { clean_bytes = parsed; have_clean = 1; continue; }
        int dirty = mr_xproc_smaps_parse_kib(line, "Private_Dirty:", &parsed);
        if (dirty < 0 || (dirty > 0 && have_dirty)) return -1;
        if (dirty > 0) { dirty_bytes = parsed; have_dirty = 1; }
    }
    if (ferror(stream) ||
        mr_xproc_smaps_finish_region(&region, have_clean, clean_bytes,
                                      have_dirty, dirty_bytes, visitor, context) != 0) return -1;
    return 0;
}

#endif /* MEMREDUCT_CROSSPROC_SMAPS_H */
