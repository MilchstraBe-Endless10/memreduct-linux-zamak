#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -Eeuo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
artifact_dir="${ARTIFACT_DIR:-$root/artifacts/p2b-x11}"
app="${1:-$artifact_dir/memreduct-zamak-ci-test}"
mkdir -p "$artifact_dir"
cd "$root"
[[ "$(id -u)" != 0 ]] || { printf 'P2B Xvfb simulation must run as an unprivileged user.\n' >&2; exit 2; }

for cmd in Xvfb openbox xdotool xwininfo xprop import identify compare xdpyinfo xrdb fc-match; do
  command -v "$cmd" >/dev/null || { printf 'Missing X11 test command: %s\n' "$cmd" >&2; exit 2; }
done
[[ -x "$app" ]] || { printf 'Production executable missing.\n' >&2; exit 2; }

runtime_dir="$(mktemp -d "${RUNNER_TEMP:-/tmp}/memreduct-p2b-x11.XXXXXX")"
chmod 0700 "$runtime_dir"
sample_failure_trigger="$runtime_dir/sample-failure.trigger"
xvfb_pid=""
wm_pid=""
app_pid=""
stop_pid() {
  local pid="$1" i
  [[ -n "$pid" ]] || return 0
  if kill -0 "$pid" 2>/dev/null; then
    kill -TERM "$pid" 2>/dev/null || true
    for i in {1..40}; do kill -0 "$pid" 2>/dev/null || break; sleep 0.1; done
    if kill -0 "$pid" 2>/dev/null; then kill -KILL "$pid" 2>/dev/null || true; fi
    wait "$pid" 2>/dev/null || true
  fi
}
cleanup() {
  local status="$?"
  trap - EXIT INT TERM
  stop_pid "$app_pid"
  stop_pid "$wm_pid"
  stop_pid "$xvfb_pid"
  rm -rf -- "$runtime_dir"
  exit "$status"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

display_file="$runtime_dir/display-number"
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp -noreset \
  3>"$display_file" >"$artifact_dir/xvfb.log" 2>&1 &
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
export GSETTINGS_BACKEND=memory
export XDG_RUNTIME_DIR="$runtime_dir"
mkdir -m 0700 "$runtime_dir/home" "$runtime_dir/config"
export HOME="$runtime_dir/home" XDG_CONFIG_HOME="$runtime_dir/config"
printf 'mode=%s\nDISPLAY=%s\nGDK_BACKEND=%s\nGTK_THEME=%s\nGDK_SCALE=%s\nGDK_DPI_SCALE=%s\nXvfb_screen=1024x768x24\n' \
  "$(stat -c '%a' "$runtime_dir")" "$DISPLAY" "$GDK_BACKEND" "$GTK_THEME" "$GDK_SCALE" "$GDK_DPI_SCALE" \
  | tee "$artifact_dir/environment.txt"
fc-match 'sans-serif:lang=zh-cn' | tee "$artifact_dir/chinese-font.txt"
xdpyinfo -queryExtensions | grep -E '(^|[[:space:]])XTEST([[:space:]]|$)' \
  | tee "$artifact_dir/xtest-extension.txt"
printf 'Xft.dpi: 96\nXft.antialias: 1\nXft.hinting: 1\n' | xrdb -merge

openbox --sm-disable >"$artifact_dir/openbox.log" 2>&1 &
wm_pid="$!"
wm_ready=false
for _ in {1..50}; do
  if xprop -root _NET_SUPPORTING_WM_CHECK 2>/dev/null | grep -q 'window id'; then wm_ready=true; break; fi
  kill -0 "$wm_pid" 2>/dev/null || break
  sleep 0.1
done
[[ "$wm_ready" == true ]] || { cat "$artifact_dir/openbox.log" >&2; exit 1; }

MEMREDUCT_CI=1 MEMREDUCT_CI_EXIT_MS=60000 MEMREDUCT_CI_SAMPLE_FAILURE_TRIGGER_FILE="$sample_failure_trigger" MEMREDUCT_CI_CAPTURE=1 stdbuf -oL "$app" \
  >"$artifact_dir/client.log" 2>&1 &
app_pid="$!"
window_id=""
for _ in {1..100}; do
  window_id="$(xdotool search --onlyvisible --name '^Mem Reduct$' 2>/dev/null | head -n 1 || true)"
  [[ -n "$window_id" ]] && break
  kill -0 "$app_pid" 2>/dev/null || break
  sleep 0.1
done
[[ -n "$window_id" ]] || { cat "$artifact_dir/client.log" >&2; exit 1; }

wait_for_marker() {
  local marker="$1"
  for _ in {1..60}; do
    grep -Fq -- "$marker" "$artifact_dir/client.log" && return 0
    kill -0 "$app_pid" 2>/dev/null || break
    sleep 0.1
  done
  printf 'Missing production UI event marker: %s\n' "$marker" >&2
  cat "$artifact_dir/client.log" >&2
  return 1
}
wait_for_marker_long() {
  local marker="$1"
  for _ in {1..200}; do
    grep -Fq -- "$marker" "$artifact_dir/client.log" && return 0
    kill -0 "$app_pid" 2>/dev/null || break
    sleep 0.1
  done
  printf 'Missing delayed production UI event marker: %s\n' "$marker" >&2
  cat "$artifact_dir/client.log" >&2
  return 1
}
wait_for_new_marker() {
  local marker="$1" previous_count="$2" count
  for _ in {1..100}; do
    count="$(grep -Fc -- "$marker" "$artifact_dir/client.log" || true)"
    if (( count > previous_count )); then return 0; fi
    kill -0 "$app_pid" 2>/dev/null || break
    sleep 0.1
  done
  printf 'Missing new production UI event marker: %s\n' "$marker" >&2
  cat "$artifact_dir/client.log" >&2
  return 1
}
escape_and_expect_focus() {
  local focus="$1" marker="CI_MENU closed reason=Escape focus=$1" previous_count
  previous_count="$(grep -Fc -- "$marker" "$artifact_dir/client.log" || true)"
  xdotool key --clearmodifiers Escape
  wait_for_new_marker "$marker" "$previous_count"
}
window_geometry() {
  local output="$1" width height canvas
  xwininfo -id "$window_id" >"$output"
  width="$(awk '$1 == "Width:" {print $2; exit}' "$output")"
  height="$(awk '$1 == "Height:" {print $2; exit}' "$output")"
  canvas="$(grep -F 'CI_DRAW width=' "$artifact_dir/client.log" | tail -n 1 | sed -E 's/^.*CI_DRAW width=([^ ]+) height=([^ ]+).*$/\1x\2/')"
  printf 'Observed top-level=%sx%s; Canvas=%s\n' "$width" "$height" "$canvas" | tee -a "$output"
  [[ "$width" == 337 && "$height" == 369 && "$canvas" == '325.0x357.0' ]] || {
    printf 'Unexpected candidate geometry. Expected top-level 337x369 and Canvas 325x357, observed %sx%s / %s.\n' \
      "$width" "$height" "$canvas" >&2
    return 1
  }
}
capture() {
  import -window "$window_id" "$artifact_dir/$1"
  [[ "$(identify -format '%wx%h' "$artifact_dir/$1")" == '337x369' ]] || {
    printf 'Unexpected window screenshot dimensions for %s.\n' "$1" >&2
    return 1
  }
}
assert_changed() {
  local first="$1" second="$2" report="$3" status pixels
  set +e
  compare -metric AE "$artifact_dir/$first" "$artifact_dir/$second" null: 2>"$artifact_dir/$report"
  status="$?"
  set -e
  pixels="$(sed -E 's/^[[:space:]]*([0-9]+).*/\1/' "$artifact_dir/$report")"
  [[ "$status" == 1 && "$pixels" =~ ^[0-9]+$ ]] && (( pixels > 0 )) || {
    printf 'Expected visible pixel change between %s and %s.\n' "$first" "$second" >&2
    return 1
  }
  printf 'changed_pixels=%s\n' "$pixels" | tee -a "$artifact_dir/$report"
}

wait_for_marker 'CI_PRODUCTION_START'
wait_for_marker 'CI_MEMORY source=/proc/meminfo'
wait_for_marker 'CI_DRAW width='
wait_for_marker 'CI_WINDOW_SIZE_POLICY fixed=1'
wait_for_marker 'CI_MAIN_BACKGROUND mode=light applied=1'
wait_for_marker 'CI_MAIN_CONTENT_INSET old=12 new=0 margins=start:12,end:0,top:0,bottom:12 verified=true'
wait_for_marker 'CI_WINDOW_SIZE_POLICY main_found=1 main_resizable=0 settings_found=1 settings_resizable=0'
wait_for_marker 'CI_CAPTURE kind=main status=ok'
wait_for_marker 'CI_CAPTURE kind=settings status=skipped visible=0 mapped=0'
if grep -Eq 'Gtk-(WARNING|CRITICAL)' "$artifact_dir/client.log"; then
  echo 'Visible-window capture emitted a GTK warning or critical' >&2
  exit 1
fi
for group in 0 1 2; do
  wait_for_marker "CI_LAYOUT_GROUP index=$group locale=en "
  grep -Fq "CI_LAYOUT_GROUP index=$group locale=en " "$artifact_dir/client.log"
done
grep -Eq 'CI_LAYOUT_GROUP index=0 locale=en .*title_y=36\.[0-9]+ rule_x=[0-9.]+ rule_y=44\.[0-9]+ row0=58\.[0-9]+ row1=76\.[0-9]+ row2=94\.[0-9]+' "$artifact_dir/client.log"
grep -Eq 'CI_LAYOUT_GROUP index=1 locale=en .*title_y=117\.[0-9]+ rule_x=[0-9.]+ rule_y=122\.[0-9]+ row0=136\.[0-9]+ row1=153\.[0-9]+ row2=171\.[0-9]+' "$artifact_dir/client.log"
grep -Eq 'CI_LAYOUT_GROUP index=2 locale=en .*title_y=192\.[0-9]+ rule_x=[0-9.]+ rule_y=200\.[0-9]+ row0=210\.[0-9]+ row1=228\.[0-9]+ row2=246\.[0-9]+' "$artifact_dir/client.log"
grep -Fq 'CI_LAYOUT_GROUP index=1 locale=en title=Pagefile / Swap ' "$artifact_dir/client.log"
grep -Eq 'CI_LAYOUT_GROUP index=0 locale=en title=Physical memory collapsed=false usage=[0-9]+\.[0-9]{3}%' "$artifact_dir/client.log"
window_geometry "$artifact_dir/geometry-en.txt"
# The upstream IDD_MAIN has no resizable frame. Ask the isolated WM to tile the
# window to the same large geometry observed on Hyprland and require the GTK
# client to keep the authenticated compact candidate allocation.
xdotool windowsize "$window_id" 926 1010
sleep 0.2
window_geometry "$artifact_dir/geometry-after-tiling-request.txt"
# Preserve an unselected capture, then reproduce the selected third group shown
# in the official Windows reference before recording the comparison screenshot.
capture 'production-main-en-default.png'
marker='CI_GROUP_SELECT group=2 selected=true'
previous_count="$(grep -Fc -- "$marker" "$artifact_dir/client.log" || true)"
xdotool mousemove --sync --window "$window_id" 120 200 click 1
wait_for_new_marker "$marker" "$previous_count"
capture 'production-main-en.png'
assert_changed 'production-main-en-default.png' 'production-main-en.png' 'group-selection-pixel-diff.txt'

# The v3.5.2 list uses collapsible groups. Exercise the disclosure affordance
# in both directions and require the redraw/state marker from the production UI.
marker='CI_GROUP_COLLAPSE group=0 collapsed=true'
previous_count="$(grep -Fc -- "$marker" "$artifact_dir/client.log" || true)"
xdotool mousemove --sync --window "$window_id" 320 44 click 1
wait_for_new_marker "$marker" "$previous_count"
grep -Fq 'CI_LAYOUT_GROUP index=0 locale=en title=Physical memory collapsed=true ' "$artifact_dir/client.log"
capture 'production-main-en-first-group-collapsed.png'
marker='CI_GROUP_COLLAPSE group=0 collapsed=false'
previous_count="$(grep -Fc -- "$marker" "$artifact_dir/client.log" || true)"
xdotool mousemove --sync --window "$window_id" 320 44 click 1
wait_for_new_marker "$marker" "$previous_count"
grep -Fq 'CI_LAYOUT_GROUP index=0 locale=en title=Physical memory collapsed=false ' "$artifact_dir/client.log"

# Open each production root menu through XTEST and assert its final popup bounds.
marker='CI_MENU root=1 open=true'
previous_count="$(grep -Fc -- "$marker" "$artifact_dir/client.log" || true)"
xdotool mousemove --sync --window "$window_id" 20 10 click 1
wait_for_marker 'CI_POPUP kind=root'
wait_for_new_marker "$marker" "$previous_count"
escape_and_expect_focus 0
marker='CI_MENU root=2 open=true'
previous_count="$(grep -Fc -- "$marker" "$artifact_dir/client.log" || true)"
xdotool mousemove --sync --window "$window_id" 70 10 click 1
wait_for_new_marker "$marker" "$previous_count"
escape_and_expect_focus 1
marker='CI_MENU root=3 open=true'
previous_count="$(grep -Fc -- "$marker" "$artifact_dir/client.log" || true)"
xdotool mousemove --sync --window "$window_id" 120 10 click 1
wait_for_new_marker "$marker" "$previous_count"
escape_and_expect_focus 2
marker='CI_MENU root=4 open=true'
previous_count="$(grep -Fc -- "$marker" "$artifact_dir/client.log" || true)"
xdotool mousemove --sync --window "$window_id" 174 10 click 1
wait_for_new_marker "$marker" "$previous_count"
escape_and_expect_focus 3
marker='CI_MENU root=2 open=true'
previous_count="$(grep -Fc -- "$marker" "$artifact_dir/client.log" || true)"
xdotool mousemove --sync --window "$window_id" 70 10 click 1
wait_for_new_marker "$marker" "$previous_count"
capture 'production-view-menu-en.png'
marker='CI_LANGUAGE_MENU open=true'
previous_count="$(grep -Fc -- "$marker" "$artifact_dir/client.log" || true)"
xdotool mousemove --sync --window "$window_id" 82 118 click 1
wait_for_new_marker "$marker" "$previous_count"
wait_for_marker 'CI_POPUP kind=language menu=2 locale=en'
capture 'production-language-menu-en.png'
# XTEST uses top-level-relative coordinates. The second language row is centered
# at Canvas y=134; the main Canvas now starts at the client top edge.
xdotool mousemove --sync --window "$window_id" 238 134 click 1
wait_for_marker 'CI_LOCALE locale=zh-CN'
sleep 0.2
window_geometry "$artifact_dir/geometry-zh.txt"
capture 'production-main-zh.png'
assert_changed 'production-main-en.png' 'production-main-zh.png' 'locale-pixel-diff.txt'
for group in 0 1 2; do
  wait_for_marker "CI_LAYOUT_GROUP index=$group locale=zh-CN "
  grep -Fq "CI_LAYOUT_GROUP index=$group locale=zh-CN " "$artifact_dir/client.log"
done
grep -Eq 'CI_LAYOUT_GROUP index=0 locale=zh-CN .*title_y=36\.[0-9]+ rule_x=[0-9.]+ rule_y=44\.[0-9]+ row0=58\.[0-9]+ row1=76\.[0-9]+ row2=94\.[0-9]+' "$artifact_dir/client.log"
grep -Eq 'CI_LAYOUT_GROUP index=1 locale=zh-CN .*title_y=117\.[0-9]+ rule_x=[0-9.]+ rule_y=122\.[0-9]+ row0=136\.[0-9]+ row1=153\.[0-9]+ row2=171\.[0-9]+' "$artifact_dir/client.log"
grep -Eq 'CI_LAYOUT_GROUP index=2 locale=zh-CN .*title_y=192\.[0-9]+ rule_x=[0-9.]+ rule_y=200\.[0-9]+ row0=210\.[0-9]+ row1=228\.[0-9]+ row2=246\.[0-9]+' "$artifact_dir/client.log"
grep -Fq 'CI_LAYOUT_GROUP index=1 locale=zh-CN title=交换空间 ' "$artifact_dir/client.log"
marker='CI_MENU root=2 open=true'
previous_count="$(grep -Fc -- "$marker" "$artifact_dir/client.log" || true)"
xdotool mousemove --sync --window "$window_id" 70 10 click 1
wait_for_new_marker "$marker" "$previous_count"
marker='CI_LANGUAGE_MENU open=true'
previous_count="$(grep -Fc -- "$marker" "$artifact_dir/client.log" || true)"
popup_marker='CI_POPUP kind=language menu=2 locale=zh-CN'
popup_previous_count="$(grep -Fc -- "$popup_marker" "$artifact_dir/client.log" || true)"
xdotool mousemove --sync --window "$window_id" 82 118 click 1
wait_for_new_marker "$marker" "$previous_count"
wait_for_new_marker "$popup_marker" "$popup_previous_count"
sleep 0.2
capture 'production-language-menu-zh.png'
escape_and_expect_focus 1
menu_index=1
for menu_x in 20 70 120 174; do
  marker="CI_POPUP kind=root menu=$menu_index locale=zh-CN"
  previous_count="$(grep -Fc -- "$marker" "$artifact_dir/client.log" || true)"
  xdotool mousemove --sync --window "$window_id" "$menu_x" 10 click 1
  wait_for_new_marker "$marker" "$previous_count"
  escape_and_expect_focus "$((menu_index - 1))"
  menu_index=$((menu_index + 1))
done

xdotool mousemove --sync --window "$window_id" 320 345 click 1
wait_for_marker 'CI_CLEAN_MENU open=true'
wait_for_marker 'CI_POPUP kind=clean'
# The first popup row center follows the 96 DPI Windows button baseline.
capture 'production-clean-menu-zh.png'
xdotool mousemove --sync --window "$window_id" 95 170 click 1
wait_for_marker 'CI_CLEAN_REGION_DISABLED index=0 action=none'
xdotool mousemove --sync --window "$window_id" 112 345 click 1
wait_for_marker 'CI_CLEAN_DISABLED action=none'

# Keyboard focus, menu navigation, Enter and Escape run in the same production Canvas.
xdotool key --clearmodifiers Tab
wait_for_marker 'CI_FOCUS value='
xdotool key --clearmodifiers Right
xdotool key --clearmodifiers Down
xdotool key --clearmodifiers Down
xdotool key --clearmodifiers Return
wait_for_marker 'CI_THEME dark=true'
wait_for_marker 'CI_MAIN_BACKGROUND mode=dark applied=1'
marker='CI_FOCUS item=201.0'
previous_count="$(grep -Fc -- "$marker" "$artifact_dir/client.log" || true)"
xdotool key --clearmodifiers Down
wait_for_new_marker "$marker" "$previous_count"
escape_and_expect_focus 1

# The private CI-only gate is opened explicitly after live values have been
# rendered and the before-state screenshot has been captured. No wall-clock
# threshold can race this interaction sequence.
grep -Eq 'CI_MEMORY source=/proc/meminfo MemTotal=[1-9][0-9]* MemAvailable=[0-9]+' "$artifact_dir/client.log" || {
  printf 'No successful live /proc/meminfo sample exists before the fault-injection checkpoint.\n' >&2
  exit 1
}
grep -Eq 'CI_LAYOUT_GROUP index=0 locale=zh-CN .*usage=[0-9]+\.[0-9]+% available=[0-9]+\.[0-9]+ [KMGT]B total=[0-9]+\.[0-9]+ [KMGT]B' "$artifact_dir/client.log" || {
  printf 'No live memory values were rendered in the Simplified Chinese main window before injection.\n' >&2
  exit 1
}
if grep -Eq 'CI_SAMPLE_FAILURE_TRIGGER|CI_MEMORY_INVALIDATED|CI_MEMORY_ERROR' "$artifact_dir/client.log"; then
  printf 'The controlled sampling failure gate fired before its explicit trigger.\n' >&2
  exit 1
fi
capture 'production-memory-before-failure-zh.png'
pre_failure_sha="$(sha256sum "$artifact_dir/production-memory-before-failure-zh.png" | awk '{print $1}')"
printf 'stage=before-trigger state=live-meminfo screenshot=production-memory-before-failure-zh.png sha256=%s\n' \
  "$pre_failure_sha" | tee "$artifact_dir/sample-failure-sequence.txt"
trigger_staging_file="$(mktemp "$runtime_dir/sample-failure-trigger.XXXXXX")"
printf 'P2B_FAIL_SAMPLE_ONCE\n' >"$trigger_staging_file"
mv -T -- "$trigger_staging_file" "$sample_failure_trigger"
printf 'stage=trigger-created source=private-file mode=%s\n' "$(stat -c '%a' "$sample_failure_trigger")" \
  | tee -a "$artifact_dir/sample-failure-sequence.txt"
wait_for_new_marker 'CI_SAMPLE_FAILURE_TRIGGER consumed=1 source=private-trigger-file' 0
wait_for_new_marker 'CI_MEMORY_ERROR injected non-privileged CI sample failure' 0
wait_for_new_marker 'CI_MEMORY_INVALIDATED redraw_requested=true' 0
wait_for_new_marker 'CI_DRAW_SAMPLE state=unavailable values=em-dash' 0
capture 'production-sample-failure-zh.png'
post_failure_sha="$(sha256sum "$artifact_dir/production-sample-failure-zh.png" | awk '{print $1}')"
printf 'stage=after-trigger state=unavailable screenshot=production-sample-failure-zh.png sha256=%s\n' \
  "$post_failure_sha" | tee -a "$artifact_dir/sample-failure-sequence.txt"
convert "$artifact_dir/production-memory-before-failure-zh.png" -crop 290x245+15+40 +repage "$artifact_dir/memory-before-failure.png"
convert "$artifact_dir/production-sample-failure-zh.png" -crop 290x245+15+40 +repage "$artifact_dir/memory-after-failure.png"
assert_changed 'memory-before-failure.png' 'memory-after-failure.png' 'memory-failure-pixel-diff.txt'

grep -Eq 'CI_MEMORY source=/proc/meminfo MemTotal=[1-9][0-9]* MemAvailable=[0-9]+' "$artifact_dir/client.log"
memory_errors="$(grep '^CI_MEMORY_ERROR ' "$artifact_dir/client.log" || true)"
[[ -n "$memory_errors" ]] || {
  printf 'The injected sampling failure was not observed.\n' >&2
  exit 1
}
[[ "$(grep -Fc 'CI_SAMPLE_FAILURE_TRIGGER consumed=1 source=private-trigger-file' "$artifact_dir/client.log")" == 1 ]] || {
  printf 'The explicit sampling trigger must be consumed exactly once.\n' >&2
  exit 1
}
[[ "$(grep -Fc 'CI_MEMORY_ERROR injected non-privileged CI sample failure' "$artifact_dir/client.log")" == 1 ]] || {
  printf 'The one-shot injection must produce exactly one sample failure.\n' >&2
  exit 1
}
if printf '%s\n' "$memory_errors" | grep -Fvxq 'CI_MEMORY_ERROR injected non-privileged CI sample failure'; then
  printf 'Unexpected production memory sampling error:\n%s\n' "$memory_errors" >&2
  exit 1
fi
awk '
  /^CI_SAMPLE_FAILURE_TRIGGER consumed=1 source=private-trigger-file$/ { injected = 1; next }
  injected && /^CI_MEMORY source=\/proc\/meminfo / { exit 1 }
  END { if (!injected) exit 2 }
' "$artifact_dir/client.log" || {
  printf 'A live sample ran after the injected failure instead of preserving the unavailable test state.\n' >&2
  exit 1
}
grep -Fq 'CI_CLEAN_DISABLED action=none' "$artifact_dir/client.log"
grep -Fq 'CI_CLEAN_REGION_DISABLED index=0 action=none' "$artifact_dir/client.log"
if grep -Eiq 'drop_caches|pkexec|polkit|swapoff' "$artifact_dir/client.log"; then
  printf 'Unexpected privileged-operation marker in production UI log.\n' >&2
  exit 1
fi
if grep -Fq 'CI_POPUP ' "$artifact_dir/client.log" && grep -Fq 'fits=false' "$artifact_dir/client.log"; then
  printf 'A production popup exceeded the Canvas safe area.\n' >&2
  exit 1
fi
for kind in root language clean; do
  grep -Fq "CI_POPUP kind=$kind " "$artifact_dir/client.log"
done
grep -Fq 'CI_MEMORY_ERROR injected non-privileged CI sample failure' "$artifact_dir/client.log"
grep -Fq 'CI_MEMORY_INVALIDATED redraw_requested=true' "$artifact_dir/client.log"
grep -Fq 'CI_DRAW_SAMPLE state=unavailable values=em-dash' "$artifact_dir/client.log"

xprop -id "$window_id" WM_PROTOCOLS | tee "$artifact_dir/window-protocols.txt"
grep -q 'WM_DELETE_WINDOW' "$artifact_dir/window-protocols.txt"
xdotool windowfocus --sync "$window_id"
xdotool key --clearmodifiers Alt+F4
wait_for_marker 'CI_WINDOW_CLOSE'
wait_for_marker 'CI_APP_RETURNED'
app_finished=false
for _ in {1..50}; do
  if ! kill -0 "$app_pid" 2>/dev/null; then app_finished=true; break; fi
  sleep 0.1
done
[[ "$app_finished" == true ]] || { printf 'Production GUI did not exit after WM close.\n' >&2; exit 1; }
set +e
wait "$app_pid"
app_status="$?"
set -e
app_pid=""
printf 'Production GUI WM-close exit status: %s\n' "$app_status" | tee "$artifact_dir/client-exit.txt"
[[ "$app_status" == 0 ]] || exit "$app_status"
if xdotool getwindowname "$window_id" >"$artifact_dir/window-after-close.txt" 2>&1; then
  printf 'Production X11 window still exists after process exit.\n' >&2
  exit 1
elif grep -Fq 'BadWindow' "$artifact_dir/window-after-close.txt"; then
  printf 'PASS: X11 window was destroyed with the application.\n' | tee -a "$artifact_dir/window-after-close.txt"
else
  printf 'Could not verify that the production X11 window was destroyed.\n' >&2
  cat "$artifact_dir/window-after-close.txt" >&2
  exit 1
fi

# Exercise the asynchronous production bridge through a private same-UID
# authorizer and MEMREDUCT_TESTING helper. Every target is below runtime_dir;
# no pkexec, installed helper, or kernel path is reachable from these tests.
test_helper="$runtime_dir/memreduct-zamak-helper-test"
cc -std=c11 -O2 -Wall -Wextra -Werror -pedantic -DMEMREDUCT_TESTING \
  native/memreduct_helper.c tests/native_write_shim.c \
  -Wl,--wrap=write -Wl,--wrap=close -o "$test_helper"
mock_authorizer="$runtime_dir/mock-authorizer"
cat >"$mock_authorizer" <<'MOCK_AUTHORIZER'
#!/usr/bin/env bash
set -Eeuo pipefail
[[ "$#" == 2 && "$1" == /usr/libexec/memreduct-zamak-helper && "$2" == pagecache ]] || exit 127
[[ -f "$MEMREDUCT_CI_MOCK_STAGE_FILE" && ! -L "$MEMREDUCT_CI_MOCK_STAGE_FILE" ]] || exit 127
[[ "$(stat -c '%a' "$MEMREDUCT_CI_MOCK_STAGE_FILE")" == 600 ]] || exit 127
printf '%s %s\n' "${MEMREDUCT_CI_MOCK_SCENARIO:-success}" "$(date +%s%N)" >>"$MEMREDUCT_CI_MOCK_INVOCATIONS"
wait_for_ack() {
  local ack="$1"
  # Screenshot capture and XTEST delivery can take longer than one short
  # polling window on loaded runners. Keep the fake authorization alive while
  # the GUI process is alive, but bound the test-only handshake to 60 seconds.
  for _ in {1..1200}; do
    [[ -f "$ack" && ! -L "$ack" ]] && return 0
    kill -0 "$PPID" 2>/dev/null || exit 127
    sleep 0.05
  done
  exit 127
}
case "${MEMREDUCT_CI_MOCK_SCENARIO:-success}" in
  authorization-cancelled) exit 126 ;;
  authorization-failed) exit 127 ;;
  authorization-signalled) kill -TERM "$$" ;;
  success|helper-failed|helper-unsupported|uncertain) ;;
  *) exit 127 ;;
esac
wait_for_ack "$MEMREDUCT_CI_MOCK_START_ACK"
printf 'authorized\n' >"$MEMREDUCT_CI_MOCK_STAGE_FILE"
wait_for_ack "$MEMREDUCT_CI_MOCK_AUTHORIZED_ACK"
printf 'helper-running\n' >"$MEMREDUCT_CI_MOCK_STAGE_FILE"
wait_for_ack "$MEMREDUCT_CI_MOCK_HELPER_ACK"
case "${MEMREDUCT_CI_MOCK_SCENARIO:-success}" in
  helper-failed)
    export MEMREDUCT_TEST_DROP_CACHES_PATH="$MEMREDUCT_CI_MOCK_TARGET_DIR"
    unset MEMREDUCT_TEST_WRITE_FAULT || true
    ;;
  helper-unsupported)
    export MEMREDUCT_TEST_DROP_CACHES_PATH=''
    unset MEMREDUCT_TEST_WRITE_FAULT || true
    ;;
  uncertain)
    export MEMREDUCT_TEST_DROP_CACHES_PATH="$MEMREDUCT_CI_MOCK_TARGET_FILE"
    export MEMREDUCT_TEST_WRITE_FAULT=short-once
    ;;
  *)
    export MEMREDUCT_TEST_DROP_CACHES_PATH="$MEMREDUCT_CI_MOCK_TARGET_FILE"
    unset MEMREDUCT_TEST_WRITE_FAULT || true
    ;;
esac
exec "$MEMREDUCT_CI_MOCK_TEST_HELPER" "$2"
MOCK_AUTHORIZER
chmod 0700 "$mock_authorizer"

wait_for_sim_marker() {
  local log="$1" marker="$2" pid="$3"
  for _ in {1..120}; do
    grep -Fq -- "$marker" "$log" && return 0
    kill -0 "$pid" 2>/dev/null || break
    sleep 0.1
  done
  printf 'Missing clean simulation event marker: %s\n' "$marker" >&2
  cat "$log" >&2
  return 1
}
wait_for_sim_result() {
  wait_for_sim_marker "$1" "CI_CLEAN_SIM result=$2" "$3"
}
finish_simulation_app() {
  local log="$1" label="$2" status finished=false
  xdotool windowfocus --sync "$window_id"
  xdotool key --clearmodifiers Alt+F4
  wait_for_sim_marker "$log" 'CI_WINDOW_CLOSE' "$app_pid"
  wait_for_sim_marker "$log" 'CI_APP_RETURNED' "$app_pid"
  for _ in {1..50}; do
    if ! kill -0 "$app_pid" 2>/dev/null; then finished=true; break; fi
    sleep 0.1
  done
  [[ "$finished" == true ]] || { printf '%s GUI did not exit after WM close.\n' "$label" >&2; return 1; }
  set +e
  wait "$app_pid"
  status="$?"
  set -e
  app_pid=""
  printf '%s WM-close exit status: %s\n' "$label" "$status" | tee "$artifact_dir/$label-exit.txt"
  [[ "$status" == 0 ]] || return "$status"
}
close_simulation_window() {
  local log="$1" name="$2"
  finish_simulation_app "$log" "$name"
  if xdotool getwindowname "$window_id" >"$artifact_dir/$name-window-after-close.txt" 2>&1; then
    printf '%s window still exists after process exit.\n' "$name" >&2
    return 1
  elif ! grep -Fq 'BadWindow' "$artifact_dir/$name-window-after-close.txt"; then
    printf 'Could not verify %s window destruction.\n' "$name" >&2
    cat "$artifact_dir/$name-window-after-close.txt" >&2
    return 1
  fi
  window_id=""
}
start_clean_simulation() {
  local name="$1" scenario="$2" case_dir="$runtime_dir/$1"
  local log="$artifact_dir/$name.log"
  mkdir -m 0700 "$case_dir"
  sim_stage_file="$case_dir/mock-stage"
  sim_invocations="$case_dir/authorizer-invocations"
  sim_start_ack="$case_dir/start.ack"
  sim_authorized_ack="$case_dir/authorized.ack"
  sim_helper_ack="$case_dir/helper-running.ack"
  sim_target_file="$case_dir/drop-caches-target"
  sim_target_dir="$case_dir/not-a-file"
  : >"$sim_stage_file"
  chmod 0600 "$sim_stage_file"
  : >"$sim_target_file"
  chmod 0600 "$sim_target_file"
  mkdir "$sim_target_dir"
  MEMREDUCT_CI=1 MEMREDUCT_CI_EXIT_MS=180000 MEMREDUCT_CI_CLEAN_SIMULATION=1 \
    MEMREDUCT_CI_MOCK_AUTHORIZER="$mock_authorizer" MEMREDUCT_CI_MOCK_STAGE_FILE="$sim_stage_file" \
    MEMREDUCT_CI_MOCK_SCENARIO="$scenario" MEMREDUCT_CI_MOCK_INVOCATIONS="$sim_invocations" \
    MEMREDUCT_CI_MOCK_START_ACK="$sim_start_ack" MEMREDUCT_CI_MOCK_AUTHORIZED_ACK="$sim_authorized_ack" \
    MEMREDUCT_CI_MOCK_HELPER_ACK="$sim_helper_ack" \
    MEMREDUCT_CI_MOCK_TEST_HELPER="$test_helper" MEMREDUCT_CI_MOCK_TARGET_FILE="$sim_target_file" \
    MEMREDUCT_CI_MOCK_TARGET_DIR="$sim_target_dir" stdbuf -oL "$app" >"$log" 2>&1 &
  app_pid="$!"
  window_id=""
  for _ in {1..100}; do
    window_id="$(xdotool search --onlyvisible --name '^Mem Reduct$' 2>/dev/null | head -n 1 || true)"
    [[ -n "$window_id" ]] && break
    kill -0 "$app_pid" 2>/dev/null || break
    sleep 0.1
  done
  [[ -n "$window_id" ]] || { cat "$log" >&2; return 1; }
  wait_for_sim_marker "$log" 'CI_CLEAN_SIM enabled=true' "$app_pid"
  wait_for_sim_marker "$log" 'CI_MEMORY source=/proc/meminfo ' "$app_pid"
  sim_log="$log"
}
open_clean_confirmation() {
  local log="$1" confirmation_png="$2"
  # Main split-button body; the adjacent Windows-only region drop-down remains disabled.
  xdotool mousemove --sync --window "$window_id" 255 345 click 1
  wait_for_sim_marker "$log" 'CI_CLEAN_CONFIRM open=true' "$app_pid"
  capture "$confirmation_png"
}
activate_clean_and_confirm() {
  local log="$1" confirmation_png="$2"
  open_clean_confirmation "$log" "$confirmation_png"
  xdotool key --clearmodifiers Return
  wait_for_sim_marker "$log" 'CI_CLEAN_SIM stage=confirmed' "$app_pid"
}

# Confirmation cancellation must not spawn the mock authorizer.
start_clean_simulation clean-confirmation-cancel success
open_clean_confirmation "$sim_log" production-clean-simulation-confirm.png
xdotool key --clearmodifiers Tab
wait_for_sim_marker "$sim_log" 'CI_CLEAN_CONFIRM_FOCUS choice=cancel' "$app_pid"
xdotool key --clearmodifiers Return
wait_for_sim_result "$sim_log" confirmation-cancelled "$app_pid"
[[ ! -e "$sim_invocations" ]] || { printf 'Cancelled confirmation spawned mock authorization.\n' >&2; exit 1; }
[[ ! -s "$sim_stage_file" ]] || { printf 'Cancelled confirmation changed the mock stage file.\n' >&2; exit 1; }
capture clean-confirmation-cancelled.png
assert_changed production-clean-simulation-confirm.png clean-confirmation-cancelled.png clean-confirmation-cancel-pixels.txt
close_simulation_window "$sim_log" clean-confirmation-cancel

run_mock_scenario() {
  local scenario="$1" expected="$2" prefix="$3" before_png after_png
  start_clean_simulation "clean-$prefix" "$scenario"
  activate_clean_and_confirm "$sim_log" "clean-$prefix-confirm.png"
  wait_for_sim_marker "$sim_log" 'CI_CLEAN_SIM stage=authorizing backend=mock no_privilege_request=true' "$app_pid"
  capture "clean-$prefix-authorizing.png"
  before_png="clean-$prefix-authorizing.png"
  if [[ "$scenario" == success ]]; then
    # Duplicate activation during an in-flight operation must be rejected.
    xdotool mousemove --sync --window "$window_id" 255 345 click 1
    wait_for_sim_marker "$sim_log" 'CI_CLEAN_DISABLED action=none' "$app_pid"
    [[ "$(wc -l <"$sim_invocations")" == 1 ]] || { printf 'Busy clean activation launched duplicate authorization.\n' >&2; return 1; }
    capture clean-success-busy.png
    assert_changed clean-success-authorizing.png clean-success-busy.png clean-success-busy-pixels.txt
  fi
  if [[ "$scenario" == success || "$scenario" == helper-failed || "$scenario" == helper-unsupported || "$scenario" == uncertain ]]; then
    : >"$sim_start_ack"; chmod 0600 "$sim_start_ack"
    wait_for_sim_marker "$sim_log" 'CI_CLEAN_SIM stage=authorization-accepted backend=mock no_privilege_request=true' "$app_pid"
    capture "clean-$prefix-authorized.png"
    assert_changed "clean-$prefix-authorizing.png" "clean-$prefix-authorized.png" "clean-$prefix-authorized-pixels.txt"
    : >"$sim_authorized_ack"; chmod 0600 "$sim_authorized_ack"
    wait_for_sim_marker "$sim_log" 'CI_CLEAN_SIM stage=helper-running helper=mock no_system_write=true' "$app_pid"
    capture "clean-$prefix-helper-running.png"
    assert_changed "clean-$prefix-authorized.png" "clean-$prefix-helper-running.png" "clean-$prefix-helper-running-pixels.txt"
    : >"$sim_helper_ack"; chmod 0600 "$sim_helper_ack"
    before_png="clean-$prefix-helper-running.png"
  fi
  wait_for_sim_result "$sim_log" "$expected" "$app_pid"
  after_png="clean-$prefix-result.png"
  capture "$after_png"
  assert_changed "$before_png" "$after_png" "clean-$prefix-result-pixels.txt"
  if [[ -e "$sim_invocations" ]]; then
    [[ "$(wc -l <"$sim_invocations")" == 1 ]] || { printf '%s launched duplicate mock authorization.\n' "$prefix" >&2; return 1; }
  fi
  case "$scenario" in
    authorization-cancelled|authorization-failed)
      [[ ! -s "$sim_stage_file" && ! -s "$sim_target_file" ]] || { printf '%s unexpectedly changed mock state.\n' "$prefix" >&2; return 1; }
      ;;
    success)
      [[ "$(<"$sim_stage_file")" == helper-running ]] && printf '1\n' | cmp -s - "$sim_target_file" || { printf 'Success scenario did not reach helper-running and write the test-only value.\n' >&2; return 1; }
      grep -Eq 'observed_memavailable_before_kib=[0-9]+ observed_memavailable_after_kib=[0-9]+ observed_delta_kib=-?[0-9]+ causal_attribution=none no_system_write=true' "$sim_log"
      ;;
    helper-failed|helper-unsupported)
      [[ "$(<"$sim_stage_file")" == helper-running ]] || { printf '%s missed the helper-running stage.\n' "$prefix" >&2; return 1; }
      ;;
    uncertain)
      [[ "$(<"$sim_stage_file")" == helper-running && "$(<"$sim_target_file")" == 1 ]] || { printf 'Uncertain helper test lost partial-write evidence.\n' >&2; return 1; }
      grep -Fq 'automatic_retry=false' "$sim_log"
      ;;
    authorization-signalled)
      [[ ! -s "$sim_stage_file" ]] || { printf 'Signalled authorizer unexpectedly advanced its stage.\n' >&2; return 1; }
      ;;
  esac
  if grep -Eiq 'drop_caches|pkexec|polkit|swapoff|zramctl|mr_start_reclaim|memreduct-zamak-helper' "$sim_log"; then
    printf '%s log indicates a privileged operation was invoked.\n' "$prefix" >&2
    return 1
  fi
  close_simulation_window "$sim_log" "clean-$prefix"
}

run_mock_scenario success success success
run_mock_scenario authorization-cancelled authorization-cancelled authorization-cancelled
run_mock_scenario authorization-failed authorization-failed authorization-failed
run_mock_scenario helper-failed execution-failed helper-failed
run_mock_scenario helper-unsupported execution-failed helper-unsupported
run_mock_scenario uncertain uncertain uncertain
run_mock_scenario authorization-signalled uncertain authorization-signalled

sha256sum "$artifact_dir"/*.png | sed "s#$artifact_dir/##" >"$artifact_dir/screenshots-sha256.txt"
printf 'PASS: production Zamak window, live proc sampling, EN/zh-CN menu switching, compact geometry, disabled production clean actions, confirmation cancellation, fake bridge authorization/helper success and failure mappings, uncertain result/no retry, duplicate prevention, stage/result screenshot changes, XTEST keyboard/mouse and WM close. All helper writes stayed in private temporary files; no privilege request or system write was invoked.\n' \
  | tee "$artifact_dir/result.txt"
