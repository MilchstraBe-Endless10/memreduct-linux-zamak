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

require_marker 'CI_CLEAN_SIM enabled=true'
require_marker 'mock_bridge_connected=true'

# The clean button is target 19. Geometry derived from clean_button_rect plus
# the DLU scale: x in [11, 319], y in [335, 356] on a 337x369 window.
xdotool mousemove --window "$window_id" 165 345
sleep 0.1
capture_before="$artifact_dir/clean-before.png"
import -window "$window_id" "$capture_before"
xdotool click 1
sleep 0.3
require_marker 'CI_MOUSE_ACTION target=19'
require_marker 'CI_CLEAN_CONFIRM open=true mode=simulation'
capture_dialog="$artifact_dir/clean-confirm.png"
import -window "$window_id" "$capture_dialog"

# The dialog owns keyboard focus: Tab must toggle between Confirm and Cancel
# without dismissing it, so a user can reach either button without the mouse.
xdotool windowfocus --sync "$window_id"
xdotool key --clearmodifiers Tab
sleep 0.15
require_marker 'CI_CLEAN_CONFIRM_FOCUS choice=cancel'
xdotool key --clearmodifiers Tab
sleep 0.15
require_marker 'CI_CLEAN_CONFIRM_FOCUS choice=confirm'

# Confirm button is a fixed window-space rect: 112,190 78x24.
xdotool mousemove --window "$window_id" 151 202
sleep 0.1
xdotool click 1

require_marker 'CI_MOUSE_ACTION target=710'
require_marker 'CI_CLEAN_SIM stage=confirmed'

# The bridge publishes staged progress on its own one-second poll, so wait for
# each stage instead of sampling a fixed delay.
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
wait_for_marker 'CI_CLEAN_SIM stage=authorization-accepted backend=mock no_privilege_request=true'
wait_for_marker 'CI_CLEAN_SIM stage=helper-running helper=mock no_system_write=true'

# The fake helper must have been launched with the production contract, even
# though nothing privileged was actually invoked.
helper_args="$mock_stage.helper-args"
[[ -f "$helper_args" ]] || { printf 'Mock helper was never launched.\n' >&2; cat "$app_log" >&2; exit 1; }
cat "$helper_args" | tee "$artifact_dir/mock-helper-args.txt"
grep -q '/usr/libexec/memreduct-zamak-helper pagecache' "$helper_args" || {
  printf 'Mock helper received the wrong contract.\n' >&2
  cat "$helper_args" >&2
  exit 1
}

wait_for_marker 'CI_CLEAN_SIM result=accepted'
grep -q 'no_system_write=true' "$app_log" || {
  printf 'Simulation markers must declare no_system_write.\n' >&2
  exit 1
}

sleep 0.3
capture_result="$artifact_dir/clean-result.png"
import -window "$window_id" "$capture_result"
# The result text must actually change the rendered window, not just the log.
set +e
compare -metric AE "$capture_dialog" "$capture_result" null: 2>"$artifact_dir/result-pixels.txt"
compare_status="$?"
set -e
pixels="$(sed -nE 's/^[[:space:]]*([0-9]+).*/\1/p' "$artifact_dir/result-pixels.txt" | head -n 1)"
[[ "$compare_status" == 1 && "${pixels:-0}" -gt 0 ]] || {
  printf 'Result state did not change the rendered window.\n' >&2
  exit 1
}
printf 'result_changed_pixels=%s\n' "$pixels" | tee -a "$artifact_dir/markers.txt"

# The screenshot is evidence, so the crop a reviewer looks at is produced here
# deterministically instead of being improvised later. A shrunken preview of
# this same row reads as "33185%", so the published crop is the zoomed one.
magick "$capture_result" -crop 160x12+185+138 +repage -filter point -resize 700% \
  -colorspace gray -threshold 55% "$artifact_dir/zoom-swap-usage.png"
printf 'swap_usage_crop=zoom-swap-usage.png\n' | tee -a "$artifact_dir/markers.txt"

# A real second click must not silently retry: the state machine owns backoff.
before_retry="$(marker_count 'CI_CLEAN_SIM result=accepted')"
xdotool mousemove --window "$window_id" 165 345
xdotool click 1
sleep 0.3
after_retry="$(marker_count 'CI_CLEAN_SIM result=accepted')"
[[ "$before_retry" == "$after_retry" ]] || {
  printf 'Clean restarted automatically after a result.\n' >&2
  exit 1
}
printf 'automatic_retry=false\n' | tee -a "$artifact_dir/markers.txt"

# No privileged operation may appear anywhere in this run. Lines that name
# drop_caches are only acceptable when the same line declares no_system_write,
# which is how the simulation states it did not touch the kernel.
privileged_hits="$(grep -Ei 'pkexec|polkit|swapoff|zramctl' "$app_log" || true)"
kernel_hits="$(grep -i 'drop_caches' "$app_log" | grep -v 'no_system_write' || true)"
if [[ -n "$privileged_hits" || -n "$kernel_hits" ]]; then
  printf 'Isolated run mentioned a privileged operation:\n%s\n%s\n' \
    "$privileged_hits" "$kernel_hits" >&2
  exit 1
fi
printf 'privileged_reference=none\n' | tee -a "$artifact_dir/markers.txt"

kill -TERM "$app_pid" 2>/dev/null || true
for _ in {1..40}; do kill -0 "$app_pid" 2>/dev/null || break; sleep 0.1; done
app_pid=""

printf 'PASS: real GUI click -> confirmation -> mock authorization -> fake helper -> result feedback, all under Xvfb with no privilege, no pkexec and no kernel write.\n' \
  | tee "$artifact_dir/result.txt"
