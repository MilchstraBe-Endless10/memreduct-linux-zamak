/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Stable, non-secret selection fingerprint shared by the unprivileged scanner
 * and privileged helper. This detects stale UI selections and process
 * replacement across the Polkit prompt. It is not an authorization token: the
 * helper still applies its independent UID, executable, cgroup, pidfd and
 * capability checks.
 */
#ifndef MEMREDUCT_CROSSPROC_IDENTITY_H
#define MEMREDUCT_CROSSPROC_IDENTITY_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static inline uint64_t mr_xproc_identity_hash_bytes(uint64_t hash,
                                                   const void *data,
                                                   size_t length)
{
    const unsigned char *bytes = (const unsigned char *)data;
    for (size_t index = 0; index < length; index++) {
        hash ^= (uint64_t)bytes[index];
        hash *= 1099511628211ull;
    }
    return hash;
}

static inline uint64_t mr_xproc_identity_hash_field(uint64_t hash,
                                                    const void *data,
                                                    size_t length)
{
    /* Length-prefix every field so concatenation boundaries are unambiguous. */
    uint64_t field_length = (uint64_t)length;
    hash = mr_xproc_identity_hash_bytes(hash, &field_length, sizeof(field_length));
    return mr_xproc_identity_hash_bytes(hash, data, length);
}

static inline uint64_t mr_xproc_identity_token(int32_t pid,
                                               uint64_t start_time,
                                               uint64_t uid,
                                               const char *comm,
                                               const char *exe,
                                               const char *cgroup,
                                               const void *cmdline,
                                               size_t cmdline_length)
{
    static const unsigned char version = 1u;
    uint64_t hash = 14695981039346656037ull;
    hash = mr_xproc_identity_hash_field(hash, &version, sizeof(version));
    hash = mr_xproc_identity_hash_field(hash, &pid, sizeof(pid));
    hash = mr_xproc_identity_hash_field(hash, &start_time, sizeof(start_time));
    hash = mr_xproc_identity_hash_field(hash, &uid, sizeof(uid));
    hash = mr_xproc_identity_hash_field(hash, comm, strlen(comm));
    hash = mr_xproc_identity_hash_field(hash, exe, strlen(exe));
    hash = mr_xproc_identity_hash_field(hash, cgroup, strlen(cgroup));
    hash = mr_xproc_identity_hash_field(hash, cmdline, cmdline_length);
    /* Keep the decimal argv form inside signed 64-bit range for Zamak FFI. */
    hash &= 0x7fffffffffffffffull;
    return hash == 0u ? 1u : hash;
}

#endif /* MEMREDUCT_CROSSPROC_IDENTITY_H */
