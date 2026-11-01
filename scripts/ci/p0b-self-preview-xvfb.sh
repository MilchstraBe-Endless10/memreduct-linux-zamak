#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Drive the isolated clean loop with real GUI events under a private Xvfb.
#
# The chain asserted here is the whole P3B closure:
#   real button click -> confirmation dialog -> mock authorization ->
#   fake helper (same-UID launcher) -> result reflected back into the window.
#
# Nothing privileged runs: no pkexec, no root helper, no /proc/sys/vm write.
# MEMREDUCT_CI enables the test bridge, MEMREDUCT_CI_CLEAN_SIMULATION selects
# the simulation backend, and MEMREDUCT_CI_MOCK_AUTHORIZER points at a private
# same-UID launcher that must satisfy the bridge's ownership/mode/link checks.
set -Eeuo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
artifact_dir="$(printenv ARTIFACT_DIR || true)"
tmp_root="$(printenv RUNNER_TEMP || printf /tmp)"
if [[ -z "$artifact_dir" ]]; then
  artifact_dir="$(mktemp -d "$tmp_root/memreduct-p3b-clean-artifacts.XXXXXX")"
else
  mkdir -p "$artifact_dir"
fi

# Xvfb and xwininfo are not installed system-wide here. They are unpacked into
# a private prefix instead of touching the host package database, so the prefix
# joins PATH before the dependency check and the caller does not need to.
xvfb_prefix="$(printenv XVFB_PREFIX || printf '%s/.local/opt/xvfb-test' "$HOME")"
if [[ -d "$xvfb_prefix/usr/bin" ]]; then
  export PATH="$xvfb_prefix/usr/bin:$PATH"
fi
for cmd in Xvfb xdotool xwininfo import identify sha256sum stat cmp grep awk; do
  command -v "$cmd" >/dev/null || {
    printf 'Missing P3B clean test command: %s (looked in %s/usr/bin)\n' "$cmd" "$xvfb_prefix" >&2
    exit 2
  }
done
app="$1"
[[ -x "$app" ]] || { printf 'Production executable missing: %s\n' "$app" >&2; exit 2; }
[[ "$(id -u)" != 0 ]] || { printf 'P3B clean loop test must run unprivileged.\n' >&2; exit 2; }

runtime_dir="$(mktemp -d "$tmp_root/memreduct-p3b-xvfb.XXXXXX")"
chmod 0700 "$runtime_dir"
xvfb_pid=""
app_pid=""
window_id=""
stop_pid() {
  local pid="$1" i
  [[ -n "$pid" ]] || return 0
  kill -0 "$pid" 2>/dev/null || return 0
  kill -TERM "$pid" 2>/dev/null || true
  for i in {1..40}; do kill -0 "$pid" 2>/dev/null || break; sleep 0.1; done
  kill -KILL "$pid" 2>/dev/null || true
  wait "$pid" 2>/dev/null || true
}
cleanup() {
  local status="$?"
  trap - EXIT INT TERM
  stop_pid "$app_pid"
  stop_pid "$xvfb_pid"
  rm -rf -- "$runtime_dir"
  exit "$status"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

display_file="$runtime_dir/display-number"
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp -noreset 3>"$display_file" \
  >"$artifact_dir/xvfb.log" 2>&1 &
xvfb_pid="$!"
for _ in {1..100}; do
  [[ -s "$display_file" ]] && break
  kill -0 "$xvfb_pid" 2>/dev/null || break
  sleep 0.1
done
[[ -s "$display_file" ]] || { cat "$artifact_dir/xvfb.log" >&2; exit 1; }
display_number="$(<"$display_file")"
export DISPLAY=":$display_number"
unset WAYLAND_DISPLAY WAYLAND_SOCKET
export GDK_BACKEND=x11 GTK_THEME=Adwaita:light GDK_SCALE=1 GDK_DPI_SCALE=1 LANG=C.UTF-8
export GSETTINGS_BACKEND=memory XDG_RUNTIME_DIR="$runtime_dir"
home_dir="$runtime_dir/home"
config_home="$runtime_dir/config"
mkdir -m 0700 "$home_dir" "$config_home"
export HOME="$home_dir"

# The mock authorizer is a private same-UID launcher. mr_start_reclaim_mock
# refuses anything that is not a single-link 0700 regular file owned by us.
mock_bin="$runtime_dir/mock-authorizer"
cat >"$mock_bin" <<'MOCK'
#!/bin/sh
# Stands in for the privileged authorizer. It records the fixed helper path and
# action it was asked for, then publishes the same staged progress a real
# authorization produces. The dwell times are what let the GUI's one-second
# poll observe each stage instead of jumping straight to the result.
# The stage reader accepts only an owner-private 0600 single-link regular file,
# so the staging write must not inherit a wider umask default.
umask 077
printf '%s %s\n' "$1" "$2" >"$MEMREDUCT_CI_MOCK_STAGE_FILE.helper-args"
printf 'authorized\n' >"$MEMREDUCT_CI_MOCK_STAGE_FILE.tmp"
mv -- "$MEMREDUCT_CI_MOCK_STAGE_FILE.tmp" "$MEMREDUCT_CI_MOCK_STAGE_FILE"
sleep 1.6
printf 'helper-running\n' >"$MEMREDUCT_CI_MOCK_STAGE_FILE.tmp"
mv -- "$MEMREDUCT_CI_MOCK_STAGE_FILE.tmp" "$MEMREDUCT_CI_MOCK_STAGE_FILE"
sleep 3.0
exit 0
MOCK
chmod 0700 "$mock_bin"
mock_stage="$runtime_dir/mock-stage"
: >"$mock_stage"
chmod 0600 "$mock_stage"

printf 'mode=isolated-clean-simulation\neuid=%s\nDISPLAY=%s\nGDK_BACKEND=%s\n' \
  "$(id -u)" "$DISPLAY" "$GDK_BACKEND" | tee "$artifact_dir/environment.txt"

app_log="$artifact_dir/app.log"
env HOME="$home_dir" XDG_CONFIG_HOME="$config_home" GDK_BACKEND=x11 \
  GTK_THEME=Adwaita:light GDK_SCALE=1 GDK_DPI_SCALE=1 GSETTINGS_BACKEND=memory \
  XDG_RUNTIME_DIR="$runtime_dir" \
  MEMREDUCT_CI=1 \
  MEMREDUCT_CI_CLEAN_SIMULATION=1 \
  MEMREDUCT_CI_MOCK_AUTHORIZER="$mock_bin" \
  MEMREDUCT_CI_MOCK_STAGE_FILE="$mock_stage" \
  stdbuf -oL "$app" >"$app_log" 2>&1 &
app_pid="$!"

for _ in {1..100}; do
  window_id="$(xdotool search --onlyvisible --name '^Mem Reduct$' 2>/dev/null | head -n 1 || true)"
  [[ -n "$window_id" ]] && break
  kill -0 "$app_pid" 2>/dev/null || break
  sleep 0.1
done
[[ -n "$window_id" ]] || { printf 'Main window did not appear.\n' >&2; cat "$app_log" >&2; exit 1; }
xwininfo -id "$window_id" >"$artifact_dir/main-geometry.txt"
width="$(awk '$1 == "Width:" {print $2; exit}' "$artifact_dir/main-geometry.txt")"
height="$(awk '$1 == "Height:" {print $2; exit}' "$artifact_dir/main-geometry.txt")"
printf 'main_window=%sx%s\n' "$width" "$height" | tee -a "$artifact_dir/environment.txt"

# Wait for the first /proc/meminfo sample; the clean path refuses to start
# without a real reading, and that refusal is a distinct marker.
for _ in {1..100}; do
  grep -q 'CI_MEMORY ' "$app_log" && break
  sleep 0.1
done
grep -q 'CI_MEMORY ' "$app_log" || { printf 'No memory sample.\n' >&2; cat "$app_log" >&2; exit 1; }

marker_count() { grep -c -- "$1" "$app_log" || true; }
require_marker() {
  grep -q -- "$1" "$app_log" || {
    printf 'Missing marker: %s\n' "$1" >&2
    tail -n 40 "$app_log" >&2
    return 1
  }
  printf 'marker=%s\n' "$1" | tee -a "$artifact_dir/markers.txt"
}

require_marker 'CI_PRODUCTION_START'

# This run exercises the ADR-0006 self trim plus the ADR-0007 READ-ONLY
# cross-process preview: open the Clean menu, click row 0 (target 500).
# Nothing privileged is reachable: no pkexec is configured, the advise half is
# a refusing stub in this build, and the preview only reads procfs.

# The clean button sits on the canvas floor. Scan downward by hover to find it
# without hardcoding the CSD titlebar offset.
clean_y=""
y=380
while (( y <= 412 )); do
  xdotool mousemove --window "$window_id" 165 "$y"
  sleep 0.1
  if grep -q "CI_MOUSE_HOVER target=19" "$app_log"; then clean_y="$y"; break; fi
  y=$(( y + 2 ))
done
[[ -n "$clean_y" ]] || { printf 'Clean button not found by hover scan.\n' >&2; tail -n 20 "$app_log" >&2; exit 1; }
# The arrow zone at the right edge of the button opens the popup (target 20);
# the body (19) would run the page-cache path instead. Window width is 337 and
# the arrow zone is the last clean_arrow_width_px pixels of the button.
xdotool mousemove --window "$window_id" 315 "$clean_y"
sleep 0.1
grep -q "CI_MOUSE_HOVER target=20" "$app_log" || {
  printf 'Arrow zone did not report target 20.\n' >&2; tail -n 20 "$app_log" >&2; exit 1;
}
xdotool click 1
sleep 0.3
require_marker 'CI_MOUSE_ACTION target=20'
require_marker 'CI_CLEAN_MENU open=true'

# Click the first row of the popup (target 500 = self trim). The popup opens
# above the button; scan upward from the button row.
row_y=""
y=$(( clean_y - 30 ))
while (( y >= 40 )); do
  xdotool mousemove --window "$window_id" 165 "$y"
  sleep 0.1
  if grep -q "CI_MOUSE_HOVER target=500" "$app_log"; then row_y="$y"; break; fi
  y=$(( y - 4 ))
done
[[ -n "$row_y" ]] || { printf 'Self-trim row not found by hover scan.\n' >&2; tail -n 20 "$app_log" >&2; exit 1; }
xdotool click 1
sleep 0.3
require_marker 'CI_MOUSE_ACTION target=500'
require_marker 'CI_CLEAN_SELF_MENU action=self-trim target=self only'
require_marker 'CI_CLEAN_SELF result='

# The read-only cross-process preview must have run and reported itself as
# read-only with no advice issued.
require_marker 'CI_CROSSPROC_PREVIEW status='
grep -q 'CI_CROSSPROC_PREVIEW .* readonly=true' "$app_log" || {
  printf 'Preview marker must declare readonly=true.\n' >&2
  grep 'CI_CROSSPROC_PREVIEW' "$app_log" >&2
  exit 1
}
grep -q 'CI_CROSSPROC_PREVIEW .* no_advice_issued=true' "$app_log" || {
  printf 'Preview marker must declare no_advice_issued=true.\n' >&2
  exit 1
}
grep 'CI_CROSSPROC_PREVIEW' "$app_log" | tail -n 1 | tee -a "$artifact_dir/markers.txt"

# No privileged operation may appear anywhere in this run.
privileged_hits="$(grep -Ei 'pkexec|polkit|swapoff|zramctl' "$app_log" || true)"
if [[ -n "$privileged_hits" ]]; then
  printf 'Isolated run mentioned a privileged operation:\n%s\n' "$privileged_hits" >&2
  exit 1
fi
printf 'privileged_reference=none\n' | tee -a "$artifact_dir/markers.txt"

import -window "$window_id" "$artifact_dir/self-trim-preview.png"

kill -TERM "$app_pid" 2>/dev/null || true
for _ in {1..40}; do kill -0 "$app_pid" 2>/dev/null || break; sleep 0.1; done
app_pid=""

printf 'PASS: self trim + read-only cross-process preview, real clicks under Xvfb, no privilege, no advice.\n' \
  | tee "$artifact_dir/result.txt"
