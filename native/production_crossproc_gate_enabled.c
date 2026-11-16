/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Production-build gate: cross-process reclaim IS compiled in.
 *
 * This translation unit exists only in builds produced by
 * scripts/ci/build-production-gui.sh. It is never part of the default build
 * (zam.toml lists production_crossproc_gate_default.c instead), so a normal
 * zam build can never link a binary where mr_xproc_start() reaches pkexec.
 *
 * Being compiled in does not make it usable: the helper and the Polkit action
 * must also be installed root-owned, the kernel must provide process_madvise,
 * and each action still requires an interactive authorization. */
#include <stdint.h>

int64_t mr_xproc_capability_enabled(void) {
    return 1;
}
