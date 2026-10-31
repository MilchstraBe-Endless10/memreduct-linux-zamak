#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Prove the production clean path is closed in a default build.
#
# The production bridge is compiled in unconditionally so its behaviour stays
# auditable, and MEMREDUCT_ENABLE_PRODUCTION_CLEAN is the only switch that
# reaches it. This run therefore checks the negative case with everything a
# user could influence turned on: MEMREDUCT_CI is set (so the test harness is
# live) but the simulation backend is not selected, and no build-time switch
# exists in this binary. The click must land on the guarded branch.
set -Eeuo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
artifact_dir="$(printenv ARTIFACT_DIR || true)"
tmp_root="$(printenv RUNNER_TEMP || printf /tmp)"
if [[ -z "$artifact_dir" ]]; then
  artifact_dir="$(mktemp -d "$tmp_root/memreduct-p3b-gate-artifacts.XXXXXX")"
else
  mkdir -p "$artifact_dir"
fi

xvfb_prefix="$(printenv XVFB_PREFIX || printf '%s/.local/opt/xvfb-test' "$HOME")"
if [[ -d "$xvfb_prefix/usr/bin" ]]; then
  export PATH="$xvfb_prefix/usr/bin:$PATH"
fi
for cmd in Xvfb xdotool xwininfo import sha256sum; do
  command -v "$cmd" >/dev/null || {
    printf 'Missing P3B gate test command: %s\n' "$cmd" >&2
    exit 2
  }
done
app="$1"
[[ -x "$app" ]] || { printf 'Executable missing: %s\n' "$app" >&2; exit 2; }
[[ "$(id -u)" != 0 ]] || { printf 'P3B gate test must run unprivileged.\n' >&2; exit 2; }

runtime_dir="$(mktemp -d "$tmp_root/memreduct-p3b-gate.XXXXXX")"
chmod 0700 "$runtime_dir"
xvfb_pid=""
app_pid=""
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

app_log="$artifact_dir/app.log"
env HOME="$home_dir" XDG_CONFIG_HOME="$config_home" GDK_BACKEND=x11 \
  GTK_THEME=Adwaita:light GDK_SCALE=1 GDK_DPI_SCALE=1 GSETTINGS_BACKEND=memory \
  XDG_RUNTIME_DIR="$runtime_dir" MEMREDUCT_CI=1 \
  stdbuf -oL "$app" >"$app_log" 2>&1 &
app_pid="$!"

window_id=""
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

# Without a window manager the window is drawn at the GTK scale factor, so the
# clean button cannot be addressed by bare canvas pixels. Its position is a
# fixed fraction of the client area: the button spans DLU x 8..(228-6) and sits
# 22px above the bottom, which is x ~51% and y ~94% at any scale.
click_x="$(python3 -c "print(round($width * 0.51))")"
click_y="$(python3 -c "print(round($height * 0.94))")"
printf 'window=%sx%s click=%s,%s\n' "$width" "$height" "$click_x" "$click_y" \
  | tee "$artifact_dir/target.txt"

for _ in {1..100}; do
  grep -q 'CI_CLEAN_SIM enabled=' "$app_log" && break
  sleep 0.1
done
grep -q 'CI_CLEAN_SIM enabled=false' "$app_log" || {
  printf 'Default build must not enable the simulation backend.\n' >&2
  cat "$app_log" >&2
  exit 1
}
printf 'marker=CI_CLEAN_SIM enabled=false\n' | tee -a "$artifact_dir/markers.txt"

xdotool mousemove --window "$window_id" "$click_x" "$click_y"
sleep 0.3
xdotool click 1
sleep 1.5

require() {
  grep -qF -- "$1" "$app_log" || {
    printf 'Missing marker: %s\n' "$1" >&2
    tail -n 30 "$app_log" >&2
    return 1
  }
  printf 'marker=%s\n' "$1" | tee -a "$artifact_dir/markers.txt"
}

# The click has to land on the button, or this run proves nothing about gating.
require 'CI_MOUSE_ACTION target=19'
# ...and the guarded branch must refuse it.
require 'CI_CLEAN_DISABLED action=none'

if grep -q 'CI_CLEAN_CONFIRM' "$app_log"; then
  printf 'Confirmation opened although production clean is not enabled.\n' >&2
  exit 1
fi
if grep -qE 'CI_CLEAN_SIM stage|CI_CLEAN result|CI_CLEAN_SIM result' "$app_log"; then
  printf 'A clean operation started although production clean is not enabled.\n' >&2
  exit 1
fi
printf 'confirmation_opened=false helper_invoked=false\n' | tee -a "$artifact_dir/markers.txt"

import -window "$window_id" "$artifact_dir/clean-disabled.png"
printf 'PASS: the default build refuses the clean action at the button, with no confirmation, no helper launch and no kernel access.\n' \
  | tee "$artifact_dir/result.txt"
