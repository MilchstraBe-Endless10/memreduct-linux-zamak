#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Exercise the M1.3 cross-process picker and result flow with fixed CI fixtures.
# This path cannot enumerate real candidates or invoke pkexec/helper/syscalls.
set -Eeuo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
app="${1:-$root/artifacts/p2b/memreduct-zamak-ci-test}"
artifact_dir="${ARTIFACT_DIR:-$root/artifacts/p3c-xproc-simulation}"
[[ -x "$app" ]] || { printf 'Production executable missing: %s\n' "$app" >&2; exit 2; }
[[ "$(id -u)" != 0 ]] || { printf 'XProc simulation must run as an unprivileged user.\n' >&2; exit 2; }

mkdir -p "$artifact_dir"
harness_log="$artifact_dir/harness.log"
printf 'HARNESS_START backend=xvfb app_pid_pending=true\n' >"$harness_log"
runtime_dir="$(mktemp -d "${TMPDIR:-/tmp}/memreduct-p3c-xvfb.XXXXXX")"
chmod 0700 "$runtime_dir"
xvfb_pid=""
app_pid=""
cleanup() {
  local status="$?"
  trap - EXIT INT TERM
  if [[ -n "$app_pid" ]] && kill -0 "$app_pid" 2>/dev/null; then
    printf 'HARNESS_TEARDOWN child=app signal=SIGTERM reason=cleanup\n' >>"$harness_log"
    kill -TERM "$app_pid" 2>/dev/null || true
    local app_wait_status=0
    wait "$app_pid" 2>/dev/null || app_wait_status="$?"
    printf 'HARNESS_TEARDOWN child=app wait_status=%s\n' "$app_wait_status" >>"$harness_log"
  fi
  if [[ -n "$xvfb_pid" ]] && kill -0 "$xvfb_pid" 2>/dev/null; then
    printf 'HARNESS_TEARDOWN child=xvfb signal=SIGTERM reason=cleanup\n' >>"$harness_log"
    kill -TERM "$xvfb_pid" 2>/dev/null || true
    local xvfb_wait_status=0
    wait "$xvfb_pid" 2>/dev/null || xvfb_wait_status="$?"
    printf 'HARNESS_TEARDOWN child=xvfb wait_status=%s\n' "$xvfb_wait_status" >>"$harness_log"
  fi
  rm -rf -- "$runtime_dir"
  exit "$status"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'printf "HARNESS_SIGNAL source=external signal=SIGTERM exit=143\\n" >>"$harness_log"; exit 143' TERM

for cmd in Xvfb xdotool import sha256sum cmp grep tail; do
  command -v "$cmd" >/dev/null || { printf 'Missing command: %s\n' "$cmd" >&2; exit 2; }
done

display_file="$runtime_dir/display"
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp -noreset \
  3>"$display_file" >"$artifact_dir/xvfb.log" 2>&1 &
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
export HOME="$home_dir" XDG_CONFIG_HOME="$config_home"

app_log="$artifact_dir/app.log"
env HOME="$home_dir" XDG_CONFIG_HOME="$config_home" DISPLAY="$DISPLAY" \
  GDK_BACKEND=x11 GTK_THEME="$GTK_THEME" GDK_SCALE=1 GDK_DPI_SCALE=1 \
  GSETTINGS_BACKEND=memory XDG_RUNTIME_DIR="$runtime_dir" \
  MEMREDUCT_CI=1 MEMREDUCT_CI_CLEAN_SIMULATION=1 \
  MEMREDUCT_CI_XPROC_SIMULATION=1 MEMREDUCT_CI_EXIT_MS=120000 \
  stdbuf -oL "$app" >"$app_log" 2>&1 &
app_pid="$!"

window_id=""
for _ in {1..100}; do
  window_id="$(xdotool search --onlyvisible --name '^Mem Reduct$' 2>/dev/null | head -n 1 || true)"
  [[ -n "$window_id" ]] && break
  kill -0 "$app_pid" 2>/dev/null || break
  sleep 0.1
done
[[ -n "$window_id" ]] || { cat "$app_log" >&2; exit 1; }
xdotool windowfocus --sync "$window_id" || true

require() {
  local marker="$1"
  if ! grep -qF -- "$marker" "$app_log"; then
    printf 'Missing marker: %s\n' "$marker" >&2
    tail -n 50 "$app_log" >&2
    return 1
  fi
}
capture() {
  import -window "$window_id" "$artifact_dir/$1.png"
  sha256sum "$artifact_dir/$1.png" | tee -a "$artifact_dir/screenshots.sha256"
}
open_picker() {
  # On the headless runner the GTK client header bar adds 46 px above the canvas.
  xdotool mousemove --window "$window_id" 320 391
  xdotool click 1
  sleep 0.1
  xdotool mousemove --window "$window_id" 125 363
  xdotool click 1
  for _ in {1..50}; do
    grep -q 'CI_XPROC_PICKER open=true candidates=2' "$app_log" && break
    sleep 0.1
  done
  require 'CI_XPROC_PICKER open=true candidates=2'
  require 'CI_XPROC_PICKER open=true candidates=2 selected=-1 preflight=0 scan=0 mock=true no_privilege_request=true'
  require 'mock=true no_privilege_request=true'
  require 'CI_CLEAN_ACTION index=8 action=selected-process'
}
open_menu_from_button_body() {
  xdotool mousemove --window "$window_id" 125 391
  xdotool click 1
  require 'CI_CLEAN_MENU open=true source=button-body default_action=none'
  ! grep -q 'CI_CLEAN_CONFIRM open=true' "$app_log"
  xdotool key --clearmodifiers Down Return
  require 'CI_CLEAN_CONFIRM open=true mode=simulation'
  ! grep -q 'CI_XPROC_PICKER open=true' "$app_log"
  xdotool key --clearmodifiers Escape
  require 'CI_CLEAN_SIM result=confirmation-cancelled stage=confirmation backend=mock helper_called=false no_system_write=true'
  ! grep -q 'CI_CLEAN_SIM stage=confirmed' "$app_log"
}
open_picker_from_button_body() {
  xdotool mousemove --window "$window_id" 125 391
  xdotool click 1
  require 'CI_CLEAN_MENU open=true source=button-body default_action=none'
  ! grep -q 'CI_CLEAN_CONFIRM open=true' "$app_log"
  xdotool mousemove --window "$window_id" 125 363
  xdotool click 1
  require 'CI_CLEAN_ACTION index=8 action=selected-process'
  require 'CI_XPROC_PICKER open=true candidates=2 selected=-1 preflight=0 scan=0 mock=true no_privilege_request=true'
  ! grep -q 'CI_CLEAN_CONFIRM open=true' "$app_log"
  ! grep -q 'CI_CLEAN_SIM stage=confirmed' "$app_log"
  xdotool key --clearmodifiers Escape
  require 'CI_XPROC_PICKER open=false cancelled=true request_started=false'
}
select_candidate() {
  local index="$1" y="$2"
  xdotool mousemove --window "$window_id" 125 "$y"
  xdotool click 1
  require "CI_XPROC_SELECT index=$index"
  if [[ "$index" == 0 ]]; then
    require 'process=fixture-app app=fixture-app-bin process_state=sleeping'
  else
    require 'process=fixture-renderer app=fixture-renderer-bin process_state=sleeping'
  fi
}

for _ in {1..50}; do
  grep -q 'CI_MEMORY ' "$app_log" && break
  sleep 0.1
done
require 'CI_MEMORY source=/proc/meminfo'
require 'CI_CLEAN_SIM enabled=true mock_bridge_connected=true production_bridge_connected=false'
require 'CI_XPROC_SIM enabled=true mock_bridge_connected=true production_bridge_connected=false'

# The button body is an action selector too. Page cache is reached only after
# choosing its own row; it cannot appear as the default when the user wants a
# process action.
open_menu_from_button_body
open_picker_from_button_body

# Mouse selection followed by confirmation cancellation must not start a request.
open_picker
capture picker
select_candidate 0 173
xdotool mousemove --window "$window_id" 165 340
xdotool click 1
require 'CI_XPROC_CONFIRM open=true'
require 'explicit_user_choice=true'
require 'budget_limit_bytes=16777216 max_bytes=16777216'
capture confirm-cancel
# Raise this fixture request to 64 MiB with the mouse, then cancel it. No real
# candidate, helper, Polkit action, or kernel syscall is reachable in this mode.
xdotool mousemove --window "$window_id" 260 205
xdotool click 1
require 'CI_XPROC_BUDGET choice=2 limit_bytes=67108864 effective_bytes=67108864'
capture confirm-budget-64
start_count_before_cancel="$(grep -c 'CI_XPROC_START started=true' "$app_log" || true)"
xdotool key --clearmodifiers Escape
require 'CI_XPROC_RESULT state=confirmation-cancelled authorization_started=false call_count=0 mock=true'
[[ "$(grep -c 'CI_XPROC_START started=true' "$app_log" || true)" == "$start_count_before_cancel" ]]
xdotool key --clearmodifiers Escape
require 'CI_XPROC_RESULT closed=true'

# Keyboard Down selects the second fixed fixture; Tab reaches Continue; Enter opens confirmation.
open_picker
xdotool key --clearmodifiers Down
require 'CI_XPROC_SELECT index=0'
require 'process=fixture-app app=fixture-app-bin process_state=sleeping'
xdotool key --clearmodifiers Down
require 'CI_XPROC_SELECT index=1'
require 'process=fixture-renderer app=fixture-renderer-bin process_state=sleeping'
xdotool key --clearmodifiers Tab Tab Return
require 'CI_XPROC_CONFIRM open=true'
require 'pid=4243'
require 'budget_limit_bytes=16777216 max_bytes=16777216'
# Tab to the first budget, Space confirms 16 MiB, Right selects 32 MiB, then
# Tab advances to Run. This covers the keyboard budget path before mock start.
xdotool key --clearmodifiers Tab Tab space Right
require 'CI_XPROC_BUDGET choice=0 limit_bytes=16777216 effective_bytes=16777216'
require 'CI_XPROC_BUDGET choice=1 limit_bytes=33554432 effective_bytes=33554432'
capture confirm-run
xdotool key --clearmodifiers Tab Tab
xdotool key --clearmodifiers Return
require 'CI_XPROC_START started=true backend=mock'
require 'pid=4243'
require 'max_bytes=33554432'
require 'automatic_retry=false'

for _ in {1..80}; do
  grep -q 'CI_XPROC_SIM stage=mock-helper-running authorization_simulated=true helper=mock no_syscall=true' "$app_log" && break
  sleep 0.1
done
require 'CI_XPROC_SIM stage=mock-helper-running authorization_simulated=true helper=mock no_syscall=true'
capture mock-helper
for _ in {1..100}; do
  grep -q 'CI_XPROC_RESULT state=partial .*call_count=0 .*mock=true no_system_write=true' "$app_log" && break
  sleep 0.1
done
grep 'CI_XPROC_RESULT state=partial ' "$app_log" | tee "$artifact_dir/result.txt"
require 'advised_bytes=16777216 requested_bytes=33554432 regions=2 error_code=0 call_count=0'
require 'memavailable_before_kib='
require 'memavailable_after_kib='
require 'causal_attribution=none no_retry=true mock=true no_system_write=true'
capture result
cmp -s "$artifact_dir/picker.png" "$artifact_dir/confirm-run.png" && {
  printf 'Picker and confirmation screenshots unexpectedly match.\n' >&2; exit 1;
}
cmp -s "$artifact_dir/confirm-run.png" "$artifact_dir/result.png" && {
  printf 'Confirmation and result screenshots unexpectedly match.\n' >&2; exit 1;
}

# Escape closes the result and picker; the GTK header-bar button closes the app normally.
xdotool key --clearmodifiers Escape
require 'CI_XPROC_RESULT closed=true'
open_picker
xdotool key --clearmodifiers Escape
require 'CI_XPROC_PICKER open=false cancelled=true request_started=false'
xdotool mousemove --window "$window_id" 309 28
xdotool click 1
for _ in {1..100}; do
  kill -0 "$app_pid" 2>/dev/null || break
  sleep 0.1
done
if kill -0 "$app_pid" 2>/dev/null; then
  printf 'Application did not close through its GTK title-bar button.\n' >&2
  tail -n 50 "$app_log" >&2
  exit 1
fi
set +e
wait "$app_pid"
app_status="$?"
set -e
[[ "$app_status" -eq 0 ]] || { printf 'Application exited with %s.\n' "$app_status" >&2; exit 1; }
app_pid=""
printf 'HARNESS_APP_EXIT status=%s method=gtk-window-close signal=none\n' "$app_status" >>"$harness_log"
require 'CI_WINDOW_CLOSE'
require 'CI_APP_RETURNED'
! grep -Eq 'pkexec|process_madvise|drop_caches' "$app_log"
if grep -Eq 'Gdk-(WARNING|CRITICAL)|GLib-(WARNING|CRITICAL)|GLib-GObject-(WARNING|CRITICAL)|Gtk-(WARNING|CRITICAL)' "$app_log"; then
  printf 'GTK warning or critical diagnostics were emitted.\n' >&2
  grep -E 'Gdk-(WARNING|CRITICAL)|GLib-(WARNING|CRITICAL)|GLib-GObject-(WARNING|CRITICAL)|Gtk-(WARNING|CRITICAL)' "$app_log" >&2
  exit 1
fi

printf 'XPROC_GUI_SIMULATION=PASS\n'
printf 'app_exit=%s syscall_count=0 no_system_write=true\n' "$app_status"
printf 'evidence=%s\n' "$artifact_dir"
