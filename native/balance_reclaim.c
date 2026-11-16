/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * File-first reclaim backend (Issue #12, balanced-clean, line B).
 *
 * Measured on the target host (kernel 7.2.15-1-omarchy, cgroup v2) with a
 * throwaway child cgroup holding ~600 MiB anon and ~0 MiB file cache:
 *
 *   write "128M swappiness=0"  -> EAGAIN, 0 bytes reclaimed, anon unchanged
 *   write "128M"               -> accepted, 128 MiB reclaimed, ALL from anon
 *   write "128M swappiness=200"-> accepted, 128 MiB reclaimed, ALL from anon
 *
 * That is the whole safety argument for this file. The plain byte count does
 * not mean "reclaim cache": on this kernel it means "take whatever is cheapest",
 * and with no file pages present that is the anonymous working set, pushed to
 * swap. swappiness=0 is what restricts it to file pages, and when no file page
 * qualifies the kernel answers EAGAIN instead of silently taking anon.
 *
 * Consequences enforced here:
 *   - every write carries swappiness=0; there is no code path that writes a
 *     bare byte count, not even as a fallback after a rejection;
 *   - EAGAIN is reported as "no eligible file pages", never retried, never
 *     counted as a partial success;
 *   - a stop condition latches: once set, no further batch is issued.
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "balance_reclaim.h"

#define MIB (1024ull * 1024ull)

/* The exact control-file name. Never built from a caller string. */
static const char RECLAIM_FILE[] = "memory.reclaim";

/* The write seam is always compiled in: production installs nothing and the
 * pointer stays NULL, which means the real write(). Only the test build had it
 * before, so balance_exec.c could not be covered at all. */
static mr_bal_write_hook hook_fn;
static long hook_calls;
static char hook_last[128];
static int hook_errno;
static int last_write_errno;

/* ---- action record ---------------------------------------------------- *
 *
 * The observation side of the same seam. The write path above already keeps
 * the one thing the refusal branches needed (the last errno); what the GUI
 * could not show was the rest of the write itself: how long the text was, what
 * the kernel returned, and whether that return was a short write rather than a
 * failure. These are recorded here, at the single point every reclaim write in
 * this tree passes through, so no caller has to reconstruct them and none can
 * report a write the seam never saw.
 *
 * Nothing here decides anything: it only remembers. The classification
 * (EAGAIN / EINVAL / short write / other) is a pure function of the values
 * below, exported as mr_bal_write_class() so the test can assert the same
 * mapping the GUI shows.
 */
static long last_write_length;   /* requested length, bytes */
static long last_write_result;   /* what the seam returned: >=0 bytes or -1 */
static int last_write_attempted; /* 1 once any write went through the seam */

/* Per-write log. The record a user copies has to name every write this round
 * made, not just the last one, so the seam keeps a small ring of them. Sized to
 * the largest round this tree can issue (32 batches plus the probe); older
 * entries are overwritten rather than grown, because an unbounded log inside a
 * process that lives as long as the GUI is a leak with no upper bound.
 *
 * Indexed absolutely: entry i has a stable position for the life of the
 * process, and an entry that has been overwritten reports "not observed"
 * instead of silently returning a later write's data.
 */
#define MR_BAL_WRITE_LOG 64
static char write_log_text[MR_BAL_WRITE_LOG][96];
static long write_log_length[MR_BAL_WRITE_LOG];
static long write_log_result[MR_BAL_WRITE_LOG];
static int write_log_errno[MR_BAL_WRITE_LOG];
/* CLOCK_MONOTONIC milliseconds when each write returned, and the same instant
 * on the wall clock. Both are recorded for the same reason the length and errno
 * are: a reader who wants to line a write up with a sample taken by another
 * process needs to know when it happened, and "shortly before I looked" is not
 * a time. 0 means "not observed". */
static unsigned long long write_log_mono_ms[MR_BAL_WRITE_LOG];
static long long write_log_wall_unix[MR_BAL_WRITE_LOG];
static long write_log_total; /* how many writes the seam has ever seen */

unsigned long long mr_bal_reclaim_mono_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (unsigned long long)ts.tv_sec * 1000ull + (unsigned long long)(ts.tv_nsec / 1000000L);
}

long long mr_bal_reclaim_wall_unix(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) return 0;
    return (long long)ts.tv_sec;
}

void mr_bal_set_write_hook(mr_bal_write_hook hook) { hook_fn = hook; }
void mr_bal_test_set_write_errno(int value) { hook_errno = value; }
long mr_bal_test_write_calls(void) { return hook_calls; }
const char *mr_bal_test_last_write(void) { return hook_last; }
void mr_bal_test_clear_last_write(void) { hook_last[0] = '\0'; }

ssize_t mr_bal_write(int fd, const char *text, size_t length);

static ssize_t write_via_hook(int fd, const char *text, size_t length)
{
    return mr_bal_write(fd, text, length);
}

/* One entry, recorded by mr_bal_write() after it has the answer. Kept out of
 * the two branches above so both of them record identically: a hook write and a
 * real write are the same event as far as the record is concerned. */
static void write_log_push(const char *text, long length, long result, int error)
{
    long slot = write_log_total % MR_BAL_WRITE_LOG;
    snprintf(write_log_text[slot], sizeof(write_log_text[slot]), "%.95s", text);
    write_log_length[slot] = length;
    write_log_result[slot] = result;
    write_log_errno[slot] = result < 0 ? error : 0;
    write_log_mono_ms[slot] = mr_bal_reclaim_mono_ms();
    write_log_wall_unix[slot] = mr_bal_reclaim_wall_unix();
    write_log_total++;
}

ssize_t mr_bal_write(int fd, const char *text, size_t length)
{
    hook_calls++;
    snprintf(hook_last, sizeof(hook_last), "%.100s", text);
    if (hook_fn != NULL) {
        long result = hook_fn(fd, text, (unsigned long)length);
        int reported = 0;
        if (result < 0 && hook_errno != 0) {
            reported = hook_errno;
            hook_errno = 0;
            errno = reported;
        }
        /* Record the failure before anything else can overwrite errno. Reading
         * hook_errno a second time here was a bug: it had just been cleared, so
         * an injected EAGAIN was recorded as whatever errno happened to hold. */
        last_write_errno = (result < 0) ? (reported ? reported : errno) : 0;
        last_write_length = (long)length;
        last_write_result = result;
        last_write_attempted = 1;
        write_log_push(text, (long)length, result, last_write_errno);
        return (ssize_t)result;
    }
    ssize_t n = write(fd, text, length);
    last_write_errno = (n < 0) ? errno : 0;
    last_write_length = (long)length;
    last_write_result = (long)n;
    last_write_attempted = 1;
    write_log_push(text, (long)length, (long)n, last_write_errno);
    return n;
}

int mr_bal_write_errno(void) { return last_write_errno; }
void mr_bal_write_errno_clear(void) { last_write_errno = 0; }

/* ---- action record accessors ----------------------------------------- */

/* Requested length of the most recent write, or -1 when the seam has not been
 * used at all. -1 is the "not observed" value the record treats as absence. */
long mr_bal_write_last_length(void)
{
    return last_write_attempted ? last_write_length : -1;
}

/* What the seam returned for the most recent write: bytes written on success,
 * -1 on failure. -1 also when nothing was attempted — callers must pair this
 * with mr_bal_write_attempted() before reading a failure out of it. */
long mr_bal_write_last_result(void)
{
    return last_write_attempted ? last_write_result : -1;
}

/* 1 once at least one write has gone through the seam since the process
 * started. Never cleared by the errno reset: "did we ever write" and "why did
 * the last write fail" are different questions. */
int mr_bal_write_attempted(void) { return last_write_attempted; }

/* How many writes the seam has ever seen. Monotonic; a caller that wants "the
 * writes that happened after I started" takes this before and diffs. */
long mr_bal_write_log_count(void) { return write_log_total; }

/* Is absolute index i still inside the ring? 1 when readable. */
static int write_log_live(long index)
{
    if (index < 0 || index >= write_log_total) return 0;
    return index >= write_log_total - MR_BAL_WRITE_LOG;
}

const char *mr_bal_write_log_text(long index)
{
    if (!write_log_live(index)) return "";
    return write_log_text[index % MR_BAL_WRITE_LOG];
}

long mr_bal_write_log_length(long index)
{
    if (!write_log_live(index)) return -1;
    return write_log_length[index % MR_BAL_WRITE_LOG];
}

long mr_bal_write_log_result(long index)
{
    if (!write_log_live(index)) return -1;
    return write_log_result[index % MR_BAL_WRITE_LOG];
}

unsigned long long mr_bal_write_log_mono_ms(long index)
{
    if (!write_log_live(index)) return 0;
    return write_log_mono_ms[index % MR_BAL_WRITE_LOG];
}

long long mr_bal_write_log_wall_unix(long index)
{
    if (!write_log_live(index)) return 0;
    return write_log_wall_unix[index % MR_BAL_WRITE_LOG];
}

int mr_bal_write_log_errno(long index)
{
    if (!write_log_live(index)) return 0;
    return write_log_errno[index % MR_BAL_WRITE_LOG];
}

/* The same classification, for an arbitrary logged write. Kept as one function
 * so the ring and the "most recent" answer can never disagree about what an
 * errno means. */
const char *mr_bal_write_log_class(long index)
{
    if (!write_log_live(index)) return "not_observed";
    long length = write_log_length[index % MR_BAL_WRITE_LOG];
    long result = write_log_result[index % MR_BAL_WRITE_LOG];
    int error = write_log_errno[index % MR_BAL_WRITE_LOG];
    if (result >= 0) return result == length ? "accepted" : "short_write";
    switch (error) {
    case EAGAIN: return "eagain";
    case EINVAL: return "einval";
    case ENOMEM: return "enomem";
    case EBUSY: return "ebusy";
    case EPERM: return "eperm";
    case EACCES: return "eacces";
    case ENOENT: return "enoent";
    case EIO: return "eio";
    default: return error == 0 ? "failed_no_errno" : "failed_other";
    }
}

/* Stable classification of the most recent write, from the values recorded
 * above. Returns a fixed name; never NULL. "not_observed" is a real answer,
 * not a placeholder for "unknown": it means this process never reached the
 * seam, so a refusal upstream of the write is the only possible cause. */
const char *mr_bal_write_class(void)
{
    if (!last_write_attempted) return "not_observed";
    if (last_write_result >= 0) {
        /* A short write is not success. reclaim_once treats it as EIO, and the
         * record must agree with the caller that refused. */
        return last_write_result == last_write_length ? "accepted" : "short_write";
    }
    switch (last_write_errno) {
    case EAGAIN: return "eagain";
    case EINVAL: return "einval";
    case ENOMEM: return "enomem";
    case EBUSY: return "ebusy";
    case EPERM: return "eperm";
    case EACCES: return "eacces";
    case ENOENT: return "enoent";
    case EIO: return "eio";
    default: return last_write_errno == 0 ? "failed_no_errno" : "failed_other";
    }
}

/* One write, always with swappiness=0. Returns 0 on accept, or the errno. */
static int reclaim_once(int scope_fd, uint64_t bytes, uint32_t *saved_errno)
{
    *saved_errno = 0;
    int fd = openat(scope_fd, RECLAIM_FILE, O_WRONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        *saved_errno = (uint32_t)errno;
        return -1;
    }
    /* The parameter order matters: the kernel parses "<bytes> <knobs...>".
     * "swappiness=0 128M" is EINVAL, which was also measured. */
    char value[64];
    int length = snprintf(value, sizeof(value), "%llu swappiness=0",
                          (unsigned long long)bytes);
    if (length <= 0 || (size_t)length >= sizeof(value)) {
        (void)close(fd);
        *saved_errno = (uint32_t)EINVAL;
        return -1;
    }
    /* errno is cleared first: a short write leaves errno untouched, and
     * inheriting a stale value from an earlier call made a truncation look
     * like ENOSYS. */
    errno = 0;
    ssize_t written = write_via_hook(fd, value, (size_t)length);
    int saved = errno;
    (void)close(fd);
    if (written != (ssize_t)length) {
        *saved_errno = (uint32_t)(written < 0 ? saved : EIO);
        return -1;
    }
    return 0;
}

/* Read one key from memory.stat, or -1. */
static int read_scope_stat(int scope_fd, const char *key, uint64_t *out)
{
    int fd = openat(scope_fd, "memory.stat", O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return -1;
    char text[4096];
    ssize_t got = read(fd, text, sizeof(text) - 1);
    (void)close(fd);
    if (got <= 0) return -1;
    text[got] = '\0';
    size_t key_length = strlen(key);
    for (char *line = text; *line != '\0';) {
        char *line_end = strchr(line, '\n');
        if (line_end == NULL) line_end = line + strlen(line);
        if ((size_t)(line_end - line) > key_length &&
            strncmp(line, key, key_length) == 0 && line[key_length] == ' ') {
            uint64_t value = 0;
            int digits = 0;
            for (char *p = line + key_length + 1; *p >= '0' && *p <= '9'; p++) {
                value = value * 10u + (uint64_t)(*p - '0');
                digits++;
            }
            if (digits > 0) {
                *out = value;
                return 0;
            }
            return -1;
        }
        if (*line_end == '\0') break;
        line = line_end + 1;
    }
    return -1;
}

int64_t mr_bal_caps_probe(int scope_fd, enum mr_bal_caps *out)
{
    if (out == NULL) return -1;
    *out = MR_BAL_CAPS_UNKNOWN;
    if (scope_fd < 0) {
        *out = MR_BAL_CAPS_UNAVAILABLE;
        return 0;
    }
    /* Existence first: no control file at all is a different answer from a
     * control file that will not take the parameter. */
    int fd = openat(scope_fd, RECLAIM_FILE, O_WRONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        if (errno == ENOENT) *out = MR_BAL_CAPS_UNAVAILABLE;
        else *out = MR_BAL_CAPS_UNKNOWN;
        return 0;
    }
    (void)close(fd);

    /* A zero-byte request is the kernel's own no-op: it reclaims nothing and
     * still parses the parameter list. It therefore answers "does this kernel
     * understand swappiness=0" without touching a single page. */
    uint32_t probe_errno = 0;
    if (reclaim_once(scope_fd, 0, &probe_errno) == 0) {
        *out = MR_BAL_CAPS_FILE_ONLY;
        return 0;
    }
    /* EINVAL means the parameter list was not understood. EAGAIN on a zero
     * request would be odd; anything else is inconclusive rather than a
     * licence to fall back to the bare byte count. */
    *out = probe_errno == (uint32_t)EINVAL ? MR_BAL_CAPS_NO_SWAPPINESS
                                           : MR_BAL_CAPS_UNKNOWN;
    return 0;
}

void mr_bal_run_config_for_budget(uint64_t request_bytes,
                                  struct mr_bal_run_config *out)
{
    if (out == NULL) return;
    memset(out, 0, sizeof(*out));
    if (request_bytes == 0) return;
    out->budget_bytes = request_bytes;
    /* Small batches, deliberately. Each one is a separate synchronous reclaim,
     * so the loop can stop between them instead of inside a 512 MiB call. */
    out->batch_bytes = 32ull * MIB;
    if (out->batch_bytes > out->budget_bytes) out->batch_bytes = out->budget_bytes;
    out->max_batches = 32;
    out->psi_stop_centi = 0; /* caller may set; 0 disables the PSI stop */
    out->stop_on_budget = 1;
}

/* Read the live PSI some-rate for the whole machine, in hundredths of a
 * percent over the kernel's own 10-second average. Used as a stop signal
 * between batches: if reclaim is making the stall worse, stop. */
static uint32_t read_psi_some_centi(void)
{
    int fd = open("/proc/pressure/memory", O_RDONLY | O_CLOEXEC);
    if (fd < 0) return 0;
    char text[256];
    ssize_t got = read(fd, text, sizeof(text) - 1);
    (void)close(fd);
    if (got <= 0) return 0;
    text[got] = '\0';
    const char *avg = strstr(text, "avg10=");
    if (avg == NULL) return 0;
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
    return whole * 100u + frac;
}

int mr_bal_reclaim_run(int scope_fd, const struct mr_bal_run_config *config,
                       struct mr_bal_run_result *out)
{
    if (config == NULL || out == NULL) return -1;
    memset(out, 0, sizeof(*out));
    out->status = MR_BAL_RUN_BAD_REQUEST;
    out->caps = MR_BAL_CAPS_UNKNOWN;
    if (scope_fd < 0 || config->budget_bytes == 0 || config->batch_bytes == 0 ||
        config->batch_bytes > config->budget_bytes || config->max_batches == 0) {
        return 0;
    }

    enum mr_bal_caps caps = MR_BAL_CAPS_UNKNOWN;
    if (mr_bal_caps_probe(scope_fd, &caps) != 0) return -1;
    out->caps = caps;

    if (caps != MR_BAL_CAPS_FILE_ONLY) {
        /* No file-only path on this kernel. The alternative would be a bare
         * byte count, which was measured to reclaim anonymous pages; refusing
         * is the only answer consistent with the product boundary. */
        out->status = MR_BAL_RUN_UNSUPPORTED;
        out->error_code = caps == MR_BAL_CAPS_NO_SWAPPINESS ? EINVAL : ENOSYS;
        out->stop_reason = MR_BAL_STOP_UNSUPPORTED;
        out->stopped_early = 1;
        return 0;
    }

    uint64_t remaining = config->budget_bytes;
    int stopped = 0;
    int64_t stop_reason = MR_BAL_STOP_NONE;
    uint64_t file_before = 0;
    (void)read_scope_stat(scope_fd, "file", &file_before);

    for (uint32_t batch = 0; batch < config->max_batches && remaining > 0; batch++) {
        if (stopped) break;
        uint64_t size = remaining < config->batch_bytes ? remaining : config->batch_bytes;

        /* Stop conditions are checked BEFORE the write, so a batch is never
         * issued once the machine has started objecting. */
        if (config->psi_stop_centi > 0) {
            uint32_t psi = read_psi_some_centi();
            if (psi >= config->psi_stop_centi) {
                stopped = 1;
                stop_reason = MR_BAL_STOP_STALL_RISING;
                break;
            }
        }
        /* Nothing left to take: asking again would only return EAGAIN and
         * would look like progress in a batch counter. */
        uint64_t file_now = 0;
        if (read_scope_stat(scope_fd, "file", &file_now) == 0 && file_now == 0) {
            stopped = 1;
            stop_reason = MR_BAL_STOP_NO_COLD_PAGES;
            break;
        }

        out->batches_attempted++;
        out->requested_bytes += size;
        uint32_t batch_errno = 0;
        if (reclaim_once(scope_fd, size, &batch_errno) == 0) {
            out->batches_accepted++;
            out->accepted_bytes += size;
            remaining -= size;
            if (config->stop_on_budget && remaining == 0) break;
            continue;
        }

        out->batches_rejected++;
        /* Record the first errno only; later ones are consequences. */
        if (out->error_code == 0) out->error_code = (int32_t)batch_errno;
        if (batch_errno == (uint32_t)EAGAIN) {
            /* Measured meaning: the request was not satisfied in full. It does
             * NOT mean nothing was reclaimed: a 32 MiB swappiness=0 request on
             * this host returned EAGAIN while the scope still lost 4 MiB of
             * file pages (see docs/ADR/0008 and the live run in
             * docs/Q12_SAME_HOST_CONTROL.md). The honest report is "ask was
             * not met", with the observed delta measured by the caller, which
             * is why this backend stops rather than retries. */
            stopped = 1;
            stop_reason = MR_BAL_STOP_NO_COLD_PAGES;
            break;
        }
        /* Any other errno is a hard stop: EBUSY, EIO, EPERM all mean the
         * kernel is not co-operating and more writes will not help. */
        stopped = 1;
        stop_reason = MR_BAL_STOP_STALL_RISING;
        break;
    }

    out->stopped_early = stopped;
    out->stop_reason = stop_reason;
    if (out->batches_accepted == 0) {
        /* Nothing was accepted. "stopped" is the honest status when a stop
         * condition fired; "partial" would imply some of the budget landed. */
        out->status = MR_BAL_RUN_STOPPED;
    } else if (stopped) {
        out->status = MR_BAL_RUN_PARTIAL;
    } else {
        out->status = MR_BAL_RUN_OK;
    }
    return 0;
}

const char *mr_bal_stop_name(int64_t stop)
{
    switch (stop) {
    case MR_BAL_STOP_NONE: return "none";
    case MR_BAL_STOP_NO_COLD_PAGES: return "no_cold_pages";
    case MR_BAL_STOP_STALL_RISING: return "stall_rising";
    case MR_BAL_STOP_UNSUPPORTED: return "unsupported";
    case MR_BAL_STOP_ERROR: return "error";
    default: return "invalid";
    }
}

const char *mr_bal_caps_name(int64_t caps)
{
    switch (caps) {
    case MR_BAL_CAPS_UNKNOWN: return "unknown";
    case MR_BAL_CAPS_FILE_ONLY: return "file_only";
    case MR_BAL_CAPS_NO_SWAPPINESS: return "no_swappiness_parameter";
    case MR_BAL_CAPS_UNAVAILABLE: return "unavailable";
    default: return "invalid";
    }
}

const char *mr_bal_run_status_name(int64_t status)
{
    switch (status) {
    case MR_BAL_RUN_OK: return "ok";
    case MR_BAL_RUN_PARTIAL: return "partial";
    case MR_BAL_RUN_STOPPED: return "stopped";
    case MR_BAL_RUN_UNSUPPORTED: return "unsupported";
    case MR_BAL_RUN_NO_SCOPE: return "no_scope";
    case MR_BAL_RUN_BAD_REQUEST: return "bad_request";
    default: return "invalid";
    }
}
