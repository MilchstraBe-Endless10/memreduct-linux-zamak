/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Behavioural harness for the file-first reclaim backend.
 *
 * Written in C on purpose: an injected failure is just an errno in the same
 * process, so the EAGAIN / EINVAL / short-write branches are exercised exactly
 * as the kernel would exercise them. A ctypes callback cannot carry errno
 * across the trampoline reliably (measured on this host), which made those
 * branches look untested-but-green.
 *
 * Usage: harness <scope-dir> <behaviour> <budget> <batch> <max-batches> <psi-stop>
 *   behaviour: accept | eagain | eperm | einval | short | accept-2-then-eagain
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "balance_reclaim.h"

static const char *g_behaviour;
static int g_calls;        /* reclaim writes only; the capability probe is separate */
static int g_probe_calls;
static char g_last[64];

/* The capability probe is a zero-byte request: on this kernel it succeeds and
 * reclaims nothing. A kernel that does not understand swappiness rejects it
 * with EINVAL, which is exactly the "einval" behaviour below. Modelling that
 * distinction is the point of counting the two kinds separately. */
static int is_probe(const char *text)
{
    return strncmp(text, "0 ", 2) == 0;
}

static long hook(int fd, const char *text, unsigned long length)
{
    (void)fd;
    snprintf(g_last, sizeof(g_last), "%s", text);
    if (is_probe(text)) {
        g_probe_calls++;
        if (strcmp(g_behaviour, "einval") == 0) { errno = EINVAL; return -1; }
        return (long)length;
    }
    g_calls++;
    if (strcmp(g_behaviour, "eagain") == 0) { errno = EAGAIN; return -1; }
    if (strcmp(g_behaviour, "eperm") == 0) { errno = EPERM; return -1; }
    if (strcmp(g_behaviour, "einval") == 0) { errno = EINVAL; return -1; }
    if (strcmp(g_behaviour, "short") == 0) return (long)length - 1;
    if (strcmp(g_behaviour, "accept-2-then-eagain") == 0) {
        if (g_calls <= 2) return (long)length;
        errno = EAGAIN;
        return -1;
    }
    return (long)length;
}

int main(int argc, char **argv)
{
    if (argc != 7) {
        fprintf(stderr, "usage: %s <scope> <behaviour> <budget> <batch> <max> <psi>\n", argv[0]);
        return 2;
    }
    g_behaviour = argv[2];
    struct mr_bal_run_config cfg;
    mr_bal_run_config_for_budget(strtoull(argv[3], NULL, 10), &cfg);
    cfg.batch_bytes = strtoull(argv[4], NULL, 10);
    cfg.max_batches = (uint32_t)strtoul(argv[5], NULL, 10);
    cfg.psi_stop_centi = (uint32_t)strtoul(argv[6], NULL, 10);

    mr_bal_set_write_hook(hook);
    int fd = open(argv[1], O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) { perror("open scope"); return 1; }
    struct mr_bal_run_result result;
    if (mr_bal_reclaim_run(fd, &cfg, &result) != 0) { close(fd); return 1; }
    close(fd);

    printf("status=%s\n", mr_bal_run_status_name(result.status));
    printf("caps=%s\n", mr_bal_caps_name(result.caps));
    printf("stop=%s\n", mr_bal_stop_name(result.stop_reason));
    printf("requested=%llu\n", (unsigned long long)result.requested_bytes);
    printf("accepted=%llu\n", (unsigned long long)result.accepted_bytes);
    printf("attempted=%u\n", result.batches_attempted);
    printf("accepted_batches=%u\n", result.batches_accepted);
    printf("rejected_batches=%u\n", result.batches_rejected);
    printf("error_code=%d\n", (int)result.error_code);
    printf("stopped_early=%d\n", (int)result.stopped_early);
    printf("write_calls=%d\n", g_calls);
    printf("probe_calls=%d\n", g_probe_calls);
    printf("last_write=%s\n", mr_bal_test_last_write());
    return 0;
}
