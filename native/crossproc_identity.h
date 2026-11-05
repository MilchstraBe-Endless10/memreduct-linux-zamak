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
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

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

static inline uint64_t mr_xproc_hash_finalize(uint64_t hash)
{
    hash &= 0x7fffffffffffffffull;
    return hash == 0u ? 1u : hash;
}

/* Separate scope from the broader identity token so audit records can prove
 * that the supported application scope remained the same without recording
 * cgroup text or command-line data. This is a change detector, not a security
 * credential; the helper independently re-reads the kernel state. */
static inline uint64_t mr_xproc_scope_token(const char *cgroup)
{
    static const unsigned char version = 1u;
    uint64_t hash = 14695981039346656037ull;
    hash = mr_xproc_identity_hash_field(hash, &version, sizeof(version));
    hash = mr_xproc_identity_hash_field(hash, cgroup, strlen(cgroup));
    return mr_xproc_hash_finalize(hash);
}

/* Stable digest of the exact eligible maps rows, shared by the read-only
 * scanner and the privileged helper. The digest covers aligned address bounds
 * plus the kernel maps metadata used to decide whether a row is eligible.
 * FNV-1a is intentionally only a stale-snapshot detector; the helper still
 * validates each row from /proc and does not trust this value as authorization. */
static inline uint64_t mr_xproc_region_digest_init(void)
{
    static const char domain[] = "memreduct-region-v1";
    uint64_t hash = 14695981039346656037ull;
    return mr_xproc_identity_hash_field(hash, domain, sizeof(domain) - 1u);
}

static inline uint64_t mr_xproc_region_digest_add(uint64_t hash,
                                                   uint64_t aligned_start,
                                                   uint64_t aligned_end,
                                                   const char *permissions,
                                                   uint64_t offset,
                                                   const char *device,
                                                   uint64_t inode,
                                                   const char *name)
{
    hash = mr_xproc_identity_hash_field(hash, &aligned_start, sizeof(aligned_start));
    hash = mr_xproc_identity_hash_field(hash, &aligned_end, sizeof(aligned_end));
    hash = mr_xproc_identity_hash_field(hash, permissions, strlen(permissions));
    hash = mr_xproc_identity_hash_field(hash, &offset, sizeof(offset));
    hash = mr_xproc_identity_hash_field(hash, device, strlen(device));
    hash = mr_xproc_identity_hash_field(hash, &inode, sizeof(inode));
    return mr_xproc_identity_hash_field(hash, name, strlen(name));
}

static inline uint64_t mr_xproc_region_digest_finish(uint64_t hash,
                                                      uint64_t region_count)
{
    hash = mr_xproc_identity_hash_field(hash, &region_count, sizeof(region_count));
    return mr_xproc_hash_finalize(hash);
}

/* Every stage logs the same non-secret request and target tuple. No raw scope,
 * executable path, or command line is recorded. Test builds route to stderr so
 * fixture runs remain local to their captured test output. */
static inline void mr_xproc_audit_emit(const char *phase,
                                       uint64_t request_id,
                                       int32_t pid,
                                       uint64_t start_time,
                                       uint64_t uid,
                                       uint64_t scope_hash,
                                       uint64_t identity_hash,
                                       uint64_t region_digest)
{
    if (phase == NULL ||
        (strcmp(phase, "selection") != 0 &&
         strcmp(phase, "confirmation") != 0 &&
         strcmp(phase, "bridge_argv") != 0 &&
         strcmp(phase, "helper_pre_syscall") != 0)) {
        return;
    }
#ifdef MEMREDUCT_TESTING
    (void)fprintf(stderr,
                  "MR_XPROC_AUDIT phase=%s request_id=%llu client_pid=%ld target_pid=%d "
                  "start_time=%llu uid=%llu scope_hash=%llu identity_hash=%llu region_digest=%llu\n",
                  phase, (unsigned long long)request_id, (long)getpid(), (int)pid,
                  (unsigned long long)start_time, (unsigned long long)uid,
                  (unsigned long long)scope_hash, (unsigned long long)identity_hash,
                  (unsigned long long)region_digest);
#else
    (void)openlog("memreduct-zamak", LOG_PID, LOG_USER);
    syslog(LOG_NOTICE,
           "xproc phase=%s request_id=%llu target_pid=%d start_time=%llu uid=%llu "
           "scope_hash=%llu identity_hash=%llu region_digest=%llu",
           phase, (unsigned long long)request_id, (int)pid,
           (unsigned long long)start_time, (unsigned long long)uid,
           (unsigned long long)scope_hash, (unsigned long long)identity_hash,
           (unsigned long long)region_digest);
    (void)closelog();
#endif
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
    return mr_xproc_hash_finalize(hash);
}

#endif /* MEMREDUCT_CROSSPROC_IDENTITY_H */
