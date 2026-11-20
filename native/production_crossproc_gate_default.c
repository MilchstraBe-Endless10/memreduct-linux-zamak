/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Default-build gate: cross-process reclaim is NOT compiled in.
 *
 * async_crossproc.c consults this only through mr_xproc_capability_enabled(), so
 * shipping this translation unit means the GUI can never reach the privileged
 * cross-process helper: no environment variable, configuration file, or runtime
 * call can flip it, because the enabling definition simply does not exist in
 * this binary.
 *
 * The production build script (scripts/ci/build-production-gui.sh) compiles
 * production_crossproc_gate_enabled.c instead of this file. That swap is the
 * deliberate, auditable decision recorded in ADR-0007 and is reviewed per
 * release. It is a separate gate from the page-cache one on purpose: enabling
 * page-cache cleaning must not silently enable cross-process reclaim. */
#include <stdint.h>

int64_t mr_xproc_capability_enabled(void) {
    return 0;
}
