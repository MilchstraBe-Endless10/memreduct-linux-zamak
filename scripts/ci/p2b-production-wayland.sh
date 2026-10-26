#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -Eeuo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
artifact_dir="${ARTIFACT_DIR:-$root/artifacts/p2b-wayland}"
app="${1:-$artifact_dir/memreduct-zamak-production}"
mkdir -p "$artifact_dir"
for cmd in weston wayland-info stat timeout grep; do
  command -v "$cmd" >/dev/null || { printf 'Missing Wayland test command: %s\n' "$cmd" >&2; exit 2; }
done
[[ -x "$app" ]] || { printf 'Production executable missing.\n' >&2; exit 2; }

runtime_dir="$(mktemp -d "${RUNNER_TEMP:-/tmp}/memreduct-p2b-wayland.XXXXXX")"
chmod 0700 "$runtime_dir"
weston_pid=""
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
  stop_pid "$weston_pid"
  rm -rf -- "$runtime_dir"
  exit "$status"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

socket="wayland-memreduct-p2b-${GITHUB_RUN_ID:-local}-${GITHUB_RUN_ATTEMPT:-0}-$$"
[[ "$(stat -c '%a' "$runtime_dir")" == 700 ]] || { printf 'XDG_RUNTIME_DIR mode is not 0700.\n' >&2; exit 1; }
{
  printf 'XDG_RUNTIME_DIR mode=700\nSocket=%s\nBackend=headless\nRenderer=pixman\nDISPLAY=unset\n' "$socket"
  printf 'Weston='; weston --version
} | tee "$artifact_dir/environment.txt"
if ! env -u DISPLAY -u WAYLAND_DISPLAY -u WAYLAND_SOCKET XDG_RUNTIME_DIR="$runtime_dir" \
    weston --backend=headless --help >"$artifact_dir/weston-headless-help.log" 2>&1; then
  cat "$artifact_dir/weston-headless-help.log" >&2
  printf 'Installed Weston does not accept the headless backend.\n' >&2
  exit 1
fi
for option in --backend --renderer --no-config --socket --log --width --height --scale; do
  grep -Fq -- "$option" "$artifact_dir/weston-headless-help.log" || {
    printf 'Installed Weston help is missing required option %s.\n' "$option" >&2; exit 1;
  }
done

env -u DISPLAY -u WAYLAND_DISPLAY -u WAYLAND_SOCKET XDG_RUNTIME_DIR="$runtime_dir" \
  weston --backend=headless --renderer=pixman --no-config --socket="$socket" --idle-time=0 \
    --width=800 --height=600 --scale=1 --log="$artifact_dir/weston.log" \
    >"$artifact_dir/weston-bootstrap.log" 2>&1 &
weston_pid="$!"
for _ in {1..100}; do
  [[ -S "$runtime_dir/$socket" ]] && break
  kill -0 "$weston_pid" 2>/dev/null || break
  sleep 0.1
done
[[ -S "$runtime_dir/$socket" ]] || {
  cat "$artifact_dir/weston-bootstrap.log" "$artifact_dir/weston.log" >&2 2>/dev/null || true
  printf 'Weston did not create its private Wayland socket.\n' >&2; exit 1;
}
stat -c 'Socket type=%F mode=%a owner=%U' "$runtime_dir/$socket" | tee "$artifact_dir/socket.txt"
env -u DISPLAY -u WAYLAND_SOCKET XDG_RUNTIME_DIR="$runtime_dir" WAYLAND_DISPLAY="$socket" \
  GDK_BACKEND=wayland GTK_THEME=Adwaita:light GDK_SCALE=1 GDK_DPI_SCALE=1 \
  timeout --signal=TERM --kill-after=2s 10s wayland-info >"$artifact_dir/wayland-info.log" 2>&1
grep -q 'wl_compositor' "$artifact_dir/wayland-info.log"

env -u DISPLAY -u WAYLAND_SOCKET XDG_RUNTIME_DIR="$runtime_dir" WAYLAND_DISPLAY="$socket" \
  GDK_BACKEND=wayland GTK_THEME=Adwaita:light GDK_SCALE=1 GDK_DPI_SCALE=1 \
  MEMREDUCT_CI=1 MEMREDUCT_CI_EXIT_MS=6000 WAYLAND_DEBUG=1 stdbuf -oL "$app" \
  >"$artifact_dir/client.log" 2>&1 &
app_pid="$!"
app_finished=false
for _ in {1..150}; do
  if ! kill -0 "$app_pid" 2>/dev/null; then app_finished=true; break; fi
  sleep 0.1
done
[[ "$app_finished" == true ]] || { printf 'Production Wayland client exceeded its 15-second lifecycle bound.\n' >&2; exit 124; }
set +e
wait "$app_pid"
app_status="$?"
set -e
app_pid=""
printf 'Production Wayland client exit status=%s\n' "$app_status" | tee "$artifact_dir/client-exit.txt"
[[ "$app_status" == 0 ]] || { cat "$artifact_dir/client.log" >&2; exit "$app_status"; }

grep -Fq 'CI_PRODUCTION_START' "$artifact_dir/client.log"
grep -Fq 'CI_DRAW width=' "$artifact_dir/client.log"
grep -Fq 'CI_MEMORY source=/proc/meminfo' "$artifact_dir/client.log"
grep -Fq 'CI_QUIT source=bounded_test_timer' "$artifact_dir/client.log"
grep -Fq 'CI_APP_RETURNED' "$artifact_dir/client.log"
grep -Fq 'xdg_toplevel' "$artifact_dir/client.log"
grep -Fq 'wl_surface' "$artifact_dir/client.log"
if grep -Eiq 'protocol error|wl_display@[^ ]+\.error|drop_caches|pkexec|swapoff' "$artifact_dir/client.log"; then
  printf 'Wayland protocol error or forbidden privileged-operation marker detected.\n' >&2; exit 1;
fi
warning_count="$(grep -Fc "gdk_seat_get_keyboard: assertion 'GDK_IS_SEAT (seat)' failed" "$artifact_dir/client.log" || true)"
if grep -Fq "interface: 'wl_seat'" "$artifact_dir/wayland-info.log"; then seat="advertised"; else seat="not-advertised"; fi
{
  printf 'gdk_seat_get_keyboard critical count=%s\nwl_seat global=%s\n' "$warning_count" "$seat"
  printf 'The diagnostic is retained without claiming the absent input seat caused it. The production GUI drew, read meminfo, returned from app.run and exited 0.\n'
  printf 'Pointer/keyboard input is NOT RUN in this headless lane; Xvfb/XTEST is reported separately.\n'
} | tee "$artifact_dir/gtk-seat-diagnostics.txt"

# Start the same production binary directly in the settings shell. Headless Weston
# has no input seat, so this verifies surface creation and lifecycle only.
env -u DISPLAY -u WAYLAND_SOCKET XDG_RUNTIME_DIR="$runtime_dir" WAYLAND_DISPLAY="$socket" \
  GDK_BACKEND=wayland GTK_THEME=Adwaita:light GDK_SCALE=1 GDK_DPI_SCALE=1 \
  MEMREDUCT_CI=1 MEMREDUCT_CI_START_SETTINGS=1 MEMREDUCT_CI_EXIT_MS=6000 WAYLAND_DEBUG=1 stdbuf -oL "$app" \
  >"$artifact_dir/settings-client.log" 2>&1 &
app_pid="$!"
settings_finished=false
for _ in {1..150}; do
  if ! kill -0 "$app_pid" 2>/dev/null; then settings_finished=true; break; fi
  sleep 0.1
done
[[ "$settings_finished" == true ]] || { printf 'Production Wayland settings surface exceeded its 15-second lifecycle bound.\n' >&2; exit 124; }
set +e
wait "$app_pid"
settings_status="$?"
set -e
app_pid=""
printf 'Production Wayland settings client exit status=%s\n' "$settings_status" | tee "$artifact_dir/settings-client-exit.txt"
[[ "$settings_status" == 0 ]] || { cat "$artifact_dir/settings-client.log" >&2; exit "$settings_status"; }
grep -Fq 'CI_SETTINGS_WINDOW mode=settings' "$artifact_dir/settings-client.log"
grep -Fq 'CI_SETTINGS_PAGE idd=101 page=General locale=en controls=116,117,118,120,119,121,122,123 child_dlu=260x168 provisional_canvas=523.0x321.0' "$artifact_dir/settings-client.log"
grep -Fq 'CI_APP_RETURNED' "$artifact_dir/settings-client.log"
grep -Fq 'xdg_toplevel' "$artifact_dir/settings-client.log"
grep -Fq 'wl_surface' "$artifact_dir/settings-client.log"
if grep -Eiq 'protocol error|wl_display@[^ ]+\.error|drop_caches|pkexec|swapoff|zramctl' "$artifact_dir/settings-client.log"; then
  printf 'Wayland settings lifecycle encountered a protocol or forbidden-operation marker.\n' >&2; exit 1;
fi

printf 'PASS: production Wayland main and settings surfaces connected to private Weston, drew and exited 0. No pointer/keyboard input is claimed.\n' \
  | tee "$artifact_dir/result.txt"
