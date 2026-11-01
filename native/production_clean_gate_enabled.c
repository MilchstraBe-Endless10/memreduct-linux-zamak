/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Production-build gate: production cleaning IS compiled in.
 *
 * This translation unit exists only in builds produced by
 * scripts/ci/build-production-clean-gui.sh. It is never part of the default
 * build (zam.toml lists the default gate instead), so a normal zam build can
 * never link a binary where mr_start_reclaim() is reachable from the UI. */
#include "async_privileged.h"

int64_t mr_production_clean_enabled(void) {
    return 1;
}
