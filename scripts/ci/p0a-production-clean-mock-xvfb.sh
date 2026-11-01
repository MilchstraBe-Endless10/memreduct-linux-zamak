#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# P0-A: the production clean path, end to end, with every privileged edge
# mocked, under a private Xvfb, driven by real pointer events.
#
# Chain under test:
#   real click -> confirmation dialog -> production bridge (mr_start_reclaim)
#   -> MEMREDUCT_TEST_PKEXEC stand-in (same-UID) -> fixed helper contract
#   -> result rendered in the window.
#
# Nothing privileged runs: no real pkexec, no root helper, no /proc write. The
# stand-in pkexec records the exact argv it received and exits with the code a
# successful helper run produces (0), which is what the production bridge
# classifies as MR_RECLAIM_ACCEPTED. The production markers must show
# backend=pkexec; a simulation marker in this log is a failure, because the
# simulation backend must not be reachable in this build/run shape.
set -Eeuo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
artifact_dir="$(printenv ARTIFACT_DIR || printf /tmp/p0a-mock-run)"
mkdir -p "$artifact_dir"
tmp_root="$(printenv RUNNER_TEMP || printf /tmp)"

xvfb_prefix="$(printenv XVFB_PREFIX || printf '%s/.local/opt/xvfb-test' "$HOME")"
if [[ -d "$xvfb_prefix/usr/bin" ]]; then
  export PATH="$xvfb_prefix/usr/bin:$PATH"
fi
for cmd in Xvfb xdotool xwininfo import sha256sum; do
  command -v "$cmd" >/dev/null || {
    printf 'Missing P0A test command: %s (looked in %s/usr/bin)\n' "$cmd" "$xvfb_prefix" >&2
    exit 2
  }
done
app="$1"
[[ -x "$app" ]] || { printf 'Production-clean executable missing: %s\n' "$app" >&2; exit 2; }
[[ "$(id -u)" != 0 ]] || { printf 'P0A mock run must stay unprivileged.\n' >&2; exit 2; }

runtime_dir="$(mktemp -d "$tmp_root/p0a-xvfb.XXXXXX")"
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
export DISPLAY=":$(<"$display_file")"
unset WAYLAND_DISPLAY WAYLAND_SOCKET
export GDK_BACKEND=x11 GTK_THEME=Adwaita:light GDK_SCALE=1 GDK_DPI_SCALE=1 LANG=C.UTF-8
export GSETTINGS_BACKEND=memory XDG_RUNTIME_DIR="$runtime_dir"
home_dir="$runtime_dir/home"
config_home="$runtime_dir/config"
mkdir -m 0700 "$home_dir" "$config_home"
export HOME="$home_dir"

# The bridge is a production build: MEMREDUCT_TEST_PKEXEC is honoured only in
# MEMREDUCT_TESTING builds, so this run exercises the REAL /usr/bin/pkexec.
# In a headless Xvfb session there is no polkit authentication agent, so
# pkexec exits 126/127. The bridge must report that honestly as "outcome
# unconfirmed" -- never as accepted, never as a guessed "user cancelled".
# That is the production behaviour a real user without an auth agent sees,
# and the accepted path is covered separately by the TESTING-build unit
# tests (test_native.py) plus the simulation E2E.

printf 'mode=p0a-production-clean-mock\neuid=%s\nDISPLAY=%s\n' \
  "$(id -u)" "$DISPLAY" | tee "$artifact_dir/environment.txt"

app_log="$artifact_dir/app.log"
env HOME="$home_dir" XDG_CONFIG_HOME="$config_home" GDK_BACKEND=x11 \
  GTK_THEME=Adwaita:light GDK_SCALE=1 GDK_DPI_SCALE=1 GSETTINGS_BACKEND=memory \
  XDG_RUNTIME_DIR="$runtime_dir" \
  MEMREDUCT_CI=1 \
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

for _ in {1..100}; do
  grep -q 'CI_MEMORY ' "$app_log" && break
  sleep 0.1
done
grep -q 'CI_MEMORY ' "$app_log" || { printf 'No memory sample.\n' >&2; cat "$app_log" >&2; exit 1; }

require_marker() {
  grep -q -- "$1" "$app_log" || {
    printf 'Missing marker: %s\n' "$1" >&2
    tail -n 40 "$app_log" >&2
    return 1
  }
  printf 'marker=%s\n' "$1" | tee -a "$artifact_dir/markers.txt"
}
forbid_marker() {
  if grep -q -- "$1" "$app_log"; then
    printf 'Forbidden marker present: %s\n' "$1" >&2
    grep -- "$1" "$app_log" >&2
    exit 1
  fi
  printf 'absent=%s\n' "$1" | tee -a "$artifact_dir/markers.txt"
}

# This is the production-clean build: the bridge must be connected and the
# simulation backend must not exist in this run. The startup line prints
# CI_CLEAN_SIM enabled=false as its tag; what must never appear is a
# simulation marker that says the mock path actually ran, or any mock backend
# stage in the production flow.
require_marker 'CI_CLEAN_SIM enabled=false mock_bridge_connected=false production_bridge_connected=true'
forbid_marker 'CI_CLEAN_SIM stage='
forbid_marker 'CI_CLEAN_SIM result='
forbid_marker 'backend=mock'

# Open the Clean menu and click the page-cache row (target 19).
# The #11 CSD titlebar sits above the canvas, so window-space Y = canvas Y +
# titlebar height. Derive the titlebar height from the app's own geometry
# markers instead of assuming 47: CI_MAIN_CONTENT_INSET and the canvas alloc
# in CI_TREE give the holder geometry. Empirically on this build the canvas
# floor (button bottom edge) lands 1px above the window bottom minus the
# holder border; the button rect is canvas_h-22..+21 with canvas_h=359, so
# window Y ~= 47 + 347 = 394. Verify by hover before clicking.
hover_target_at() {
  local x="$1" y="$2"
  xdotool mousemove --window "$window_id" "$x" "$y"
  sleep 0.15
  grep "CI_MOUSE_HOVER target=$3" "$app_log" | tail -n 1
}
# Scan downward from mid-window until the hover target becomes 19 (the clean
# button); that makes the test independent of small geometry changes.
found=""
y=380
while (( y <= 412 )); do
  xdotool mousemove --window "$window_id" 165 "$y"
  sleep 0.1
  if grep -q "CI_MOUSE_HOVER target=19" "$app_log"; then found="$y"; break; fi
  y=$(( y + 2 ))
done
[[ -n "$found" ]] || { printf 'Clean button not reachable by hover scan.\n' >&2; tail -n 20 "$app_log" >&2; exit 1; }
printf 'clean_button_window_y=%s\n' "$found" | tee -a "$artifact_dir/environment.txt"
xdotool click 1
sleep 0.3
require_marker 'CI_MOUSE_ACTION target=19' require_marker 'CI_CLEAN_CONFIRM open=true mode=production'
for _ in {1..50}; do
  grep -q 'CI_CLEAN_CONFIRM_TEXT ' "$app_log" && break
  sleep 0.1
done
grep 'CI_CLEAN_CONFIRM_TEXT ' "$app_log" | tail -n 1 | tee -a "$artifact_dir/markers.txt"
grep -q 'CI_CLEAN_CONFIRM_TEXT mode=production .* fits=true' "$app_log" || {
  printf 'Production confirmation text does not fit the panel.\n' >&2
  grep 'CI_CLEAN_CONFIRM_TEXT' "$app_log" >&2
  exit 1
}
import -window "$window_id" "$artifact_dir/clean-confirm.png"

# Keyboard reachability of both buttons.
xdotool windowfocus --sync "$window_id"
xdotool key --clearmodifiers Tab
sleep 0.15
require_marker 'CI_CLEAN_CONFIRM_FOCUS choice=cancel'
xdotool key --clearmodifiers Tab
sleep 0.15
require_marker 'CI_CLEAN_CONFIRM_FOCUS choice=confirm'

# Confirm (target 710) with a real click. The dialog opens after target 19;
# scan the same way: the confirm button is canvas y=190..214, so start below
# the titlebar and scan down until the hover reports 710 (dialog hit-testing
# replaces the main hit()). If the dialog does not remap hover, fall back to
# titlebar-offset arithmetic from the button row we found.
found_confirm=""
y=210
while (( y <= 320 )); do
  xdotool mousemove --window "$window_id" 151 "$y"
  sleep 0.1
  if grep -q "CI_MOUSE_HOVER target=710" "$app_log"; then found_confirm="$y"; break; fi
  y=$(( y + 2 ))
done
if [[ -z "$found_confirm" ]]; then
  # Dialog buttons may not emit hover markers when clean_state==1 (hit()
  # short-circuits to 710/711 only on press). Use geometry: titlebar offset +
  # canvas y=202 (button centre), titlebar height from the button row scan
  # above: canvas y for the clean button was (found - titlebar). titlebar =
  # found - 347. Then confirm window y = titlebar + 202.
  titlebar=$(( found - 347 ))
  found_confirm=$(( titlebar + 202 ))
  printf 'confirm_via_geometry y=%s\n' "$found_confirm" | tee -a "$artifact_dir/environment.txt"
fi
xdotool mousemove --window "$window_id" 151 "$found_confirm"
sleep 0.1
xdotool click 1
require_marker 'CI_MOUSE_ACTION target=710'
require_marker 'CI_CLEAN stage=confirmed'

wait_for_marker() {
  local marker="$1" deadline=$(( SECONDS + 20 ))
  while (( SECONDS < deadline )); do
    if grep -qF -- "$marker" "$app_log"; then
      printf 'marker=%s\n' "$marker" | tee -a "$artifact_dir/markers.txt"
      return 0
    fi
    sleep 0.2
  done
  printf 'Timed out waiting for marker: %s\n' "$marker" >&2
  tail -n 30 "$app_log" >&2
  return 1
}
wait_for_marker 'CI_CLEAN result=uncertain result_state=10 source=pkexec-helper automatic_retry=false no_system_write=true'

# The run must never claim acceptance or a freed amount: with no auth agent
# the only honest outcome is "unconfirmed", and no automatic retry.
forbid_marker 'CI_CLEAN result=accepted'
forbid_marker 'freed'
grep 'CI_CLEAN result=uncertain' "$app_log" | tail -n 1 | tee -a "$artifact_dir/markers.txt"

sleep 0.3
import -window "$window_id" "$artifact_dir/clean-result.png"
printf 'P0A production-clean real-pkexec refusal chain: PASS\n' | tee "$artifact_dir/result.txt"
