#!/usr/bin/env bash
# Read-only evidence dump for the balanced-reclaim policy.
#
# This script writes nothing: it opens /proc and /sys for reading, prints what
# the planner would see, and exits. It exists so a real machine can be compared
# against the planner's classification without any privileged operation.
#
# Usage: scripts/balance-sample.sh [seconds-between-samples]
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
gap="${1:-0}"

if ! command -v cc >/dev/null; then
    echo 'Missing cc; this script compiles a small read-only probe.' >&2
    exit 1
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

cat >"$work/probe.c" <<'PROBE'
#define _POSIX_C_SOURCE 200809L
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "balance_snapshot.h"
#include "balance_plan.h"

static double mib(uint64_t v) { return (double)v / (1024.0 * 1024.0); }

int main(void)
{
    struct mr_bal_snapshot a;
    struct mr_bal_config cfg;
    struct mr_bal_planner planner;
    struct mr_bal_plan plan;

    if (mr_bal_snapshot_read("/proc", "/sys", &a) != 0) return 1;

    mr_bal_config_default(&cfg);
    mr_bal_planner_init(&planner);

    if (mr_bal_plan_build(&cfg, &planner, &a, NULL, 0, &plan) != 0) return 1;

    printf("MemTotal        %10.1f MiB\n", mib(a.total));
    printf("MemAvailable    %10.1f MiB\n", mib(a.available));
    printf("Cached          %10.1f MiB\n", mib(a.cached));
    printf("SReclaimable    %10.1f MiB\n", mib(a.sreclaimable));
    printf("Inactive(file)  %10.1f MiB\n", mib(a.inactive_file));
    printf("Inactive(anon)  %10.1f MiB\n", mib(a.inactive_anon));
    printf("Dirty+WB        %10.1f MiB\n", mib(a.dirty + a.writeback));
    printf("Swap used/free  %10.1f / %.1f MiB\n",
           mib(a.swap_total >= a.swap_free ? a.swap_total - a.swap_free : 0),
           mib(a.swap_free));
    printf("PSI some avg10  %10.2f %%  (cumulative %" PRIu64 " us)\n",
           a.psi_some_avg10_centi / 100.0, a.psi_some_total_us);
    printf("PSI full avg10  %10.2f %%\n", a.psi_full_avg10_centi / 100.0);
    printf("vmstat pswpin/out %" PRIu64 " / %" PRIu64 "\n", a.pswpin, a.pswpout);
    printf("swap devices    %10u\n", a.swap_devices);
    for (uint32_t i = 0; i < a.swap_devices; i++)
        printf("  %-16s size %8.1f MiB used %8.1f MiB%s\n", a.swap[i].name,
               mib(a.swap[i].size), mib(a.swap[i].used),
               a.swap[i].is_zram ? "  (zram)" : "");
    printf("missing sources 0x%x\n", a.missing);
    printf("\nstate   %s\naction  %s\nreason  %s\nrequest %" PRIu64 " bytes\n",
           mr_bal_state_name(plan.state), mr_bal_action_name(plan.action),
           mr_bal_reason_name(plan.reason), plan.request_bytes);
    return 0;
}
PROBE

cc -std=c11 -O2 -Wall -Wextra -Werror -pedantic \
   -I "$root/native" \
   -D_POSIX_C_SOURCE=200809L -include stdlib.h \
   "$work/probe.c" "$root/native/balance_snapshot.c" "$root/native/balance_plan.c" \
   -o "$work/probe"
"$work/probe"
if [[ "$gap" != "0" ]]; then
    echo
    echo "--- second sample in ${gap}s (read-only) ---"
    sleep "$gap"
    "$work/probe"
fi
