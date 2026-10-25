#!/usr/bin/env bash
set -Eeuo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
artifact_dir="${ARTIFACT_DIR:-$root/artifacts/wayland}"
probe="${1:-$root/artifacts/probe/p2a_r4_canvas_probe}"
mkdir -p "$artifact_dir"

for cmd in weston wayland-info timeout stat grep; do
  command -v "$cmd" >/dev/null || {
    printf 'Missing required Wayland test command: %s\n' "$cmd" >&2
    exit 2
  }
done
[[ -x "$probe" ]] || { printf 'Probe executable missing: %s\n' "$probe" >&2; exit 2; }

runtime_dir="$(mktemp -d "${RUNNER_TEMP:-/tmp}/p2a-r4-wayland.XXXXXX")"
chmod 0700 "$runtime_dir"
weston_pid=""
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
  stop_pid "$weston_pid"
  rm -rf -- "$runtime_dir"
  exit "$status"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

socket="wayland-p2a-r4-${GITHUB_RUN_ID:-local}-${GITHUB_RUN_ATTEMPT:-0}-$$"
[[ "$(stat -c '%a' "$runtime_dir")" == "700" ]] || {
  printf 'XDG_RUNTIME_DIR is not mode 0700\n' >&2
  exit 1
}
{
  printf 'XDG_RUNTIME_DIR mode: %s\n' "$(stat -c '%a' "$runtime_dir")"
  printf 'Socket: %s\n' "$socket"
  printf 'Backend: headless; renderer: pixman; DISPLAY and inherited Wayland socket unset for server\n'
  printf 'Weston version: '
  weston --version
} | tee "$artifact_dir/environment.txt"

if ! env -u DISPLAY -u WAYLAND_DISPLAY -u WAYLAND_SOCKET \
    XDG_RUNTIME_DIR="$runtime_dir" weston --backend=headless --help \
    >"$artifact_dir/weston-headless-help.log" 2>&1; then
  cat "$artifact_dir/weston-headless-help.log" >&2
  printf 'Installed Weston does not accept the requested headless backend.\n' >&2
  exit 1
fi
for option in --backend --renderer --no-config --socket --log --width --height --scale; do
  grep -Fq -- "$option" "$artifact_dir/weston-headless-help.log" || {
    printf 'Installed Weston help does not advertise required option %s\n' "$option" >&2
    exit 1
  }
done

env -u DISPLAY -u WAYLAND_DISPLAY -u WAYLAND_SOCKET \
  XDG_RUNTIME_DIR="$runtime_dir" \
  weston --backend=headless --renderer=pixman --no-config \
    --socket="$socket" --idle-time=0 --width=800 --height=600 --scale=1 \
    --log="$artifact_dir/weston.log" \
    >"$artifact_dir/weston-bootstrap.log" 2>&1 &
weston_pid="$!"

socket_ready=false
for _ in {1..100}; do
  if [[ -S "$runtime_dir/$socket" ]]; then
    socket_ready=true
    break
  fi
  if ! kill -0 "$weston_pid" 2>/dev/null; then
    break
  fi
  sleep 0.1
done
[[ "$socket_ready" == true ]] || {
  printf 'Weston did not create its unique Wayland socket.\n' >&2
  cat "$artifact_dir/weston-bootstrap.log" "$artifact_dir/weston.log" >&2 2>/dev/null || true
  exit 1
}
stat -c 'Wayland socket type=%F mode=%a owner=%U' "$runtime_dir/$socket" \
  | tee "$artifact_dir/socket.txt"

env -u DISPLAY -u WAYLAND_SOCKET \
  XDG_RUNTIME_DIR="$runtime_dir" WAYLAND_DISPLAY="$socket" \
  GDK_BACKEND=wayland timeout --signal=TERM --kill-after=2s 10s wayland-info \
  >"$artifact_dir/wayland-info.log" 2>&1
grep -q 'wl_compositor' "$artifact_dir/wayland-info.log"

env -u DISPLAY -u WAYLAND_SOCKET \
  XDG_RUNTIME_DIR="$runtime_dir" WAYLAND_DISPLAY="$socket" \
  GDK_BACKEND=wayland WAYLAND_DEBUG=1 stdbuf -oL "$probe" \
  >"$artifact_dir/client.log" 2>&1 &
app_pid="$!"

app_finished=false
for _ in {1..150}; do
  if ! kill -0 "$app_pid" 2>/dev/null; then
    app_finished=true
    break
  fi
  sleep 0.1
done
[[ "$app_finished" == true ]] || {
  printf 'Canvas probe exceeded its 15-second lifecycle bound.\n' >&2
  exit 124
}
set +e
wait "$app_pid"
app_status="$?"
set -e
app_pid=""
printf 'Canvas probe exit status: %s\n' "$app_status" | tee "$artifact_dir/client-exit.txt"
[[ "$app_status" == "0" ]] || { cat "$artifact_dir/client.log" >&2; exit "$app_status"; }

grep -Fq 'CI_PROBE_START' "$artifact_dir/client.log"
grep -Fq 'CI_DRAW width=' "$artifact_dir/client.log"
grep -Fq 'CI_QUIT source=timer' "$artifact_dir/client.log"
grep -Fq 'CI_APP_RETURNED' "$artifact_dir/client.log"
grep -Fq 'xdg_toplevel' "$artifact_dir/client.log"
grep -Fq 'wl_surface' "$artifact_dir/client.log"
if grep -Eiq 'protocol error|wl_display@[^ ]+\.error' "$artifact_dir/client.log"; then
  printf 'Wayland protocol error found in client log.\n' >&2
  exit 1
fi
printf 'Wayland headless smoke: socket connected, Canvas drawn, timer requested ui.quit, app.run returned, exit=0.\n' \
  | tee "$artifact_dir/result.txt"
