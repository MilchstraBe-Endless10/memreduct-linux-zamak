#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Q14 / Issue #12 — attribution harness for "Clean memory".
#
# WHAT THIS SCRIPT DOES
#   Samples the machine at fixed offsets around a marker instant and writes one
#   CSV row per sample. It never allocates memory to create pressure, never
#   touches swap settings, and — in its default mode — never performs any
#   cleanup at all.
#
# TWO MODES
#   control (default)
#       Sample at -10s, 0s, +1s, +30s, +120s, +300s. Nothing is executed at the
#       marker. This is the noise floor the cleanup arm must be compared against.
#
#   clean (opt-in, refuses by default)
#       Same timeline, but at the marker it runs $MR_CLEAN_CMD. It refuses to run
#       unless BOTH of these are set:
#           MR_CONFIRM_REAL_CLEAN=1
#           MR_CLEAN_CMD=<absolute path to the command>
#       There is deliberately no default command: this script must never be the
#       thing that decides to drop a cache. The authorized operator supplies it.
#
# WHY IT RE-EXECs ITSELF UNDER systemd-run
#   The harness runs for more than five minutes (marker + 300s), which outlives
#   any foreground bash invocation. Running under `systemd-run --user` gives the
#   run its own service scope, so it is not killed with the parent shell and the
#   run's own cgroup is identifiable. Output goes to $HOME, never /tmp: inside a
#   transient unit /tmp is a private tmpfs and the results would vanish.
#
# USAGE
#   scripts/ci/measure-click-attribution.sh                       # control arm
#   scripts/ci/measure-click-attribution.sh --mode clean ...      # refused unless confirmed
#   OUT_ROOT=/path scripts/ci/measure-click-attribution.sh
#
# EXIT CODES
#   0  timeline completed, all samples written
#   2  bad usage
#   3  clean mode requested without explicit confirmation
#   4  a required kernel interface is missing (see the error line)
set -uo pipefail

SELF_PATH="$(readlink -f "${BASH_SOURCE[0]}")"
MODE="control"
SKIP_REEXEC=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --mode) MODE="${2:-}"; shift 2 ;;
        --mode=*) MODE="${1#--mode=}"; shift ;;
        --no-reexec) SKIP_REEXEC=1; shift ;;
        -h|--help) sed -n '2,40p' "$SELF_PATH"; exit 0 ;;
        *) printf 'unknown argument: %s\n' "$1" >&2; exit 2 ;;
    esac
done

case "$MODE" in
    control|clean) ;;
    *) printf 'mode must be control or clean, got %q\n' "$MODE" >&2; exit 2 ;;
esac

# ---------------------------------------------------------------------------
# Re-exec under systemd-run --user so the run outlives the calling shell.
# ---------------------------------------------------------------------------
if [[ "$SKIP_REEXEC" -eq 0 ]]; then
    if [[ -n "${INVOCATION_ID:-}" ]]; then
        SKIP_REEXEC=1   # already inside a unit
    fi
fi
if [[ "$SKIP_REEXEC" -eq 0 ]]; then
    if ! command -v systemd-run >/dev/null 2>&1; then
        printf 'systemd-run not found; cannot guarantee a >5min run\n' >&2
        exit 4
    fi
    unit="memreduct-attrib-$(date +%Y%m%dT%H%M%S)-$$"
    exec systemd-run --user --quiet --pipe --wait --collect \
        --unit="$unit" \
        --setenv="HOME=$HOME" \
        --setenv="XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/run/user/$(id -u)}" \
        -- "$SELF_PATH" --mode "$MODE" --no-reexec
fi

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
set -u

OUT_ROOT="${OUT_ROOT:-$HOME/memreduct-attribution}"
UNIT_NAME="${UNIT_NAME:-session.slice}"
MARKER_OFFSETS=(-10 0 1 30 120 300)
LATENCY_SECONDS="${LATENCY_SECONDS:-2}"

USER_UID="$(id -u)"
SESSION_ROOT="/sys/fs/cgroup/user.slice/user-${USER_UID}.slice/user@${USER_UID}.service"
SESSION_SLICE_DIR="$SESSION_ROOT/$UNIT_NAME"

RUN_ID="run-$(date +%Y%m%dT%H%M%S)-$MODE-$$"
RUN_DIR="$OUT_ROOT/$RUN_ID"
RAW_DIR="$RUN_DIR/raw"
mkdir -p "$RAW_DIR" || exit 4

CSV="$RUN_DIR/samples.csv"
CSV_KV="$RUN_DIR/samples.kv"

# ---------------------------------------------------------------------------
# Hard precondition checks — refuse early instead of writing a CSV full of
# empty columns that later reads as "zero".
# ---------------------------------------------------------------------------
fail_missing() { printf 'missing required interface: %s\n' "$1" >&2; exit 4; }
[[ -r /proc/meminfo ]]        || fail_missing /proc/meminfo
[[ -r /proc/pressure/memory ]]|| fail_missing /proc/pressure/memory
[[ -r /proc/vmstat ]]         || fail_missing /proc/vmstat
[[ -r /proc/diskstats ]]      || fail_missing /proc/diskstats
[[ -r "$SESSION_ROOT/memory.stat" ]] || fail_missing "$SESSION_ROOT/memory.stat"
[[ -r "$SESSION_SLICE_DIR/memory.stat" ]] || fail_missing "$SESSION_SLICE_DIR/memory.stat"

# ---------------------------------------------------------------------------
# Clean-mode gate. Two independent opt-ins, no default command.
# ---------------------------------------------------------------------------
CLEAN_CMD=""
if [[ "$MODE" == "clean" ]]; then
    if [[ "${MR_CONFIRM_REAL_CLEAN:-0}" != "1" ]]; then
        printf 'refusing: clean mode needs MR_CONFIRM_REAL_CLEAN=1\n' >&2
        printf 'this harness does not decide to drop a cache on its own\n' >&2
        exit 3
    fi
    if [[ -z "${MR_CLEAN_CMD:-}" ]]; then
        printf 'refusing: clean mode needs MR_CLEAN_CMD (absolute path)\n' >&2
        exit 3
    fi
    if [[ "${MR_CLEAN_CMD:0:1}" != "/" ]]; then
        printf 'refusing: MR_CLEAN_CMD must be an absolute path\n' >&2
        exit 3
    fi
    CLEAN_CMD="$MR_CLEAN_CMD"
fi

# ---------------------------------------------------------------------------
# Latency probe.
#
# The probe must be small, reproducible and page-cache resident so that its own
# allocation cannot show up as the effect being measured. It is compiled once
# into $HOME (never /tmp) and cached by content hash: a stat + read of a 4 KiB
# file already in cache, timed per iteration in one process so process start-up
# is not part of the number.
# ---------------------------------------------------------------------------
PROBE_DIR="$OUT_ROOT/probe"
mkdir -p "$PROBE_DIR"
PROBE_SRC="$PROBE_DIR/latprobe.c"
PROBE_BIN="$PROBE_DIR/latprobe"
PROBE_DATA="$PROBE_DIR/latprobe.dat"

cat >"$PROBE_SRC" <<'PROBE_C'
/* SPDX-License-Identifier: GPL-3.0-or-later
 * Front-end responsiveness proxy: repeated stat + 4 KiB read of one page-cache
 * resident file. Reports p50/p99 in nanoseconds. Reads only; never writes the
 * data file, so it cannot dirty or evict cache itself. */
#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int cmp_u64(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

int main(int argc, char **argv)
{
    if (argc < 3) { fputs("usage: latprobe FILE SECONDS\n", stderr); return 2; }
    const char *path = argv[1];
    double seconds = atof(argv[2]);
    if (seconds <= 0.0) seconds = 2.0;

    char buf[4096];
    size_t cap = 200000, n = 0;
    uint64_t *samples = malloc(cap * sizeof(uint64_t));
    if (samples == NULL) { fputs("oom\n", stderr); return 1; }

    /* Warm one reference read so the first cold hit is not the p99. */
    int warm = open(path, O_RDONLY | O_CLOEXEC);
    if (warm < 0) { perror("latprobe: open"); free(samples); return 1; }
    (void)read(warm, buf, sizeof(buf));
    close(warm);

    uint64_t started = now_ns();
    uint64_t deadline = started + (uint64_t)(seconds * 1e9);
    while (now_ns() < deadline) {
        uint64_t t0 = now_ns();
        struct stat st;
        (void)stat(path, &st);
        int fd = open(path, O_RDONLY | O_CLOEXEC);
        if (fd >= 0) { ssize_t r = read(fd, buf, sizeof(buf)); (void)r; close(fd); }
        uint64_t dt = now_ns() - t0;
        if (n == cap) {
            cap *= 2;
            uint64_t *grown = realloc(samples, cap * sizeof(uint64_t));
            if (grown == NULL) break;
            samples = grown;
        }
        samples[n++] = dt;
    }
    if (n == 0) { fputs("no samples\n", stderr); free(samples); return 1; }
    qsort(samples, n, sizeof(uint64_t), cmp_u64);
    uint64_t p50 = samples[(size_t)((double)(n - 1) * 0.50)];
    uint64_t p99 = samples[(size_t)((double)(n - 1) * 0.99)];
    uint64_t pmax = samples[n - 1];
    printf("n=%zu p50_ns=%llu p99_ns=%llu max_ns=%llu\n",
           n, (unsigned long long)p50, (unsigned long long)p99,
           (unsigned long long)pmax);
    free(samples);
    return 0;
}
PROBE_C

# 4096 bytes of a fixed pattern: content is irrelevant, residency is the point.
if [[ ! -s "$PROBE_DATA" ]]; then
    head -c 4096 /dev/zero | tr '\0' 'x' >"$PROBE_DATA"
fi

probe_available=1
if [[ ! -x "$PROBE_BIN" || "$PROBE_SRC" -nt "$PROBE_BIN" ]]; then
    if command -v cc >/dev/null 2>&1; then
        cc -std=c11 -O2 -Wall -Wextra -o "$PROBE_BIN" "$PROBE_SRC" 2>"$PROBE_DIR/cc.log" || probe_available=0
    else
        probe_available=0
    fi
fi

run_latency_probe() {
    if [[ "$probe_available" -ne 1 ]]; then
        printf 'no_probe'
        return
    fi
    local out
    out="$("$PROBE_BIN" "$PROBE_DATA" "$LATENCY_SECONDS" 2>/dev/null)"
    if [[ -z "$out" ]]; then printf 'no_probe'; return; fi
    printf '%s' "$out" | tr ' ' ';'
}

# ---------------------------------------------------------------------------
# Readers. Each prints a compact key=value blob; parse into the CSV line.
# ---------------------------------------------------------------------------
read_meminfo() {
    awk '
        /^MemTotal:/      {t=$2}
        /^MemFree:/       {f=$2}
        /^MemAvailable:/  {a=$2}
        /^Buffers:/       {b=$2}
        /^Cached:/        {c=$2}
        /^SwapCached:/    {sc=$2}
        /^Active:/        {ac=$2}
        /^Inactive:/      {ic=$2}
        /^Active\(anon\):/ {aa=$2}
        /^Inactive\(anon\):/ {ia=$2}
        /^Active\(file\):/ {af=$2}
        /^Inactive\(file\):/ {iff=$2}
        /^AnonPages:/     {ap=$2}
        /^Mapped:/        {m=$2}
        /^Shmem:/         {sh=$2}
        /^Slab:/          {sl=$2}
        /^SReclaimable:/  {sr=$2}
        /^SUnreclaim:/    {su=$2}
        /^SwapTotal:/     {st=$2}
        /^SwapFree:/      {sf=$2}
        /^Dirty:/         {d=$2}
        /^Writeback:/     {w=$2}
        END {
            printf "mem_total_kib=%s mem_free_kib=%s mem_available_kib=%s buffers_kib=%s cached_kib=%s ",
                   t, f, a, b, c
            printf "swap_cached_kib=%s active_kib=%s inactive_kib=%s ", sc, ac, ic
            printf "active_anon_kib=%s inactive_anon_kib=%s active_file_kib=%s inactive_file_kib=%s ",
                   aa, ia, af, iff
            printf "anon_pages_kib=%s mapped_kib=%s shmem_kib=%s ", ap, m, sh
            printf "slab_kib=%s sreclaimable_kib=%s sunreclaim_kib=%s ", sl, sr, su
            printf "swap_total_kib=%s swap_free_kib=%s dirty_kib=%s writeback_kib=%s\n",
                   st, sf, d, w
        }
    ' /proc/meminfo
}

read_pressure() {
    awk -v pre="" '
        /^some / {
            for (i=1;i<=NF;i++) {
                split($i, kv, "=")
                if (kv[1]=="avg10")  s10=kv[2]
                if (kv[1]=="avg60")  s60=kv[2]
                if (kv[1]=="avg300") s300=kv[2]
                if (kv[1]=="total")  stot=kv[2]
            }
        }
        /^full / {
            for (i=1;i<=NF;i++) {
                split($i, kv, "=")
                if (kv[1]=="avg10")  f10=kv[2]
                if (kv[1]=="avg60")  f60=kv[2]
                if (kv[1]=="avg300") f300=kv[2]
                if (kv[1]=="total")  ftot=kv[2]
            }
        }
        END {
            printf "psi_some_avg10=%s psi_some_avg60=%s psi_some_avg300=%s psi_some_total_us=%s ", s10, s60, s300, stot
            printf "psi_full_avg10=%s psi_full_avg60=%s psi_full_avg300=%s psi_full_total_us=%s\n", f10, f60, f300, ftot
        }
    ' /proc/pressure/memory
}

read_psi_cpu_io() {
    awk '
        FNR==1 { which="cpu" } FNR>2 { which="io" }
        /^some / {
            for (i=1;i<=NF;i++) {
                split($i, kv, "=")
                if (kv[1]=="avg10")  a=kv[2]
                if (kv[1]=="total")  t=kv[2]
            }
            if (which=="cpu") { c10=a; ctot=t } else { i10=a; itot=t }
        }
        /^full / {
            for (i=1;i<=NF;i++) {
                split($i, kv, "=")
                if (kv[1]=="avg10")  a=kv[2]
                if (kv[1]=="total")  t=kv[2]
            }
            if (which=="cpu") { cf10=a; cftot=t } else { if10=a; iftot=t }
        }
        END {
            printf "psi_cpu_some_avg10=%s psi_cpu_some_total_us=%s psi_cpu_full_avg10=%s psi_cpu_full_total_us=%s ", c10, ctot, cf10, cftot
            printf "psi_io_some_avg10=%s psi_io_some_total_us=%s psi_io_full_avg10=%s psi_io_full_total_us=%s\n", i10, itot, if10, iftot
        }
    ' /proc/pressure/cpu /proc/pressure/io
}

read_vmstat() {
    awk '
        $1=="pswpin"                  {v_pswpin=$2}
        $1=="pswpout"                 {v_pswpout=$2}
        $1=="pgmajfault"              {v_pgmajfault=$2}
        $1=="pgfault"                 {v_pgfault=$2}
        $1=="workingset_refault_anon" {v_wsr_anon=$2}
        $1=="workingset_refault_file" {v_wsr_file=$2}
        $1=="workingset_activate_file" {v_ws_act_file=$2}
        $1=="nr_free_pages"           {v_nr_free=$2}
        $1=="nr_file_pages"           {v_nr_file=$2}
        $1=="nr_dirty"                {v_nr_dirty=$2}
        $1=="nr_writeback"            {v_nr_writeback=$2}
        $1=="pgscan_direct"           {v_pgscan_direct=$2}
        $1=="pgsteal_direct"          {v_pgsteal_direct=$2}
        $1=="pgscan_kswapd"           {v_pgscan_kswapd=$2}
        $1=="pgsteal_kswapd"          {v_pgsteal_kswapd=$2}
        $1=="pgactivate"              {v_pgactivate=$2}
        END {
            printf "vm_pswpin=%s vm_pswpout=%s vm_pgmajfault=%s vm_pgfault=%s ", v_pswpin, v_pswpout, v_pgmajfault, v_pgfault
            printf "vm_ws_refault_anon=%s vm_ws_refault_file=%s vm_ws_activate_file=%s ", v_wsr_anon, v_wsr_file, v_ws_act_file
            printf "vm_nr_free_pages=%s vm_nr_file_pages=%s vm_nr_dirty=%s vm_nr_writeback=%s ", v_nr_free, v_nr_file, v_nr_dirty, v_nr_writeback
            printf "vm_pgscan_direct=%s vm_pgsteal_direct=%s vm_pgscan_kswapd=%s vm_pgsteal_kswapd=%s vm_pgactivate=%s\n",
                   v_pgscan_direct, v_pgsteal_direct, v_pgscan_kswapd, v_pgsteal_kswapd, v_pgactivate
        }
    ' /proc/vmstat
}

# memory.stat for one cgroup directory, prefixed for the CSV.
read_cgroup_stat() {
    local dir="$1" prefix="$2"
    awk -v p="$prefix" '
        $1=="anon"                    {anon=$2}
        $1=="file"                    {file=$2}
        $1=="active_anon"             {a_anon=$2}
        $1=="inactive_anon"           {i_anon=$2}
        $1=="active_file"             {a_file=$2}
        $1=="inactive_file"           {i_file=$2}
        $1=="shmem"                   {shmem=$2}
        $1=="file_dirty"              {f_dirty=$2}
        $1=="file_writeback"          {f_wb=$2}
        $1=="slab_reclaimable"        {sl_rec=$2}
        $1=="slab_unreclaimable"      {sl_unrec=$2}
        $1=="workingset_refault_file" {wsr_file=$2}
        $1=="workingset_refault_anon" {wsr_anon=$2}
        $1=="workingset_activate_file" {ws_act=$2}
        $1=="pgscan"                  {pgscan=$2}
        $1=="pgsteal"                 {pgsteal=$2}
        END {
            printf "%s_cg_anon=%s %s_cg_file=%s %s_cg_active_anon=%s %s_cg_inactive_anon=%s ",
                   p, anon, p, file, p, a_anon, p, i_anon
            printf "%s_cg_active_file=%s %s_cg_inactive_file=%s %s_cg_shmem=%s %s_cg_file_dirty=%s ",
                   p, a_file, p, i_file, p, shmem, p, f_dirty
            printf "%s_cg_file_writeback=%s %s_cg_slab_reclaimable=%s %s_cg_slab_unreclaimable=%s ",
                   p, f_wb, p, sl_rec, p, sl_unrec
            printf "%s_cg_ws_refault_file=%s %s_cg_ws_refault_anon=%s %s_cg_ws_activate_file=%s ",
                   p, wsr_file, p, wsr_anon, p, ws_act
            printf "%s_cg_pgscan=%s %s_cg_pgsteal=%s\n", p, pgscan, p, pgsteal
        }
    ' "$dir/memory.stat"
}

# Per-device I/O. Written to a raw sidecar (one line per device) and summed into
# the CSV as sectors read/written across whole disks only, so a partition and its
# parent are never double counted. Whole disks have no parent: the raw name has
# no trailing digit and is not nvmeXnYpZ / mmcblkXpY.
read_diskstats() {
    local target="$1"
    awk '{ printf "%s %s %s %s\n", $3, $4, $6, $10 }' /proc/diskstats >"$target"

    awk '
        {
            name=$3; rd=$4; rsec=$6; wsec=$10
            # skip partitions: sda1, nvme0n1p1, mmcblk0p1, dm-0, loop0, zram0
            if (name ~ /^loop/ || name ~ /^zram/ || name ~ /^ram/) next
            if (name ~ /^nvme[0-9]+n[0-9]+p[0-9]+$/) next
            if (name ~ /^mmcblk[0-9]+p[0-9]+$/) next
            if (name ~ /^dm-[0-9]+$/) next
            if (name ~ /^md[0-9]+$/) next
            if (name ~ /^sd[a-z]+[0-9]+$/) next
            if (name ~ /^vd[a-z]+[0-9]+$/) next
            if (name ~ /^hd[a-z]+[0-9]+$/) next
            rd_total += rsec
            wr_total += wsec
        }
        END { printf "disk_sectors_read=%s disk_sectors_written=%s\n", rd_total, wr_total }
    ' /proc/diskstats
}

read_zram() {
    if [[ -r /sys/block/zram0/mm_stat ]]; then
        awk '{ printf "zram_orig_bytes=%s zram_compr_bytes=%s zram_mem_used_bytes=%s\n", $1, $2, $3 }' \
            /sys/block/zram0/mm_stat
    else
        printf 'zram_orig_bytes= zram_compr_bytes= zram_mem_used_bytes=\n'
    fi
}

# ---------------------------------------------------------------------------
# One sample = one CSV row.
# ---------------------------------------------------------------------------
collect_sample() {
    local offset="$1" label="$2"
    local stamp raw_base
    stamp="$(date -u +%Y-%m-%dT%H:%M:%S.%3NZ)"
    raw_base="$RAW_DIR/off${offset}"

    local mem pressure psi_cpu_io vm cg_session cg_slice disk zram
    mem="$(read_meminfo)"
    pressure="$(read_pressure)"
    psi_cpu_io="$(read_psi_cpu_io)"
    vm="$(read_vmstat)"
    cg_session="$(read_cgroup_stat "$SESSION_ROOT" session)"
    cg_slice="$(read_cgroup_stat "$SESSION_SLICE_DIR" slice)"
    disk="$(read_diskstats "$raw_base.diskstats")"
    zram="$(read_zram)"

    printf '%s\n' "$mem"        >"$raw_base.meminfo"
    printf '%s\n' "$pressure"   >"$raw_base.pressure"
    printf '%s\n' "$psi_cpu_io" >"$raw_base.pressure-cpu-io"
    printf '%s\n' "$vm"         >"$raw_base.vmstat"
    printf '%s\n' "$cg_session" >"$raw_base.cgroup-session"
    printf '%s\n' "$cg_slice"   >"$raw_base.cgroup-slice"
    cp "$SESSION_ROOT/memory.stat" "$raw_base.session.memory.stat" 2>/dev/null
    cp "$SESSION_SLICE_DIR/memory.stat" "$raw_base.slice.memory.stat" 2>/dev/null
    cp /proc/meminfo "$raw_base.meminfo.raw" 2>/dev/null
    cp /proc/pressure/memory "$raw_base.pressure.raw" 2>/dev/null

    local probe
    probe="$(run_latency_probe)"

    local line
    line="ts_utc=$stamp offset_s=$offset label=$label $mem $pressure $psi_cpu_io $vm $cg_session $cg_slice $disk $zram probe=$probe"
    # Two views of one sample, both derived from this exact string so they cannot
    # disagree: samples.kv is the readable/debug form, samples.csv is the
    # analysis form whose header names are taken from the same line.
    printf '%s\n' "$line" >>"$CSV_KV"
    emit_csv_row "$line"
    printf '[off %+4ss] %-8s avail=%s KiB  file=%s anon=%s  refault_file=%s\n' \
        "$offset" "$label" \
        "$(printf '%s' "$mem" | tr ' ' '\n' | sed -n 's/^mem_available_kib=//p')" \
        "$(printf '%s' "$cg_session" | tr ' ' '\n' | sed -n 's/^session_cg_file=//p')" \
        "$(printf '%s' "$cg_session" | tr ' ' '\n' | sed -n 's/^session_cg_anon=//p')" \
        "$(printf '%s' "$vm" | tr ' ' '\n' | sed -n 's/^vm_ws_refault_file=//p')"
}

# ---------------------------------------------------------------------------
# Timeline
# ---------------------------------------------------------------------------
{
    printf 'run_id=%s\n'          "$RUN_ID"
    printf 'mode=%s\n'            "$MODE"
    printf 'invoked_at_utc=%s\n'  "$(date -u +%Y-%m-%dT%H:%M:%S.%3NZ)"
    printf 'invoked_at_local=%s\n' "$(date +%Y-%m-%dT%H:%M:%S%:z)"
    printf 'kernel=%s\n'          "$(uname -srmo)"
    printf 'os=%s\n'              "$(sed -n 's/^PRETTY_NAME="\(.*\)"/\1/p' /etc/os-release)"
    printf 'session_root=%s\n'    "$SESSION_ROOT"
    printf 'unit=%s\n'            "$UNIT_NAME"
    printf 'unit_dir=%s\n'        "$SESSION_SLICE_DIR"
    printf 'marker_offsets=%s\n'  "${MARKER_OFFSETS[*]}"
    printf 'latency_seconds=%s\n' "$LATENCY_SECONDS"
    printf 'probe_available=%s\n' "$probe_available"
    printf 'unit_cgroup=%s\n'     "$(cat /proc/self/cgroup | head -1)"
    printf 'memreduct_installed_sha256=%s\n' \
        "$(sha256sum /usr/bin/memreduct-zamak 2>/dev/null | awk '{print $1}')"
    printf 'clean_cmd=%s\n'       "${CLEAN_CMD:-<none>}"
} >"$RUN_DIR/meta.txt"

# The CSV header is derived from the first sample, so a column can never be
# labelled with a name the values do not have.
header_written=0

emit_csv_row() {
    local line="$1"
    local values=()
    local kv
    for kv in $line; do
        values+=("${kv#*=}")
    done
    if [[ "$header_written" -eq 0 ]]; then
        local names=()
        for kv in $line; do names+=("${kv%%=*}"); done
        printf '%s\n' "$(IFS=,; echo "${names[*]}")" >"$CSV"
        header_written=1
    fi
    printf '%s\n' "$(IFS=,; echo "${values[*]}")" >>"$CSV"
}


printf 'timeline start: mode=%s run=%s\n' "$MODE" "$RUN_ID"

# The marker instant is a fixed wall-clock second, computed up front. Every
# sample sleeps until (marker + offset) instead of accumulating per-step delays,
# so a slow sample cannot push the later offsets out of position.
pre_offset=0
for off in "${MARKER_OFFSETS[@]}"; do
    if [[ "$off" -lt "$pre_offset" ]]; then pre_offset="$off"; fi
done
pre_lead=$(( -pre_offset ))   # seconds spent before the marker

marker_epoch=$(( $(date +%s) + pre_lead ))

for off in "${MARKER_OFFSETS[@]}"; do
    target_epoch=$(( marker_epoch + off ))
    while :; do
        now_epoch=$(date +%s)
        delta=$(( target_epoch - now_epoch ))
        [[ "$delta" -le 0 ]] && break
        sleep "$delta"
    done

    case "$off" in
        -*) label="pre-marker" ;;
        0)  label="marker" ;;
        *)  label="post-$off" ;;
    esac
    collect_sample "$off" "$label"

    if [[ "$off" -eq 0 && "$MODE" == "clean" ]]; then
        printf 'executing clean command at marker: %s\n' "$CLEAN_CMD"
        clean_started_utc="$(date -u +%Y-%m-%dT%H:%M:%S.%3NZ)"
        "$CLEAN_CMD" >>"$RUN_DIR/clean.out" 2>>"$RUN_DIR/clean.err"
        clean_rc=$?
        printf 'clean_rc=%s\nclean_started_utc=%s\nclean_finished_utc=%s\n' \
            "$clean_rc" "$clean_started_utc" "$(date -u +%Y-%m-%dT%H:%M:%S.%3NZ)" \
            >>"$RUN_DIR/clean-result.txt"
        printf 'clean command exit=%s\n' "$clean_rc"
    fi
done

# One late row after the last scheduled sample: catches a rebound that peaked
# between samples rather than turning it into a straight line.
late_epoch=$(( marker_epoch + ${MARKER_OFFSETS[${#MARKER_OFFSETS[@]}-1]} + 10 ))
while :; do
    now_epoch=$(date +%s)
    delta=$(( late_epoch - now_epoch ))
    [[ "$delta" -le 0 ]] && break
    sleep "$delta"
done
collect_sample $(( ${MARKER_OFFSETS[${#MARKER_OFFSETS[@]}-1]} + 10 )) "late"

{
    printf 'finished_at_utc=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%S.%3NZ)"
    printf 'samples=%s\n' "$(($(wc -l <"$CSV")))"
} >>"$RUN_DIR/meta.txt"

printf 'timeline done: %s\n' "$RUN_DIR"
printf 'csv: %s\n' "$CSV"
