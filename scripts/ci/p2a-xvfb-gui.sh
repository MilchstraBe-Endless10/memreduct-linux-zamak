#!/usr/bin/env bash
set -Eeuo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
artifact_dir="${ARTIFACT_DIR:-$root/artifacts/x11}"
probe="${1:-$root/artifacts/probe/p2a_r4_canvas_probe}"
mkdir -p "$artifact_dir"

for cmd in Xvfb openbox xdotool xwininfo xprop import identify compare convert stdbuf grep awk xdpyinfo sed; do
  command -v "$cmd" >/dev/null || {
    printf 'Missing required X11 test command: %s\n' "$cmd" >&2
    exit 2
  }
done
[[ -x "$probe" ]] || { printf 'Probe executable missing: %s\n' "$probe" >&2; exit 2; }

runtime_dir="$(mktemp -d "${RUNNER_TEMP:-/tmp}/p2a-r4-x11.XXXXXX")"
chmod 0700 "$runtime_dir"
xvfb_pid=""
wm_pid=""
app_pid=""
stop_pid() {
  local pid="$1"
  local i
  if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
    kill -TERM "$pid" 2>/dev/null || true
    for i in {1..50}; do
      kill -0 "$pid" 2>/dev/null || break
      sleep 0.1
    done
    if kill -0 "$pid" 2>/dev/null; then
      kill -KILL "$pid" 2>/dev/null || true
    fi
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
Xvfb -displayfd 3 -screen 0 800x600x24 -nolisten tcp -noreset \
  3>"$display_file" >"$artifact_dir/xvfb.log" 2>&1 &
xvfb_pid="$!"

display_ready=false
for _ in {1..100}; do
  if [[ -s "$display_file" ]]; then
    display_ready=true
    break
  fi
  if ! kill -0 "$xvfb_pid" 2>/dev/null; then
    break
  fi
  sleep 0.1
done
[[ "$display_ready" == true ]] || {
  printf 'Xvfb did not allocate an isolated display.\n' >&2
  cat "$artifact_dir/xvfb.log" >&2 || true
  exit 1
}
display_number="$(<"$display_file")"
export DISPLAY=":$display_number"
unset WAYLAND_DISPLAY WAYLAND_SOCKET
export GDK_BACKEND=x11
export XDG_RUNTIME_DIR="$runtime_dir"
export GSETTINGS_BACKEND=memory
printf 'Xvfb display=%s screen=800x600x24; GDK_BACKEND=x11; XDG_RUNTIME_DIR mode=%s\n' \
  "$DISPLAY" "$(stat -c '%a' "$runtime_dir")" | tee "$artifact_dir/environment.txt"
xdpyinfo -queryExtensions | grep -E '(^|[[:space:]])XTEST([[:space:]]|$)' \
  | tee "$artifact_dir/xtest-extension.txt"

mkdir -m 0700 "$runtime_dir/home" "$runtime_dir/config"
HOME="$runtime_dir/home" XDG_CONFIG_HOME="$runtime_dir/config" \
  openbox --sm-disable >"$artifact_dir/openbox.log" 2>&1 &
wm_pid="$!"
wm_ready=false
for _ in {1..50}; do
  if xprop -root _NET_SUPPORTING_WM_CHECK 2>/dev/null | grep -q 'window id'; then
    wm_ready=true
    break
  fi
  if ! kill -0 "$wm_pid" 2>/dev/null; then
    break
  fi
  sleep 0.1
done
[[ "$wm_ready" == true ]] || {
  printf 'Openbox did not register as the isolated X11 window manager.\n' >&2
  cat "$artifact_dir/openbox.log" >&2 || true
  exit 1
}
printf 'Window manager: Openbox (test child pid=%s)\n' "$wm_pid" | tee -a "$artifact_dir/environment.txt"

stdbuf -oL "$probe" >"$artifact_dir/client.log" 2>&1 &
app_pid="$!"

window_id=""
for _ in {1..50}; do
  window_id="$(xdotool search --onlyvisible --name 'P2A-R4 CI Canvas Probe' 2>/dev/null | head -n 1 || true)"
  if [[ -n "$window_id" ]]; then
    break
  fi
  if ! kill -0 "$app_pid" 2>/dev/null; then
    break
  fi
  sleep 0.1
done
[[ -n "$window_id" ]] || {
  printf 'Canvas top-level X11 window did not appear.\n' >&2
  cat "$artifact_dir/client.log" >&2 || true
  exit 1
}

wait_for_marker() {
  local marker="$1"
  for _ in {1..50}; do
    grep -Fq -- "$marker" "$artifact_dir/client.log" && return 0
    kill -0 "$app_pid" 2>/dev/null || break
    sleep 0.1
  done
  printf 'Missing client event marker: %s\n' "$marker" >&2
  return 1
}
check_geometry() {
  local expected_width="$1" expected_height="$2" expected_canvas="$3" output="$4"
  local width height canvas_size
  xwininfo -id "$window_id" >"$output"
  width="$(awk '$1 == "Width:" {print $2; exit}' "$output")"
  height="$(awk '$1 == "Height:" {print $2; exit}' "$output")"
  canvas_size="$(grep '^CI_DRAW width=' "$artifact_dir/client.log" | tail -n 1 | sed -E 's/^CI_DRAW width=([^ ]+) height=([^ ]+).*$/\1x\2/')"
  printf 'Observed X11 top-level geometry: %sx%s; GTK Canvas viewport: %s\n' "$width" "$height" "$canvas_size"
  printf 'GTK Canvas viewport: %s\n' "$canvas_size" >>"$output"
  [[ "$width" == "$expected_width" && "$height" == "$expected_height" ]] || {
    printf 'Expected top-level window %sx%s.\n' "$expected_width" "$expected_height" >&2
    return 1
  }
  [[ "$canvas_size" == "$expected_canvas" ]] || {
    printf 'Expected GTK Canvas viewport %s.\n' "$expected_canvas" >&2
    return 1
  }
}

wait_for_draw_count() {
  local marker="$1" expected_count="$2" count
  for _ in {1..50}; do
    count="$(grep -Fc -- "$marker" "$artifact_dir/client.log" || true)"
    if (( count >= expected_count )); then
      return 0
    fi
    kill -0 "$app_pid" 2>/dev/null || break
    sleep 0.1
  done
  printf 'Missing resize draw marker %s (%s occurrences expected).\n' "$marker" "$expected_count" >&2
  return 1
}

capture_button_roi() {
  convert "$1" -crop '288x28+24+293' +repage "$2"
}

assert_pixel_delta() {
  local before="$1" after="$2" report="$3" compare_status changed_pixels
  set +e
  compare -metric AE "$before" "$after" null: 2>"$report"
  compare_status="$?"
  set -e
  changed_pixels="$(sed -E 's/^[[:space:]]*([0-9]+).*/\1/' "$report")"
  [[ "$compare_status" == "1" && "$changed_pixels" =~ ^[0-9]+$ ]] && (( changed_pixels > 0 )) || {
    printf 'Expected a non-zero pixel delta for %s vs %s; status=%s metric=%s\n' \
      "$(basename "$before")" "$(basename "$after")" "$compare_status" "$changed_pixels" >&2
    return 1
  }
  printf '\nchanged_pixels=%s\n' "$changed_pixels" | tee -a "$report"
}

assert_pixel_identity() {
  local first="$1" second="$2" report="$3" compare_status changed_pixels
  set +e
  compare -metric AE "$first" "$second" null: 2>"$report"
  compare_status="$?"
  set -e
  changed_pixels="$(sed -E 's/^[[:space:]]*([0-9]+).*/\1/' "$report")"
  [[ "$compare_status" == "0" && "$changed_pixels" == "0" ]] || {
    printf 'Expected identical pixels for %s vs %s; status=%s metric=%s\n' \
      "$(basename "$first")" "$(basename "$second")" "$compare_status" "$changed_pixels" >&2
    return 1
  }
  printf '\nchanged_pixels=0\n' | tee -a "$report"
}
wait_for_marker 'CI_DRAW width='
check_geometry 336 348 312.0x324.0 "$artifact_dir/window-initial.txt"
xdotool windowsize --sync "$window_id" 420 380
wait_for_draw_count 'CI_DRAW width=396.0 height=356.0' 1
check_geometry 420 380 396.0x356.0 "$artifact_dir/window-resized.txt"
xdotool windowsize --sync "$window_id" 336 348
wait_for_draw_count 'CI_DRAW width=312.0 height=324.0' 2
check_geometry 336 348 312.0x324.0 "$artifact_dir/window-restored.txt"

xdotool windowfocus --sync "$window_id"
import -window "$window_id" "$artifact_dir/00-probe-en-initial.png"

xdotool mousemove --sync --window "$window_id" 150 306
wait_for_marker 'CI_MOUSE_HOVER target=3'
sleep 0.2
import -window "$window_id" "$artifact_dir/01-probe-en-hover.png"
capture_button_roi "$artifact_dir/01-probe-en-hover.png" "$artifact_dir/hover-button-roi.png"
xdotool mousedown 1
wait_for_marker 'CI_MOUSE_DOWN target=3'
sleep 0.2
import -window "$window_id" "$artifact_dir/02-probe-en-pressed.png"
capture_button_roi "$artifact_dir/02-probe-en-pressed.png" "$artifact_dir/pressed-button-roi.png"
assert_pixel_delta "$artifact_dir/hover-button-roi.png" "$artifact_dir/pressed-button-roi.png" \
  "$artifact_dir/hover-pressed-pixel-diff.txt"
xdotool mouseup 1
wait_for_marker 'CI_MOUSE_UP target=3'
wait_for_marker 'CI_MOUSE_ACTION action=3'
wait_for_marker 'CI_STATE primary=on'
sleep 0.2
import -window "$window_id" "$artifact_dir/03-probe-en-released.png"
capture_button_roi "$artifact_dir/03-probe-en-released.png" "$artifact_dir/released-button-roi.png"
assert_pixel_delta "$artifact_dir/pressed-button-roi.png" "$artifact_dir/released-button-roi.png" \
  "$artifact_dir/pressed-release-pixel-diff.txt"

xdotool mousemove --sync --window "$window_id" 310 306 click 1
wait_for_marker 'CI_MOUSE_ACTION action=4'
wait_for_marker 'CI_MENU open=true'
import -window "$window_id" "$artifact_dir/04-probe-en-split-menu.png"
xdotool mousemove --sync --window "$window_id" 170 252 click 1
wait_for_marker 'CI_MOUSE_ACTION action=6'
wait_for_marker 'CI_STATE option=B'

sleep 0.2
import -window "$window_id" "$artifact_dir/05-probe-en-locale.png"
convert "$artifact_dir/05-probe-en-locale.png" -crop '288x42+24+50' +repage \
  "$artifact_dir/locale-en-layout-roi.png"
xdotool mousemove --sync --window "$window_id" 278 22 click 1
wait_for_marker 'CI_STATE locale=zh'
sleep 0.2
check_geometry 336 348 312.0x324.0 "$artifact_dir/window-zh.txt"
import -window "$window_id" "$artifact_dir/06-probe-zh-locale.png"
convert "$artifact_dir/06-probe-zh-locale.png" -crop '288x42+24+50' +repage \
  "$artifact_dir/locale-zh-layout-roi.png"
assert_pixel_delta "$artifact_dir/locale-en-layout-roi.png" "$artifact_dir/locale-zh-layout-roi.png" \
  "$artifact_dir/locale-layout-pixel-diff.txt"
xdotool key --clearmodifiers F2
wait_for_marker 'CI_STATE locale=en'
sleep 0.2
check_geometry 336 348 312.0x324.0 "$artifact_dir/window-en-restored.txt"
import -window "$window_id" "$artifact_dir/07-probe-en-restored.png"
convert "$artifact_dir/07-probe-en-restored.png" -crop '288x42+24+50' +repage \
  "$artifact_dir/locale-en-restored-layout-roi.png"
assert_pixel_identity "$artifact_dir/locale-en-layout-roi.png" \
  "$artifact_dir/locale-en-restored-layout-roi.png" "$artifact_dir/locale-layout-return-diff.txt"

xdotool windowfocus --sync "$window_id"
xdotool key --clearmodifiers Tab
wait_for_marker 'CI_FOCUS value='
xdotool key --clearmodifiers Return
wait_for_marker 'CI_KEY_ACTION action=4'
wait_for_marker 'CI_MENU open=true'
xdotool key --clearmodifiers Escape
wait_for_marker 'CI_MENU closed reason=Escape'

check_geometry 336 348 312.0x324.0 "$artifact_dir/window-final.txt"
for image in "$artifact_dir"/0*-probe-*.png; do
  dimensions="$(identify -format '%wx%h' "$image")"
  [[ "$dimensions" == "336x348" ]] || {
    printf 'Unexpected screenshot dimensions for %s: %s\n' "$(basename "$image")" "$dimensions" >&2
    exit 1
  }
done

xdotool key --clearmodifiers F12
wait_for_marker 'CI_QUIT source=f12'
wait_for_marker 'CI_APP_RETURNED'
app_exited=false
for _ in {1..100}; do
  if ! kill -0 "$app_pid" 2>/dev/null; then
    app_exited=true
    break
  fi
  sleep 0.1
done
[[ "$app_exited" == true ]] || {
  printf 'Canvas process did not exit after its explicit test-only quit event.\n' >&2
  exit 1
}
set +e
wait "$app_pid"
app_status="$?"
set -e
app_pid=""
printf 'Canvas probe exit status: %s\n' "$app_status" | tee "$artifact_dir/client-exit.txt"
[[ "$app_status" == "0" ]] || { cat "$artifact_dir/client.log" >&2; exit "$app_status"; }

close_log="$artifact_dir/client-close.log"
stdbuf -oL "$probe" >"$close_log" 2>&1 &
app_pid="$!"
close_window_id=""
for _ in {1..50}; do
  close_window_id="$(xdotool search --onlyvisible --name 'P2A-R4 CI Canvas Probe' 2>/dev/null | head -n 1 || true)"
  [[ -n "$close_window_id" ]] && break
  kill -0 "$app_pid" 2>/dev/null || break
  sleep 0.1
done
[[ -n "$close_window_id" ]] || {
  printf 'Independent close-test window did not appear.\n' >&2
  cat "$close_log" >&2 || true
  exit 1
}
close_drawn=false
for _ in {1..50}; do
  if grep -Fq 'CI_DRAW width=312.0 height=324.0' "$close_log"; then
    close_drawn=true
    break
  fi
  kill -0 "$app_pid" 2>/dev/null || break
  sleep 0.1
done
[[ "$close_drawn" == true ]] || {
  printf 'Close-test window did not draw the compact Canvas.\n' >&2
  cat "$close_log" >&2 || true
  exit 1
}
xprop -id "$close_window_id" WM_PROTOCOLS WM_STATE \
  | tee "$artifact_dir/window-close-protocols.txt"
xprop -root _NET_SUPPORTING_WM_CHECK _NET_SUPPORTED \
  | tee "$artifact_dir/window-manager-protocols.txt"
grep -q 'WM_DELETE_WINDOW' "$artifact_dir/window-close-protocols.txt" || {
  printf 'The GTK window does not advertise WM_DELETE_WINDOW; a graceful close cannot be tested.\n' >&2
  exit 1
}
# xdotool windowclose uses XDestroyWindow; Alt+F4 exercises the actual WM close binding.
printf 'XTEST Alt+F4 to focused GTK window; WM should send WM_DELETE_WINDOW. xdotool windowclose is destructive.\n' \
  | tee "$artifact_dir/window-close-request.txt"
xdotool windowfocus --sync "$close_window_id"
xdotool key --clearmodifiers Alt+F4
close_seen=false
for _ in {1..50}; do
  if grep -Fq 'CI_WINDOW_CLOSE' "$close_log"; then
    close_seen=true
    break
  fi
  kill -0 "$app_pid" 2>/dev/null || break
  sleep 0.1
done
[[ "$close_seen" == true ]] || {
  printf 'The WM close request did not invoke CI_WINDOW_CLOSE; inspect window-close-protocols.txt and the Openbox/client logs.\n' >&2
  cat "$close_log" >&2 || true
  exit 1
}
if grep -Eiq 'unexpectedly destroyed|invalid \(NULL\) pointer|g_signal_handler_disconnect: assertion|gdk_frame_clock_end_updating: assertion' "$close_log"; then
  printf 'GTK reported an unexpected window/resource teardown during WM close.\n' >&2
  cat "$close_log" >&2
  exit 1
fi
returned_seen=false
for _ in {1..50}; do
  if grep -Fq 'CI_APP_RETURNED' "$close_log"; then
    returned_seen=true
    break
  fi
  kill -0 "$app_pid" 2>/dev/null || break
  sleep 0.1
done
[[ "$returned_seen" == true ]] || {
  printf 'The app event loop did not return after the WM close request.\n' >&2
  cat "$close_log" >&2 || true
  exit 1
}
if grep -Fq 'CI_QUIT source=timer' "$close_log"; then
  printf 'The close test used the timer fallback instead of the WM close request.\n' >&2
  cat "$close_log" >&2 || true
  exit 1
fi
app_exited=false
for _ in {1..50}; do
  if ! kill -0 "$app_pid" 2>/dev/null; then
    app_exited=true
    break
  fi
  sleep 0.1
done
[[ "$app_exited" == true ]] || {
  printf 'The app process did not exit within 5 seconds of the WM close request.\n' >&2
  exit 1
}
set +e
wait "$app_pid"
close_status="$?"
set -e
app_pid=""
printf 'Window-close probe exit status: %s\n' "$close_status" | tee "$artifact_dir/window-close-exit.txt"
[[ "$close_status" == "0" ]] || { cat "$close_log" >&2; exit "$close_status"; }
if xdotool getwindowname "$close_window_id" >"$artifact_dir/window-after-close.txt" 2>&1; then
  printf 'The X11 window still exists after the app exited.\n' >&2
  exit 1
fi
grep -Fq 'CI_WINDOW_CLOSE' "$close_log"
grep -Fq 'CI_APP_RETURNED' "$close_log"
printf 'X11 GUI input/lifecycle test: actual XTEST hover/pressed/release ROI diffs, split menu, bilingual layout, resize and geometry, focus/Enter/Escape/F12, plus Alt+F4 WM close; both app exits=0.\n' \
  | tee "$artifact_dir/result.txt"
