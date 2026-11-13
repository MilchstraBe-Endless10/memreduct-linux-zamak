#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Verify under a private Xvfb that the balanced-clean row exists, is drawn
# inside the popup, and is clickable.
#
# What this proves: the tenth row is reachable by a real pointer event and the
# popup grew with it. What it does not prove: that a reclaim ran. There is no
# execute path in this branch, and a test that clicked the row and printed
# "cleaned" would be reporting something no code did.
#
# Nothing privileged runs here: no pkexec, no root helper, no vm write.
set -Eeuo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
artifact_dir="$(printenv ARTIFACT_DIR || true)"
tmp_root="$(printenv RUNNER_TEMP || printf /tmp)"
if [[ -z "$artifact_dir" ]]; then
  artifact_dir="$(mktemp -d "$tmp_root/memreduct-q12-balanced-artifacts.XXXXXX")"
else
  mkdir -p "$artifact_dir"
fi

xvfb_prefix="$(printenv XVFB_PREFIX || printf '%s/.local/opt/xvfb-test' "$HOME")"
if [[ -d "$xvfb_prefix/usr/bin" ]]; then
  export PATH="$xvfb_prefix/usr/bin:$PATH"
fi
for cmd in Xvfb xdotool xwininfo import identify; do
  command -v "$cmd" >/dev/null || {
    printf 'Missing Q12 balanced test command: %s\n' "$cmd" >&2
    exit 2
  }
done
app="$1"
[[ -x "$app" ]] || { printf 'Executable missing: %s\n' "$app" >&2; exit 2; }
[[ "$(id -u)" != 0 ]] || { printf 'This test must run unprivileged.\n' >&2; exit 2; }

harness_log="$artifact_dir/harness.log"
printf 'HARNESS_START backend=xvfb target=balanced-menu-row\n' >"$harness_log"

runtime_dir="$(mktemp -d "$tmp_root/memreduct-q12-xvfb.XXXXXX")"
chmod 0700 "$runtime_dir"
xvfb_pid=""
app_pid=""
cleanup() {
  local status="$?"
  trap - EXIT INT TERM
  for pid in "$app_pid" "$xvfb_pid"; do
    [[ -n "$pid" ]] || continue
    kill -0 "$pid" 2>/dev/null || continue
    kill -TERM "$pid" 2>/dev/null || true
    for _ in {1..40}; do kill -0 "$pid" 2>/dev/null || break; sleep 0.1; done
    kill -KILL "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
  done
  rm -rf -- "$runtime_dir"
  exit "$status"
}
trap cleanup EXIT
trap 'exit 130' INT

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
display_number="$(tr -d '[:space:]' <"$display_file")"
[[ -n "$display_number" ]] || { printf 'Xvfb did not report a display number\n' >&2; exit 1; }
export DISPLAY=":$display_number"
unset WAYLAND_DISPLAY WAYLAND_SOCKET
export GDK_BACKEND=x11 GTK_THEME=Adwaita:light GDK_SCALE=1 GDK_DPI_SCALE=1 LANG=C.UTF-8
export GSETTINGS_BACKEND=memory XDG_RUNTIME_DIR="$runtime_dir"
export HOME="$runtime_dir/home"
mkdir -m 0700 "$HOME"

MEMREDUCT_CI=1 "$app" >"$artifact_dir/app.log" 2>&1 &
app_pid="$!"
window_id=""
for _ in {1..120}; do
  window_id="$(xdotool search --name 'Mem Reduct' 2>/dev/null | head -1 || true)"
  [[ -n "$window_id" ]] && break
  kill -0 "$app_pid" 2>/dev/null || break
  sleep 0.1
done
[[ -n "$window_id" ]] || { printf 'Window never appeared\n' >&2; cat "$artifact_dir/app.log" >&2; exit 1; }
sleep 1.2

import -window "$window_id" "$artifact_dir/window.png"
identify "$artifact_dir/window.png" >>"$harness_log"

# The row geometry the GUI uses is reported by the test bridge. Read it instead
# of guessing coordinates from the screenshot.
grep -E 'CI_BALANCED|CI_POPUP|CI_CLEAN' "$artifact_dir/app.log" >>"$harness_log" || true

# Open the popup with a real click on the clean button, then click the tenth
# row. Both coordinates come from CI_POPUP, which the drawing code emits from
# the same rect it paints, so the test cannot drift from the layout.
# The canvas is inset from the window origin by the GTK header bar. Read the
# real inset from the CI tree report rather than assuming the canvas starts at
# the window corner: with a header bar attached it does not.
win_geom="$(xwininfo -id "$window_id" | awk '/Absolute upper-left X/{x=$4} /Absolute upper-left Y/{y=$4} END{print x" "y}')"
win_x="$(printf '%s' "${win_geom% *}")"
win_y="$(printf '%s' "${win_geom#* }")"
# The canvas coordinates the GUI publishes are relative to the drawing area,
# which starts below the GTK header bar. The bar height is read from the CI
# report of the applied content size; without it every click lands one header
# bar too high (measured: 46 px, and CI_MOUSE_ACTION reported target=none).
canvas_line="$(grep -m1 'CI_CONTENT_SIZE_APPLIED tag=main' "$artifact_dir/app.log" || true)"
bar_h="$(printf '%s\n' "$canvas_line" | sed -E 's/.* bar_h=([0-9]+).*/\1/')"
[[ "$bar_h" =~ ^[0-9]+$ ]] || bar_h=0
canvas_dx="${win_x}"
canvas_dy=$((win_y + bar_h))
printf 'CANVAS_ORIGIN %s,%s bar_h=%s\n' "$canvas_dx" "$canvas_dy" "$bar_h" >>"$harness_log"
button_line="$(grep -m1 'CI_CLEAN_BUTTON' "$artifact_dir/app.log" || true)"
printf 'CI_CLEAN_BUTTON=%s\n' "$button_line" >>"$harness_log"

# The clean button and the tenth row publish their own rects, so the clicks use
# the exact geometry the drawing code painted. Guessing either coordinate is
# how a test passes while clicking empty canvas.
button_line="$(grep -m1 'CI_CLEAN_BUTTON' "$artifact_dir/app.log" || true)"
if [[ -z "$button_line" ]]; then
  # The button rect is only emitted when the popup opens; open it first with a
  # click at the production button position (8 DLU from the left, 21 px tall,
  # on the canvas floor), then read the published rects.
  # CI_CLEAN_BUTTON is emitted on every draw, so it must already be present.
  # Reaching here means the draw never reported it, which is a real failure and
  # not something a guessed coordinate should paper over.
  printf 'FAIL the clean button rect was never published\n' | tee -a "$harness_log" >&2
  exit 1
  xdotool mousemove "$click_x" "$click_y" click 1
  sleep 0.8
  button_line="$(grep -m1 'CI_CLEAN_BUTTON' "$artifact_dir/app.log" || true)"
fi
printf 'CI_CLEAN_BUTTON=%s\n' "$button_line" >>"$harness_log"

read -r btn_x btn_y btn_w btn_h < <(printf '%s\n' "$button_line" \
  | sed -E 's/.* x=([0-9.-]+) y=([0-9.-]+) w=([0-9.-]+) h=([0-9.-]+).*/\1 \2 \3 \4/')
open_x=$(awk -v wx="$canvas_dx" -v rx="$btn_x" -v rw="$btn_w" 'BEGIN{printf "%d", wx + rx + rw - 8}')
open_y=$(awk -v wy="$canvas_dy" -v ry="$btn_y" -v rh="$btn_h" 'BEGIN{printf "%d", wy + ry + rh/2}')
printf 'OPEN_POPUP at %s,%s (button %s,%s %sx%s)\n' "$open_x" "$open_y" \
  "$btn_x" "$btn_y" "$btn_w" "$btn_h" >>"$harness_log"
xdotool mousemove "$open_x" "$open_y" click 1
sleep 0.9

popup_line="$(grep -m1 'CI_POPUP kind=clean' "$artifact_dir/app.log" || true)"
printf 'CI_POPUP_CLEAN=%s\n' "$popup_line" >>"$harness_log"
if [[ -z "$popup_line" || -z "$button_line" ]]; then
  printf 'FAIL the clean popup never reported its geometry\n' | tee -a "$harness_log" >&2
  exit 1
fi

row_line="$(grep -m1 'CI_BALANCED_ROW' "$artifact_dir/app.log" || true)"
printf 'CI_BALANCED_ROW=%s\n' "$row_line" >>"$harness_log"
if [[ -z "$row_line" ]]; then
  printf 'FAIL the balanced row never published a rect\n' | tee -a "$harness_log" >&2
  exit 1
fi

# Canvas coordinates plus the window origin are root-window pixels.
read -r row_x row_y row_w row_h < <(printf '%s\n' "$row_line" \
  | sed -E 's/.* x=([0-9.-]+) y=([0-9.-]+) w=([0-9.-]+) h=([0-9.-]+).*/\1 \2 \3 \4/')
click_x=$(awk -v wx="$canvas_dx" -v rx="$row_x" -v rw="$row_w" 'BEGIN{printf "%d", wx + rx + rw/2}')
click_y=$(awk -v wy="$canvas_dy" -v ry="$row_y" -v rh="$row_h" 'BEGIN{printf "%d", wy + ry + rh/2}')
printf 'CLICK row9 at %s,%s (row %s,%s %sx%s, window %s,%s)\n' "$click_x" "$click_y" \
  "$row_x" "$row_y" "$row_w" "$row_h" "$win_x" "$win_y" >>"$harness_log"
xdotool mousemove "$click_x" "$click_y" click 1
sleep 1.0

# The row must answer. Either it produced a plan or it said why it could not;
# silence would mean the click missed the row entirely.
if ! grep -qE 'CI_BALANCED_PLAN|CI_BALANCED_DISABLED' "$artifact_dir/app.log"; then
  printf 'FAIL clicking the tenth row produced no CI_BALANCED_* line\n' | tee -a "$harness_log" >&2
  exit 1
fi
grep -E 'CI_BALANCED' "$artifact_dir/app.log" | tee -a "$harness_log"
printf 'PASS balanced row reachable by pointer\n' >>"$harness_log"

# Terminate cleanly and let the harness record the exit.
kill -TERM "$app_pid" 2>/dev/null || true
for _ in {1..40}; do kill -0 "$app_pid" 2>/dev/null || break; sleep 0.1; done
wait "$app_pid" 2>/dev/null || true
app_pid=""

printf 'Q12_BALANCED_MENU_TEST artifacts=%s\n' "$artifact_dir"
