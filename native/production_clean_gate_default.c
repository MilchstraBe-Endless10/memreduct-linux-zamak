/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Default-build gate: production cleaning is NOT compiled in.
 *
 * async_privileged.c consults this only through mr_production_clean_enabled(),
 * so shipping this translation unit means the GUI can never reach the pkexec
 * bridge: no environment variable, config file, or runtime call can flip it,
 * because the enabling definition simply does not exist in this binary. The
 * production build script compiles production_clean_gate_enabled.c instead of
 * this file, which is the deliberate, auditable decision recorded in ADR-0005
 * territory and reviewed per release. */
#include "async_privileged.h"

int64_t mr_production_clean_enabled(void) {
    return 0;
}
