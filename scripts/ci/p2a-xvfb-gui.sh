#!/usr/bin/env bash
set -Eeuo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
artifact_dir="${ARTIFACT_DIR:-$root/artifacts/x11}"
probe="${1:-$root/artifacts/probe/p2a_r4_canvas_probe}"
mkdir -p "$artifact_dir"

for cmd in Xvfb xdotool xwininfo import identify compare stdbuf grep awk xdpyinfo; do
  command -v "$cmd" >/dev/null || {
    printf 'Missing required X11 test command: %s\n' "$cmd" >&2
    exit 2
  }
done
[[ -x "$probe" ]] || { printf 'Probe executable missing: %s\n' "$probe" >&2; exit 2; }

runtime_dir="$(mktemp -d "${RUNNER_TEMP:-/tmp}/p2a-r4-x11.XXXXXX")"
chmod 0700 "$runtime_dir"
xvfb_pid=""
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
  local output="$1"
  local width height canvas_size
  xwininfo -id "$window_id" >"$output"
  width="$(awk '$1 == "Width:" {print $2; exit}' "$output")"
  height="$(awk '$1 == "Height:" {print $2; exit}' "$output")"
  canvas_size="$(grep -m 1 '^CI_DRAW width=' "$artifact_dir/client.log" | sed -E 's/^CI_DRAW width=([^ ]+) height=([^ ]+).*$/\1x\2/')"
  printf 'Observed X11 top-level geometry: %sx%s; GTK Canvas viewport: %s\n' "$width" "$height" "$canvas_size"
  printf 'GTK Canvas viewport: %s\n' "$canvas_size" >>"$output"
  [[ "$width" == "338" && "$height" == "395" ]] || {
    printf 'Expected compact 338x395 top-level window.\n' >&2
    return 1
  }
  [[ "$canvas_size" == "314.0x371.0" ]] || {
    printf 'Expected compact 314x371 GTK Canvas viewport.\n' >&2
    return 1
  }
}
wait_for_marker 'CI_DRAW width='
check_geometry "$artifact_dir/window-initial.txt"
xdotool windowfocus --sync "$window_id"
import -window "$window_id" "$artifact_dir/01-probe-en-initial.png"

xdotool mousemove --sync --window "$window_id" 150 360
wait_for_marker 'CI_MOUSE_HOVER target=3'
xdotool mousedown 1
wait_for_marker 'CI_MOUSE_DOWN target=3'
sleep 0.2
import -window "$window_id" "$artifact_dir/02-probe-en-pressed.png"
set +e
compare -metric AE "$artifact_dir/01-probe-en-initial.png" "$artifact_dir/02-probe-en-pressed.png" null: \
  2>"$artifact_dir/pressed-pixel-diff.txt"
compare_status="$?"
set -e
[[ "$compare_status" == "1" ]] || {
  printf 'Expected a pixel difference for the pressed Canvas state; compare exit=%s\n' "$compare_status" >&2
  exit 1
}
awk 'NF && $1 + 0 > 0 { found=1 } END { exit !found }' "$artifact_dir/pressed-pixel-diff.txt"
xdotool mouseup 1
wait_for_marker 'CI_MOUSE_ACTION action=3'
wait_for_marker 'CI_STATE primary=on'

xdotool mousemove --sync --window "$window_id" 310 360 click 1
wait_for_marker 'CI_MOUSE_ACTION action=4'
wait_for_marker 'CI_MENU open=true'
import -window "$window_id" "$artifact_dir/03-probe-en-split-menu.png"
xdotool mousemove --sync --window "$window_id" 170 310 click 1
wait_for_marker 'CI_MOUSE_ACTION action=6'
wait_for_marker 'CI_STATE option=B'

xdotool mousemove --sync --window "$window_id" 278 22 click 1
wait_for_marker 'CI_STATE locale=zh'
import -window "$window_id" "$artifact_dir/04-probe-zh.png"
xdotool windowfocus --sync "$window_id"
xdotool key --clearmodifiers F2
wait_for_marker 'CI_STATE locale=en'
xdotool key --clearmodifiers Tab
wait_for_marker 'CI_FOCUS value='
xdotool key --clearmodifiers Return
wait_for_marker 'CI_KEY_ACTION action=4'
wait_for_marker 'CI_MENU open=true'
xdotool key --clearmodifiers Escape
wait_for_marker 'CI_MENU closed reason=Escape'

check_geometry "$artifact_dir/window-final.txt"
for image in "$artifact_dir"/0*-probe-*.png; do
  dimensions="$(identify -format '%wx%h' "$image")"
  [[ "$dimensions" == "338x395" ]] || {
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
printf 'X11 GUI input/lifecycle test: XTEST mouse hover/press/release, pixel state change, split menu selection, locale change, keyboard focus, Enter/Escape, F12 clean app quit; exit=0.\n' \
  | tee "$artifact_dir/result.txt"
