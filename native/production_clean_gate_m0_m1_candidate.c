/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Combined M0+M1 production profile. This translation unit is selected only by
 * build-production-crossproc-gui.sh m0+m1-candidate. The installer requires
 * the unique profile symbol, which is called by the enabled page-cache gate so
 * link-time dead-code removal cannot silently discard the marker. */
#include "async_privileged.h"

int64_t mr_build_profile_m0_m1_candidate(void) {
    return 1;
}

int64_t mr_production_clean_enabled(void) {
    return mr_build_profile_m0_m1_candidate();
}
