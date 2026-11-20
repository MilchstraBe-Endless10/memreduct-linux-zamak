/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Q17 per-scope throttle pulse - see balance_pulse.c for the safety contract. */
#ifndef MR_BALANCE_PULSE_H
#define MR_BALANCE_PULSE_H

#include <stdint.h>

#define MR_PULSE_MAX_SCOPES 32

enum mr_pulse_rc {
    MR_PULSE_OK = 0,
    MR_PULSE_OK_ABORTED_PSI = 1,
    MR_PULSE_OK_NOTHING_TO_DO = 2,
    MR_PULSE_ERR_ARG = -1,
    MR_PULSE_ERR_READ = -2,
    MR_PULSE_ERR_WRITE = -3,
    MR_PULSE_ERR_RESTORE = -4,
    MR_PULSE_ERR_LIMIT = -5
};

struct mr_pulse_result {
    int64_t before_current;
    int64_t throttle_target;
    int64_t after_current;
    int restored;
    int aborted_by_psi;
};

int64_t mr_pulse_scope(const char *scope_path, int64_t pulse_ms,
                       struct mr_pulse_result *out);
int64_t mr_pulse_restore_all(void);

#endif

/* Aggregated run over every delegated scope in the caller's app.slice.
 * Results are per-scope; restore failures are never swallowed. */
struct mr_pulse_run {
    int64_t scopes_pulsed;
    int64_t scopes_restored;
    int64_t restore_failures;
    int64_t psi_aborts;
    int64_t before_available;
    int64_t after_available;
};

int64_t mr_pulse_run_all(struct mr_pulse_run *out);
