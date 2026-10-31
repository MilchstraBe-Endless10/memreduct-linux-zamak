#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -Eeuo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
artifact_dir="$(printenv ARTIFACT_DIR || true)"
tmp_root="$(printenv RUNNER_TEMP || printf /tmp)"
if [[ -z "$artifact_dir" ]]; then artifact_dir="$(mktemp -d "$tmp_root/memreduct-p3a-preferences-artifacts.XXXXXX")"; else mkdir -p "$artifact_dir"; fi

for cmd in Xvfb openbox xdotool xwininfo xprop import identify convert compare xdpyinfo xrdb sha256sum stat; do
  command -v "$cmd" >/dev/null || { printf 'Missing P3A Xvfb test command: %s\n' "$cmd" >&2; exit 2; }
done
app="$1"
[[ -x "$app" ]] || { printf 'Production executable missing: %s\n' "$app" >&2; exit 2; }
[[ "$(id -u)" != 0 ]] || { printf 'P3A GUI persistence test must run unprivileged.\n' >&2; exit 2; }

while IFS= read -r variable; do
  case "$variable" in MEMREDUCT_CI*) unset "$variable" ;; esac
done < <(compgen -e)
if env | grep -q '^MEMREDUCT_CI'; then
  printf 'CI-only test environment leaked into normal-mode persistence.\n' >&2
  exit 1
fi

runtime_dir="$(mktemp -d "$tmp_root/memreduct-p3a-xvfb.XXXXXX")"
chmod 0700 "$runtime_dir"
xvfb_pid=""
wm_pid=""
app_pid=""
window_id=""
settings_window_id=""
app_log=""
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
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp -noreset 3>"$display_file" >"$artifact_dir/xvfb.log" 2>&1 &
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
default_xdg="$runtime_dir/default-config"
test_xdg="$runtime_dir/test-config"
mkdir -m 0700 "$home_dir" "$default_xdg" "$test_xdg"
export HOME="$home_dir"
printf 'mode=normal-without-MEMREDUCT_CI\neuid=%s\nDISPLAY=%s\nGDK_BACKEND=%s\nGTK_THEME=%s\nGDK_SCALE=%s\nGDK_DPI_SCALE=%s\nXDG_CONFIG_HOME=%s\n' \
  "$(id -u)" "$DISPLAY" "$GDK_BACKEND" "$GTK_THEME" "$GDK_SCALE" "$GDK_DPI_SCALE" "$test_xdg" | tee "$artifact_dir/environment.txt"
xdpyinfo -queryExtensions | grep -E '(^|[[:space:]])XTEST([[:space:]]|$)' | tee "$artifact_dir/xtest-extension.txt"
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

window_geometry() {
  local xid="$1" output="$2" width height
  xwininfo -id "$xid" >"$output"
  width="$(awk '$1 == "Width:" {print $2; exit}' "$output")"
  height="$(awk '$1 == "Height:" {print $2; exit}' "$output")"
  [[ "$width" == 337 && "$height" == 369 ]] || {
    printf 'Unexpected main window geometry %sx%s (expected 337x369).\n' "$width" "$height" >&2
    return 1
  }
}
wait_main_window() {
  for _ in {1..100}; do
    window_id="$(xdotool search --onlyvisible --name '^Mem Reduct$' 2>/dev/null | head -n 1 || true)"
    [[ -n "$window_id" ]] && return 0
    kill -0 "$app_pid" 2>/dev/null || break
    sleep 0.1
  done
  printf 'Main window did not appear.\n' >&2
  [[ -n "$app_log" && -f "$app_log" ]] && cat "$app_log" >&2
  return 1
}
start_app() {
  local label="$1" config_home="$2"
  app_log="$artifact_dir/$label.log"
  env HOME="$home_dir" XDG_CONFIG_HOME="$config_home" GDK_BACKEND=x11 GTK_THEME=Adwaita:light \
    GDK_SCALE=1 GDK_DPI_SCALE=1 GSETTINGS_BACKEND=memory XDG_RUNTIME_DIR="$runtime_dir" \
    stdbuf -oL "$app" >"$app_log" 2>&1 &
  app_pid="$!"
  wait_main_window
  window_geometry "$window_id" "$artifact_dir/$label-geometry.txt"
  if grep -Eq 'Gtk-(WARNING|CRITICAL)' "$app_log"; then
    printf 'GTK warning/critical in normal-mode run %s.\n' "$label" >&2
    cat "$app_log" >&2
    return 1
  fi
}
capture_window() {
  local xid="$1" name="$2" expected="$3" actual
  import -window "$xid" "$artifact_dir/$name"
  actual="$(identify -format '%wx%h' "$artifact_dir/$name")"
  [[ "$actual" == "$expected" ]] || {
    printf 'Unexpected capture dimensions for %s: %s (expected %s).\n' "$name" "$actual" "$expected" >&2
    return 1
  }
}
pixels_same() {
  local left="$1" right="$2" report="$3" status metric
  set +e
  compare -metric AE "$left" "$right" null: 2>"$artifact_dir/$report"
  status="$?"
  set -e
  metric="$(sed -nE 's/^[[:space:]]*([0-9]+).*/\1/p' "$artifact_dir/$report" | head -n 1)"
  [[ "$status" == 0 && "$metric" == 0 ]]
}
assert_same() {
  local left="$1" right="$2" report="$3"
  if ! pixels_same "$left" "$right" "$report"; then
    printf 'Expected identical menu state crops; see %s\n' "$artifact_dir/$report" >&2
    cat "$artifact_dir/$report" >&2
    return 1
  fi
  printf 'changed_pixels=0\n' | tee -a "$artifact_dir/$report"
}
assert_changed() {
  local left="$1" right="$2" report="$3" status metric
  set +e
  compare -metric AE "$left" "$right" null: 2>"$artifact_dir/$report"
  status="$?"
  set -e
  metric="$(sed -nE 's/^[[:space:]]*([0-9]+).*/\1/p' "$artifact_dir/$report" | head -n 1)"
  [[ "$status" == 1 && "$metric" =~ ^[0-9]+$ ]] && (( metric > 0 )) || {
    printf 'Expected visible menu/modal pixel change: %s vs %s.\n' "$left" "$right" >&2
    cat "$artifact_dir/$report" >&2
    return 1
  }
  printf 'changed_pixels=%s\n' "$metric" | tee -a "$artifact_dir/$report"
}
capture_state() {
  local prefix="$1" full
  xdotool mousemove --sync --window "$window_id" 70 10 click 1
  sleep 0.15
  xdotool mousemove --sync --window "$window_id" 10 300
  sleep 0.15
  full="$artifact_dir/$prefix-view-full.png"
  capture_window "$window_id" "$prefix-view-full.png" '337x369'
  convert "$full" -crop 256x99+44+24 +repage "$artifact_dir/$prefix-view-menu.png"
  convert "$full" -crop 110x48+225+99 +repage "$artifact_dir/$prefix-language-base.png"
  xdotool mousemove --sync --window "$window_id" 82 110 click 1
  sleep 0.15
  xdotool mousemove --sync --window "$window_id" 10 300
  sleep 0.15
  capture_window "$window_id" "$prefix-language-full.png" '337x369'
  convert "$artifact_dir/$prefix-language-full.png" -crop 110x48+225+99 +repage "$artifact_dir/$prefix-language-menu.png"
  assert_changed "$artifact_dir/$prefix-language-base.png" "$artifact_dir/$prefix-language-menu.png" "$prefix-language-popup-pixels.txt"
  xdotool key --clearmodifiers Escape
  sleep 0.1
}
assert_config() {
  local language="$1" theme="$2" label="$3" expected mode owner
  expected="$runtime_dir/expected-$label.conf"
  printf 'schema=1\nlanguage=%s\ntheme=%s\n' "$language" "$theme" >"$expected"
  [[ -f "$config_file" && ! -L "$config_file" ]] || {
    printf 'Missing regular preferences file: %s\n' "$config_file" >&2
    return 1
  }
  if ! cmp -s "$expected" "$config_file"; then
    printf 'Preference bytes do not match %s/%s (%s):\n' "$language" "$theme" "$label" >&2
    cat "$config_file" >&2 || true
    return 1
  fi
  mode="$(stat -c '%a' "$config_file")"
  owner="$(stat -c '%u' "$config_file")"
  [[ "$mode" == 600 && "$owner" == "$(id -u)" ]] || {
    printf 'Unsafe preference metadata: mode=%s owner=%s\n' "$mode" "$owner" >&2
    return 1
  }
  [[ "$(stat -c '%a' "$config_dir")" == 700 && "$(stat -c '%u' "$config_dir")" == "$(id -u)" ]]
  printf 'state=%s/%s bytes=canonical mode=%s owner=%s\n' "$language" "$theme" "$mode" "$owner" | tee -a "$artifact_dir/preferences-state-matrix.txt"
}
close_app() {
  local status process_state exited=false
  xdotool windowfocus --sync "$window_id"
  xdotool key --clearmodifiers Alt+F4
  for _ in {1..100}; do
    if ! kill -0 "$app_pid" 2>/dev/null; then
      exited=true
      break
    fi
    process_state="$(awk '{print $3}' "/proc/$app_pid/stat" 2>/dev/null || true)"
    if [[ "$process_state" == Z ]]; then
      exited=true
      break
    fi
    sleep 0.1
  done
  if [[ "$exited" != true ]]; then
    printf 'Application did not exit after Alt+F4.\n' >&2
    ps -o pid,state,stat,cmd -p "$app_pid" >&2 || true
    cat "$app_log" >&2
    return 1
  fi
  set +e
  wait "$app_pid"
  status="$?"
  set -e
  app_pid=""
  window_id=""
  [[ "$status" == 0 ]] || {
    printf 'Application exited with %s, expected 0.\n' "$status" >&2
    cat "$app_log" >&2
    return "$status"
  }
}
restart_and_compare() {
  local state="$1" language="$2" theme="$3" before_view="$4" before_language="$5"
  close_app
  start_app "restart-$state" "$test_xdg"
  assert_config "$language" "$theme" "restart-$state"
  capture_state "restart-$state"
  assert_same "$before_view" "$artifact_dir/restart-$state-view-menu.png" "restart-$state-view-identical.txt"
  assert_same "$before_language" "$artifact_dir/restart-$state-language-menu.png" "restart-$state-language-identical.txt"
}
select_theme() {
  xdotool mousemove --sync --window "$window_id" 70 10 click 1
  sleep 0.1
  xdotool mousemove --sync --window "$window_id" 70 59 click 1
  sleep 0.15
}
cancel_theme_selection() {
  local before after
  before="$(sha256sum "$config_file" | awk '{print $1}')"
  xdotool mousemove --sync --window "$window_id" 70 10 click 1
  sleep 0.1
  capture_window "$window_id" theme-cancel-popup.png '337x369'
  xdotool key --clearmodifiers Escape
  xdotool mousemove --sync --window "$window_id" 10 300
  sleep 0.15
  capture_window "$window_id" theme-cancel-closed.png '337x369'
  after="$(sha256sum "$config_file" | awk '{print $1}')"
  [[ "$before" == "$after" ]] || {
    printf 'Cancelling the View/theme menu changed persisted preferences.\n' >&2
    return 1
  }
  assert_changed "$artifact_dir/theme-cancel-popup.png" \
    "$artifact_dir/theme-cancel-closed.png" theme-cancel-popup-dismissed.txt
  printf 'theme_menu_cancelled=true preferences_unchanged=true\n' \
    | tee -a "$artifact_dir/preferences-state-matrix.txt"
}
select_language() {
  local language="$1" y
  xdotool mousemove --sync --window "$window_id" 70 10 click 1
  sleep 0.1
  xdotool mousemove --sync --window "$window_id" 82 110 click 1
  sleep 0.1
  y=134
  [[ "$language" == en ]] && y=112
  xdotool mousemove --sync --window "$window_id" 238 "$y" click 1
  sleep 0.25
}
open_settings() {
  xdotool windowfocus --sync "$window_id"
  xdotool key --clearmodifiers F2
  for _ in {1..100}; do
    settings_window_id="$(xdotool search --onlyvisible --name '^Settings - Mem Reduct$' 2>/dev/null | head -n 1 || true)"
    [[ -n "$settings_window_id" ]] && break
    kill -0 "$app_pid" 2>/dev/null || break
    sleep 0.1
  done
  [[ -n "$settings_window_id" ]] || { printf 'Settings window did not appear.\n' >&2; cat "$app_log" >&2; return 1; }
  xwininfo -id "$settings_window_id" >"$artifact_dir/settings-geometry.txt"
  local width height
  width="$(awk '$1 == "Width:" {print $2; exit}' "$artifact_dir/settings-geometry.txt")"
  height="$(awk '$1 == "Height:" {print $2; exit}' "$artifact_dir/settings-geometry.txt")"
  [[ "$width" == 558 && "$height" == 335 ]] || {
    printf 'Unexpected settings client size: %sx%s (expected 558x335).\n' "$width" "$height" >&2
    return 1
  }
}
capture_settings_nav_page() {
  local page="$1" method="$2" image crop nav x y
  x=50
  y=$((20 + page * 20))
  if [[ "$method" == mouse ]]; then
    xdotool mousemove --sync --window "$settings_window_id" "$x" "$y" click 1
    sleep 0.15
  fi
  xdotool mousemove --sync --window "$settings_window_id" 540 320
  sleep 0.1
  image="$artifact_dir/settings-$method-page-$((101 + page)).png"
  crop="$artifact_dir/settings-$method-page-$((101 + page))-content.png"
  nav="$artifact_dir/settings-$method-page-$((101 + page))-navigation.png"
  capture_window "$settings_window_id" "settings-$method-page-$((101 + page)).png" '558x335'
  convert "$image" -crop 390x273+156+10 +repage "$crop"
  convert "$image" -crop 132x100+12+10 +repage "$nav"
  printf 'method=%s expected_page_idd=%s navigation_row=%s image=%s\n' \
    "$method" "$((101 + page))" "$page" "$(basename "$image")" \
    | tee -a "$artifact_dir/settings-page-navigation.txt"
}
assert_settings_navigation_pages() {
  local page earlier
  # Mouse-select all five actual production pages. Each content panel and the
  # selected navigation row must produce a distinct rendered state.
  for page in 0 1 2 3 4; do
    capture_settings_nav_page "$page" mouse
  done
  for page in 1 2 3 4; do
    for earlier in $(seq 0 $((page - 1))); do
      assert_changed \
        "$artifact_dir/settings-mouse-page-$((101 + earlier))-content.png" \
        "$artifact_dir/settings-mouse-page-$((101 + page))-content.png" \
        "settings-page-content-$((101 + earlier))-$((101 + page)).txt"
      assert_changed \
        "$artifact_dir/settings-mouse-page-$((101 + earlier))-navigation.png" \
        "$artifact_dir/settings-mouse-page-$((101 + page))-navigation.png" \
        "settings-page-selection-$((101 + earlier))-$((101 + page)).txt"
    done
  done

  # Keyboard navigation must reach the same five pages as mouse navigation.
  xdotool mousemove --sync --window "$settings_window_id" 50 20 click 1
  xdotool mousemove --sync --window "$settings_window_id" 540 320
  capture_window "$settings_window_id" settings-keyboard-page-101.png '558x335'
  assert_same "$artifact_dir/settings-mouse-page-101.png" \
    "$artifact_dir/settings-keyboard-page-101.png" settings-page-keyboard-101.txt
  for page in 1 2 3 4; do
    xdotool key --clearmodifiers Down
    sleep 0.1
    capture_window "$settings_window_id" \
      "settings-keyboard-page-$((101 + page)).png" '558x335'
    assert_same "$artifact_dir/settings-mouse-page-$((101 + page)).png" \
      "$artifact_dir/settings-keyboard-page-$((101 + page)).png" \
      "settings-page-keyboard-$((101 + page)).txt"
  done
  xdotool key --clearmodifiers Return
  capture_window "$settings_window_id" settings-keyboard-enter-page-105.png '558x335'
  assert_same "$artifact_dir/settings-mouse-page-105.png" \
    "$artifact_dir/settings-keyboard-enter-page-105.png" settings-page-enter-105.txt
}
assert_settings_window_returns_to_main() {
  local reason="$1" closed_id visible_settings main_visible
  closed_id="$settings_window_id"
  for _ in {1..100}; do
    visible_settings="$(xdotool search --onlyvisible --name '^Settings - Mem Reduct$' 2>/dev/null || true)"
    if ! grep -Fxq -- "$closed_id" <<<"$visible_settings"; then
      main_visible="$(xdotool search --onlyvisible --name '^Mem Reduct$' 2>/dev/null || true)"
      [[ -n "$main_visible" ]] || {
        printf 'Main window was not visible after Settings closed by %s.\n' "$reason" >&2
        return 1
      }
      [[ -n "$app_pid" ]] && kill -0 "$app_pid" 2>/dev/null || {
        printf 'Application exited when Settings closed by %s.\n' "$reason" >&2
        return 1
      }
      settings_window_id=""
      printf 'settings_close_method=%s settings_xid=%s main_visible=true process_alive=true\n' \
        "$reason" "$closed_id" | tee -a "$artifact_dir/settings-lifecycle.txt"
      return 0
    fi
    sleep 0.1
  done
  printf 'Settings stayed visible after close method %s.\n' "$reason" >&2
  return 1
}
capture_live_settings_sync() {
  local name="$1"
  xdotool windowraise "$settings_window_id"
  xdotool windowfocus --sync "$settings_window_id"
  sleep 0.15
  capture_window "$settings_window_id" "$name.png" '558x335'
  convert "$artifact_dir/$name.png" -crop 390x273+156+10 +repage \
    "$artifact_dir/$name-content.png"
}
exercise_windows_only_setting_is_read_only() {
  local before after
  before="$(sha256sum "$config_file" | awk '{print $1}')"
  capture_window "$settings_window_id" settings-windows-only-before.png '558x335'
  convert "$artifact_dir/settings-windows-only-before.png" \
    -crop 390x36+156+282 +repage "$artifact_dir/settings-windows-only-before-status.png"
  # Resource 116 is the Windows-only Always on top checkbox in General.
  xdotool mousemove --sync --window "$settings_window_id" 220 38 click 1
  sleep 0.2
  capture_window "$settings_window_id" settings-windows-only-after.png '558x335'
  convert "$artifact_dir/settings-windows-only-after.png" \
    -crop 390x36+156+282 +repage "$artifact_dir/settings-windows-only-after-status.png"
  assert_changed "$artifact_dir/settings-windows-only-before-status.png" \
    "$artifact_dir/settings-windows-only-after-status.png" \
    settings-windows-only-feedback-visible.txt
  after="$(sha256sum "$config_file" | awk '{print $1}')"
  [[ "$before" == "$after" ]] || {
    printf 'Clicking unsupported resource 116 changed persisted preferences.\n' >&2
    return 1
  }
  printf 'resource=116 platform=Windows-only feedback=visible persisted_preferences=unchanged\n' \
    | tee -a "$artifact_dir/preferences-state-matrix.txt"
}
exercise_settings_live_sync() {
  open_settings
  capture_live_settings_sync settings-live-sync-en-light

  # Change the real main-window preference while Settings remains open. Its
  # rendered palette and language must follow the shared application state.
  xdotool windowfocus --sync "$window_id"
  select_theme
  assert_config en dark settings-live-theme-dark
  capture_live_settings_sync settings-live-sync-en-dark
  assert_changed "$artifact_dir/settings-live-sync-en-light-content.png" \
    "$artifact_dir/settings-live-sync-en-dark-content.png" settings-live-theme-redraw.txt

  xdotool windowfocus --sync "$window_id"
  select_language zh-CN
  assert_config zh-CN dark settings-live-locale-zh
  capture_live_settings_sync settings-live-sync-zh-dark
  assert_changed "$artifact_dir/settings-live-sync-en-dark-content.png" \
    "$artifact_dir/settings-live-sync-zh-dark-content.png" settings-live-locale-redraw.txt

  xdotool windowfocus --sync "$window_id"
  select_language en
  assert_config en dark settings-live-locale-en
  xdotool windowfocus --sync "$window_id"
  select_theme
  assert_config en light settings-live-theme-light
  capture_live_settings_sync settings-live-sync-en-light-restored
  assert_same "$artifact_dir/settings-live-sync-en-light-content.png" \
    "$artifact_dir/settings-live-sync-en-light-restored-content.png" \
    settings-live-theme-locale-restored.txt
  printf 'live_settings_sync=English/Light->English/Dark->简体中文/Dark->English/Light restored=true\n' \
    | tee -a "$artifact_dir/settings-lifecycle.txt"
  close_settings
}
exercise_settings_lifecycle() {
  local retained_id
  open_settings
  retained_id="$settings_window_id"
  exercise_windows_only_setting_is_read_only
  assert_settings_navigation_pages

  # Escape closes only Settings; reopening must restore the same native window.
  xdotool key --clearmodifiers Escape
  assert_settings_window_returns_to_main Escape
  open_settings
  [[ "$settings_window_id" == "$retained_id" ]] || {
    printf 'Escape/reopen did not reuse the Settings window. before=%s after=%s\n' \
      "$retained_id" "$settings_window_id" >&2
    return 1
  }
  printf 'settings_reopened=true method=Escape reused_xid=%s\n' "$settings_window_id" \
    | tee -a "$artifact_dir/settings-lifecycle.txt"

  # Alt+F4 goes through the window manager's normal close protocol. Do not use
  # xdotool `windowclose`: it destroys the X resource and bypasses GTK teardown.
  xdotool windowfocus --sync "$settings_window_id"
  xdotool key --clearmodifiers Alt+F4
  assert_settings_window_returns_to_main Alt+F4
  open_settings
  [[ "$settings_window_id" == "$retained_id" ]] || {
    printf 'Alt+F4/reopen did not reuse the Settings window. before=%s after=%s\n' \
      "$retained_id" "$settings_window_id" >&2
    return 1
  }
  close_settings
}
settings_language_combo_coords() {
  # Mirror settings_page_rect/settings_child_rect plus resource 123 bounds.
  # The settings client is fixed at 558x335; page reference and DLU scaling
  # therefore reduce directly to the same values used by the geometry module.
  python3 - <<'PY'
page_x = 156.0
page_y = 10.0
page_width = 390.0
page_height = 273.0
page_dlu_width = 260.0
page_dlu_height = 168.0
combo_x_dlu, combo_y_dlu, combo_width_dlu, combo_height_dlu = 128.0, 104.0, 124.0, 12.0
combo_x = page_x + combo_x_dlu * page_width / page_dlu_width
combo_y = page_y + combo_y_dlu * page_height / page_dlu_height
combo_width = combo_width_dlu * page_width / page_dlu_width
combo_height = combo_height_dlu * page_height / page_dlu_height
print(round(combo_x + combo_width / 2), round(combo_y + combo_height / 2),
      round(combo_y + combo_height + 9), round(combo_y + combo_height + 27),
      round(combo_x), round(combo_y + combo_height), round(combo_width), 36)
PY
}
capture_reset_roi() {
  local xid="$1" name="$2"
  capture_window "$xid" "$name-full.png" '558x335'
  convert "$artifact_dir/$name-full.png" -crop 378x117+168+117 +repage "$artifact_dir/$name-roi.png"
}
request_reset_keyboard_from_general() {
  xdotool key --clearmodifiers Tab
  xdotool key --clearmodifiers Tab
  xdotool key --clearmodifiers Return
  sleep 0.2
}
close_settings() {
  local closed_id="$settings_window_id" visible_settings main_visible
  xdotool mousemove --sync --window "$settings_window_id" 508 310 click 1
  for _ in {1..100}; do
    visible_settings="$(xdotool search --onlyvisible --name '^Settings - Mem Reduct$' 2>/dev/null || true)"
    if ! grep -Fxq -- "$closed_id" <<<"$visible_settings"; then
      main_visible="$(xdotool search --onlyvisible --name '^Mem Reduct$' 2>/dev/null || true)"
      [[ -n "$main_visible" ]] || { printf 'Main window was not visible after closing Settings.\n' >&2; return 1; }
      settings_window_id=""
      printf 'settings_hidden=true settings_xid=%s main_visible=true\n' "$closed_id" | tee -a "$artifact_dir/settings-lifecycle.txt"
      return 0
    fi
    sleep 0.1
  done
  printf 'Settings Close did not hide its native window.\n' >&2
  return 1
}
copy_state_record() {
  local label="$1"
  if [[ -f "$config_file" ]]; then
    cp -- "$config_file" "$artifact_dir/$label-preferences.conf"
    sha256sum "$artifact_dir/$label-preferences.conf" >>"$artifact_dir/preferences-sha256.txt"
  fi
  if [[ -f "$lock_file" ]]; then cp -- "$lock_file" "$artifact_dir/$label-preferences.lock"; fi
}

config_dir="$default_xdg/memreduct-zamak"
config_file="$config_dir/preferences.conf"
lock_file="$config_dir/preferences.lock"
start_app fresh-default "$default_xdg"
capture_state fresh-default
[[ ! -e "$config_file" ]] || { printf 'Missing-config startup created preferences.conf without a user action.\n' >&2; exit 1; }
close_app

config_dir="$test_xdg/memreduct-zamak"
config_file="$config_dir/preferences.conf"
lock_file="$config_dir/preferences.lock"
mkdir -m 0700 "$config_dir"
printf 'schema=1\nlanguage=en\ntheme=light\n' >"$config_file"
chmod 0600 "$config_file"
assert_config en light seed

start_app en-light-first "$test_xdg"
assert_config en light en-light-first
cancel_theme_selection
capture_state en-light-before
copy_state_record en-light-first
restart_and_compare en-light en light "$artifact_dir/en-light-before-view-menu.png" "$artifact_dir/en-light-before-language-menu.png"

select_theme
assert_config en dark en-dark-save
capture_state en-dark-before
assert_changed "$artifact_dir/en-light-before-view-menu.png" "$artifact_dir/en-dark-before-view-menu.png" en-dark-visible.txt
copy_state_record en-dark
restart_and_compare en-dark en dark "$artifact_dir/en-dark-before-view-menu.png" "$artifact_dir/en-dark-before-language-menu.png"

select_language zh-CN
assert_config zh-CN dark zh-dark-save
capture_state zh-dark-before
assert_changed "$artifact_dir/en-dark-before-view-menu.png" "$artifact_dir/zh-dark-before-view-menu.png" zh-locale-visible-in-view.txt
assert_changed "$artifact_dir/en-dark-before-language-menu.png" "$artifact_dir/zh-dark-before-language-menu.png" zh-language-check-visible.txt
copy_state_record zh-dark
restart_and_compare zh-dark zh-CN dark "$artifact_dir/zh-dark-before-view-menu.png" "$artifact_dir/zh-dark-before-language-menu.png"

select_theme
assert_config zh-CN light zh-light-save
capture_state zh-light-before
assert_changed "$artifact_dir/zh-dark-before-view-menu.png" "$artifact_dir/zh-light-before-view-menu.png" zh-light-theme-visible.txt
copy_state_record zh-light
restart_and_compare zh-light zh-CN light "$artifact_dir/zh-light-before-view-menu.png" "$artifact_dir/zh-light-before-language-menu.png"

select_language en
assert_config en light en-light-save
capture_state en-light-final-before
assert_same "$artifact_dir/en-light-before-view-menu.png" "$artifact_dir/en-light-final-before-view-menu.png" en-light-cycle-view-identical.txt
assert_same "$artifact_dir/en-light-before-language-menu.png" "$artifact_dir/en-light-final-before-language-menu.png" en-light-cycle-language-identical.txt
copy_state_record en-light-final
restart_and_compare en-light-final en light "$artifact_dir/en-light-final-before-view-menu.png" "$artifact_dir/en-light-final-before-language-menu.png"

# Exercise resource 123's General-page ComboBox independently of the main
# View-menu language command. Escape must not write; mouse choice must persist.
open_settings
close_settings
open_settings
read -r settings_combo_x settings_combo_y settings_english_row_y settings_chinese_row_y \
  settings_combo_popup_x settings_combo_popup_y settings_combo_popup_w settings_combo_popup_h \
  < <(settings_language_combo_coords | tee "$artifact_dir/resource-123-click-coordinates.txt")
combo_sha_before="$(sha256sum "$config_file" | awk '{print $1}')"
capture_window "$settings_window_id" settings-language-combo-before.png '558x335'
convert "$artifact_dir/settings-language-combo-before.png" \
  -crop "${settings_combo_popup_w}x${settings_combo_popup_h}+${settings_combo_popup_x}+${settings_combo_popup_y}" +repage \
  "$artifact_dir/settings-language-combo-popup-before.roi.png"
xdotool mousemove --sync --window "$settings_window_id" "$settings_combo_x" "$settings_combo_y" click 1
sleep 0.2
capture_window "$settings_window_id" settings-language-combo-popup.png '558x335'
convert "$artifact_dir/settings-language-combo-popup.png" \
  -crop "${settings_combo_popup_w}x${settings_combo_popup_h}+${settings_combo_popup_x}+${settings_combo_popup_y}" +repage \
  "$artifact_dir/settings-language-combo-popup-open.roi.png"
assert_changed "$artifact_dir/settings-language-combo-popup-before.roi.png" \
  "$artifact_dir/settings-language-combo-popup-open.roi.png" settings-language-combo-popup.txt
xdotool key --clearmodifiers Escape
sleep 0.2
capture_window "$settings_window_id" settings-language-combo-escape.png '558x335'
convert "$artifact_dir/settings-language-combo-escape.png" \
  -crop "${settings_combo_popup_w}x${settings_combo_popup_h}+${settings_combo_popup_x}+${settings_combo_popup_y}" +repage \
  "$artifact_dir/settings-language-combo-escape.roi.png"
assert_changed "$artifact_dir/settings-language-combo-popup-open.roi.png" \
  "$artifact_dir/settings-language-combo-escape.roi.png" settings-language-combo-escape.txt
[[ "$(sha256sum "$config_file" | awk '{print $1}')" == "$combo_sha_before" ]] || {
  printf 'Cancelling resource 123 language popup changed persisted settings.\n' >&2
  exit 1
}
xdotool mousemove --sync --window "$settings_window_id" "$settings_combo_x" "$settings_combo_y" click 1
sleep 0.15
xdotool mousemove --sync --window "$settings_window_id" "$settings_combo_x" "$settings_chinese_row_y" click 1
sleep 0.3
assert_config zh-CN light settings-combo-mouse-zh
capture_window "$settings_window_id" settings-language-combo-zh.png '558x335'
close_settings
capture_state settings-combo-zh
assert_changed "$artifact_dir/en-light-final-before-view-menu.png" \
  "$artifact_dir/settings-combo-zh-view-menu.png" settings-combo-main-locale.txt
close_app
start_app settings-combo-restart-zh "$test_xdg"
assert_config zh-CN light settings-combo-restart-zh
capture_state settings-combo-restart-zh
assert_same "$artifact_dir/settings-combo-zh-view-menu.png" \
  "$artifact_dir/settings-combo-restart-zh-view-menu.png" settings-combo-restart-view.txt
assert_same "$artifact_dir/settings-combo-zh-language-menu.png" \
  "$artifact_dir/settings-combo-restart-zh-language-menu.png" settings-combo-restart-language.txt
open_settings
xdotool mousemove --sync --window "$settings_window_id" "$settings_combo_x" "$settings_combo_y" click 1
sleep 0.15
xdotool mousemove --sync --window "$settings_window_id" "$settings_combo_x" "$settings_english_row_y" click 1
sleep 0.3
assert_config en light settings-combo-mouse-en
close_settings
capture_state settings-combo-en-restored
assert_same "$artifact_dir/en-light-final-before-view-menu.png" \
  "$artifact_dir/settings-combo-en-restored-view-menu.png" settings-combo-en-restored-view.txt
close_app
start_app settings-combo-restart-en "$test_xdg"
assert_config en light settings-combo-restart-en
capture_state settings-combo-restart-en
assert_same "$artifact_dir/en-light-final-before-view-menu.png" \
  "$artifact_dir/settings-combo-restart-en-view-menu.png" settings-combo-restart-en-view.txt
close_app
start_app settings-combo-reset-base "$test_xdg"
assert_config en light settings-combo-reset-base

# Real normal-mode keyboard path: Tab reaches resource 123, Space opens it,
# Escape cancels without writing, then End+Enter selects Simplified Chinese.
open_settings
combo_sha_before="$(sha256sum "$config_file" | awk '{print $1}')"
capture_window "$settings_window_id" settings-keyboard-combo-before.png '558x335'
convert "$artifact_dir/settings-keyboard-combo-before.png" \
  -crop "${settings_combo_popup_w}x${settings_combo_popup_h}+${settings_combo_popup_x}+${settings_combo_popup_y}" +repage \
  "$artifact_dir/settings-keyboard-combo-before.roi.png"
xdotool key --clearmodifiers Tab
xdotool key --clearmodifiers space
sleep 0.2
capture_window "$settings_window_id" settings-keyboard-combo-popup.png '558x335'
convert "$artifact_dir/settings-keyboard-combo-popup.png" \
  -crop "${settings_combo_popup_w}x${settings_combo_popup_h}+${settings_combo_popup_x}+${settings_combo_popup_y}" +repage \
  "$artifact_dir/settings-keyboard-combo-popup.roi.png"
assert_changed "$artifact_dir/settings-keyboard-combo-before.roi.png" \
  "$artifact_dir/settings-keyboard-combo-popup.roi.png" settings-keyboard-combo-open.txt
xdotool key --clearmodifiers Escape
sleep 0.2
capture_window "$settings_window_id" settings-keyboard-combo-escape.png '558x335'
convert "$artifact_dir/settings-keyboard-combo-escape.png" \
  -crop "${settings_combo_popup_w}x${settings_combo_popup_h}+${settings_combo_popup_x}+${settings_combo_popup_y}" +repage \
  "$artifact_dir/settings-keyboard-combo-escape.roi.png"
assert_changed "$artifact_dir/settings-keyboard-combo-popup.roi.png" \
  "$artifact_dir/settings-keyboard-combo-escape.roi.png" settings-keyboard-combo-escape.txt
[[ "$(sha256sum "$config_file" | awk '{print $1}')" == "$combo_sha_before" ]] || {
  printf 'Keyboard Escape on resource 123 changed persisted settings.\n' >&2
  exit 1
}
xdotool key --clearmodifiers space
xdotool key --clearmodifiers End
xdotool key --clearmodifiers Return
sleep 0.3
assert_config zh-CN light settings-combo-keyboard-zh
close_settings
capture_state settings-keyboard-zh
close_app
start_app settings-keyboard-restart-zh "$test_xdg"
assert_config zh-CN light settings-keyboard-restart-zh
capture_state settings-keyboard-restart-zh
assert_same "$artifact_dir/settings-keyboard-zh-view-menu.png" \
  "$artifact_dir/settings-keyboard-restart-zh-view-menu.png" settings-keyboard-restart-zh-view.txt
open_settings
xdotool key --clearmodifiers Tab
xdotool key --clearmodifiers space
xdotool key --clearmodifiers Home
xdotool key --clearmodifiers Return
sleep 0.3
assert_config en light settings-combo-keyboard-en
close_settings
capture_state settings-keyboard-en-restored
assert_same "$artifact_dir/en-light-final-before-view-menu.png" \
  "$artifact_dir/settings-keyboard-en-restored-view-menu.png" settings-keyboard-en-restored-view.txt
close_app
start_app settings-keyboard-reset-base "$test_xdg"
assert_config en light settings-keyboard-reset-base
exercise_settings_live_sync
exercise_settings_lifecycle
assert_config en light settings-keyboard-reset-after-lifecycle
close_app
start_app settings-reset-base "$test_xdg"
assert_config en light settings-reset-base

default_state=""
for candidate in en-light en-dark zh-dark zh-light; do
  candidate_view="$artifact_dir/$candidate-before-view-menu.png"
  candidate_language="$artifact_dir/$candidate-before-language-menu.png"
  if [[ "$candidate" == en-light ]]; then
    candidate_view="$artifact_dir/en-light-before-view-menu.png"
    candidate_language="$artifact_dir/en-light-before-language-menu.png"
  fi
  if pixels_same "$artifact_dir/fresh-default-view-menu.png" "$candidate_view" "default-$candidate-view-match.txt" &&
     pixels_same "$artifact_dir/fresh-default-language-menu.png" "$candidate_language" "default-$candidate-language-match.txt"; then
    [[ -z "$default_state" ]] || { printf 'Fresh default ambiguously matched multiple states.\n' >&2; exit 1; }
    default_state="$candidate"
  fi
done
[[ -n "$default_state" ]] || { printf 'Fresh default did not match any of four language/theme state pairs.\n' >&2; exit 1; }
printf 'fresh_default_state=%s\n' "$default_state" | tee "$artifact_dir/default-state.txt"
default_theme="$(printf '%s' "$default_state" | cut -d- -f2)"
case "$default_state" in
  en-*) default_language=en ;;
  zh-*) default_language=zh-CN ;;
  *) printf 'Invalid default-state key: %s\n' "$default_state" >&2; exit 1 ;;
esac

reset_state="en-light"
[[ "$default_state" == en-light ]] && reset_state="zh-dark"
if [[ "$reset_state" == zh-dark ]]; then
  select_language zh-CN
  assert_config zh-CN light reset-prepare-zh
  select_theme
  assert_config zh-CN dark reset-prepare-zh-dark
fi
case "$reset_state" in
  en-light) reset_language=en; reset_theme=light ;;
  zh-dark) reset_language=zh-CN; reset_theme=dark ;;
esac
capture_state reset-$reset_state-before
reset_sha="$(sha256sum "$config_file" | awk '{print $1}')"
open_settings
capture_reset_roi "$settings_window_id" reset-cancel-before
request_reset_keyboard_from_general
capture_reset_roi "$settings_window_id" reset-cancel-open
assert_changed "$artifact_dir/reset-cancel-before-roi.png" "$artifact_dir/reset-cancel-open-roi.png" reset-confirm-visible.txt
assert_config "$reset_language" "$reset_theme" reset-cancel-open
[[ "$(sha256sum "$config_file" | awk '{print $1}')" == "$reset_sha" ]] || { printf 'Opening Reset changed preferences before confirmation.\n' >&2; exit 1; }
xdotool key --clearmodifiers Tab
xdotool key --clearmodifiers Return
sleep 0.2
capture_reset_roi "$settings_window_id" reset-cancel-after
assert_same "$artifact_dir/reset-cancel-before-roi.png" "$artifact_dir/reset-cancel-after-roi.png" reset-cancel-dismissed.txt
assert_config "$reset_language" "$reset_theme" reset-cancelled
[[ "$(sha256sum "$config_file" | awk '{print $1}')" == "$reset_sha" ]] || { printf 'Cancelling Reset changed preferences.\n' >&2; exit 1; }

xdotool key --clearmodifiers Return
sleep 0.2
capture_reset_roi "$settings_window_id" reset-confirm-open
assert_changed "$artifact_dir/reset-cancel-before-roi.png" "$artifact_dir/reset-confirm-open-roi.png" reset-confirm-visible-again.txt
assert_config "$reset_language" "$reset_theme" reset-confirm-before-write
xdotool key --clearmodifiers Return
sleep 0.4
assert_config "$default_language" "$default_theme" reset-confirmed
[[ "$(sha256sum "$config_file" | awk '{print $1}')" != "$reset_sha" ]] || { printf 'Confirmed Reset did not change the non-default bytes.\n' >&2; exit 1; }
copy_state_record reset-confirmed
retained_settings_window_id="$settings_window_id"
close_settings
capture_state reset-default-live
assert_same "$artifact_dir/fresh-default-view-menu.png" "$artifact_dir/reset-default-live-view-menu.png" reset-default-live-view.txt
assert_same "$artifact_dir/fresh-default-language-menu.png" "$artifact_dir/reset-default-live-language-menu.png" reset-default-live-language.txt
open_settings
[[ "$settings_window_id" == "$retained_settings_window_id" ]] || {
  printf 'Reopening Settings did not reuse the retained native window. previous=%s reopened=%s\n' \
    "$retained_settings_window_id" "$settings_window_id" >&2
  exit 1
}
printf 'settings_reopened=true reused_xid=%s\n' "$settings_window_id" | tee -a "$artifact_dir/settings-lifecycle.txt"
close_settings
close_app

start_app reset-default-restart "$test_xdg"
assert_config "$default_language" "$default_theme" reset-default-restart
capture_state reset-default-restarted
assert_same "$artifact_dir/fresh-default-view-menu.png" "$artifact_dir/reset-default-restarted-view-menu.png" reset-default-restart-view.txt
assert_same "$artifact_dir/fresh-default-language-menu.png" "$artifact_dir/reset-default-restarted-language-menu.png" reset-default-restart-language.txt
if grep -Eiq 'drop_caches|pkexec|polkit|swapoff|zramctl|mr_start_reclaim|memreduct-zamak-helper' "$artifact_dir"/fresh-default.log "$artifact_dir"/en-light-first.log "$artifact_dir"/restart-*.log; then
  printf 'Normal-mode GUI logs mention a privileged operation.\n' >&2
  exit 1
fi
close_app
sha256sum "$artifact_dir"/*.png | sed "s#$artifact_dir/##" >"$artifact_dir/screenshots-sha256.txt"
printf 'PASS: normal-mode XDG save/restart for all four language/theme pairs; Reset cancellation preserved bytes; confirmed Reset restored fresh-process defaults after restart. MEMREDUCT_CI was unset throughout; no real user/system config or privileged operation was used.\n' | tee "$artifact_dir/result.txt"
