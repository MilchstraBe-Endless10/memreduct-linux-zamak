#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Verify the balanced-clean scope picker renders and responds to a real click.
#
# The picker normally opens only when the planner proposes an action, and on an
# anonymous-heavy host it correctly proposes none. MEMREDUCT_CI_START_BALANCED_PICKER
# opens the panel directly so its layout and click handling can be tested
# regardless of machine state. The hook selects nothing and reclaims nothing.
set -Eeuo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
artifact_dir="$(printenv ARTIFACT_DIR || true)"
tmp_root="$(printenv RUNNER_TEMP || printf /tmp)"
if [[ -z "$artifact_dir" ]]; then
  artifact_dir="$(mktemp -d "$tmp_root/memreduct-q12-picker-artifacts.XXXXXX")"
else
  mkdir -p "$artifact_dir"
fi

xvfb_prefix="$(printenv XVFB_PREFIX || printf '%s/.local/opt/xvfb-test' "$HOME")"
if [[ -d "$xvfb_prefix/usr/bin" ]]; then export PATH="$xvfb_prefix/usr/bin:$PATH"; fi
for cmd in Xvfb xdotool xwininfo import identify magick; do
  command -v "$cmd" >/dev/null || { printf 'Missing: %s\n' "$cmd" >&2; exit 2; }
done
app="$1"
[[ -x "$app" ]] || { printf 'Executable missing: %s\n' "$app" >&2; exit 2; }
[[ "$(id -u)" != 0 ]] || { printf 'must run unprivileged\n' >&2; exit 2; }

harness_log="$artifact_dir/harness.log"
printf 'HARNESS_START backend=xvfb target=balanced-picker\n' >"$harness_log"
runtime_dir="$(mktemp -d "$tmp_root/memreduct-q12-picker.XXXXXX")"; chmod 0700 "$runtime_dir"
xvfb_pid=""; app_pid=""
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
for _ in {1..100}; do [[ -s "$display_file" ]] && break; sleep 0.1; done
display_number="$(tr -d '[:space:]' <"$display_file")"
[[ -n "$display_number" ]] || { printf 'no display\n' >&2; exit 1; }
export DISPLAY=":$display_number"
unset WAYLAND_DISPLAY WAYLAND_SOCKET
export GDK_BACKEND=x11 GTK_THEME=Adwaita:light GDK_SCALE=1 GDK_DPI_SCALE=1 LANG=C.UTF-8
export GSETTINGS_BACKEND=memory XDG_RUNTIME_DIR="$runtime_dir"
export HOME="$runtime_dir/home"; mkdir -m 0700 "$HOME"

MEMREDUCT_CI=1 MEMREDUCT_CI_START_BALANCED_PICKER=1 "$app" >"$artifact_dir/app.log" 2>&1 &
app_pid="$!"
window_id=""
for _ in {1..120}; do
  window_id="$(xdotool search --name 'Mem Reduct' 2>/dev/null | head -1 || true)"
  [[ -n "$window_id" ]] && break
  kill -0 "$app_pid" 2>/dev/null || break
  sleep 0.1
done
[[ -n "$window_id" ]] || { printf 'window never appeared\n' >&2; cat "$artifact_dir/app.log" >&2; exit 1; }
sleep 1.5
import -window "$window_id" "$artifact_dir/picker.png"
identify "$artifact_dir/picker.png" >>"$harness_log"

picker_line="$(grep -m1 'CI_BALANCED_PICKER' "$artifact_dir/app.log" || true)"
printf 'CI_BALANCED_PICKER=%s\n' "$picker_line" >>"$harness_log"
if [[ -z "$picker_line" ]]; then
  printf 'FAIL the picker never reported itself\n' | tee -a "$harness_log" >&2
  exit 1
fi
scopes="$(printf '%s\n' "$picker_line" | sed -E 's/.*scopes=([0-9]+).*/\1/')"
printf 'SCOPES=%s\n' "$scopes" >>"$harness_log"
if [[ "$scopes" == "0" ]]; then
  printf 'FAIL picker opened with no scope to offer\n' | tee -a "$harness_log" >&2
  exit 1
fi

# Click the first row: the panel is centred, rows start 58 px below its top.
win_geom="$(xwininfo -id "$window_id" | awk '/Absolute upper-left X/{x=$4} /Absolute upper-left Y/{y=$4} END{print x" "y}')"
win_x="${win_geom% *}"; win_y="${win_geom#* }"
size="$(xwininfo -id "$window_id" | awk '/Width:/{w=$2} /Height:/{h=$2} END{print w" "h}')"
win_w="${size% *}"; win_h="${size#* }"
# Coordinates come from what the GUI published, not from re-deriving the
# layout here: the first version recomputed it and missed the row by enough
# that the selection never moved (measured: two identical screenshots).
row_line="$(grep -m1 'CI_BALANCED_ROW0' "$artifact_dir/app.log" || true)"
printf 'CI_BALANCED_ROW0=%s\n' "$row_line" >>"$harness_log"
if [[ -z "$row_line" ]]; then
  printf 'FAIL the picker never published a row rect\n' | tee -a "$harness_log" >&2
  exit 1
fi
read -r rx ry rw rh < <(printf '%s\n' "$row_line" | sed -E 's/.* x=([0-9.-]+) y=([0-9.-]+) w=([0-9.-]+) h=([0-9.-]+).*/\1 \2 \3 \4/')
click_x=$(awk -v wx="$win_x" -v a="$rx" -v b="$rw" 'BEGIN{printf "%d", wx + a + b/2}')
click_y=$(awk -v wy="$win_y" -v a="$ry" -v b="$rh" 'BEGIN{printf "%d", wy + a + b/2}')
printf 'CLICK picker row at %s,%s (row %s,%s %sx%s, window %sx%s at %s,%s)\n' \
  "$click_x" "$click_y" "$rx" "$ry" "$rw" "$rh" "$win_w" "$win_h" "$win_x" "$win_y" >>"$harness_log"
xdotool mousemove "$click_x" "$click_y" click 1
sleep 0.8
import -window "$window_id" "$artifact_dir/picker_after_click.png"

# The click must visibly change the panel. Comparing the screenshots is the
# check that caught the first version silently missing every row.
diff_pixels="$(magick compare -metric AE "$artifact_dir/picker.png" \
  "$artifact_dir/picker_after_click.png" null: 2>&1 || true)"
printf 'CLICK_DIFF_PIXELS=%s\n' "$diff_pixels" >>"$harness_log"
if [[ "$diff_pixels" == "0" ]]; then
  printf 'FAIL the click changed nothing on screen\n' | tee -a "$harness_log" >&2
  exit 1
fi
printf 'PASS picker rendered with %s scope(s) and the click changed the panel\n' "$scopes" >>"$harness_log"
printf 'artifacts=%s\n' "$artifact_dir"
