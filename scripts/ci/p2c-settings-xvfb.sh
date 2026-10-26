#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -Eeuo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
artifact_dir="${ARTIFACT_DIR:-$root/artifacts/p2c-x11}"
app="${1:-$artifact_dir/memreduct-zamak-production}"
font_candidate="${P2C_FONT_CANDIDATE:-default}"
fontconfig_mode=system-default
mkdir -p "$artifact_dir"
for cmd in Xvfb openbox wmctrl xdotool xwininfo xprop import identify sha256sum xrdb; do
  command -v "$cmd" >/dev/null || { printf 'Missing X11 test command: %s\n' "$cmd" >&2; exit 2; }
done
[[ -x "$app" ]] || { printf 'Production executable missing.\n' >&2; exit 2; }

runtime_dir="$(mktemp -d "${RUNNER_TEMP:-/tmp}/memreduct-p2c-x11.XXXXXX")"
chmod 0700 "$runtime_dir"
xvfb_pid=""; wm_pid=""; app_pid=""
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
  3>"$display_file" >"$artifact_dir/p2c-xvfb.log" 2>&1 &
xvfb_pid="$!"
for _ in {1..100}; do
  [[ -s "$display_file" ]] && break
  kill -0 "$xvfb_pid" 2>/dev/null || break
  sleep 0.1
done
[[ -s "$display_file" ]] || { cat "$artifact_dir/p2c-xvfb.log" >&2; exit 1; }
display_number="$(<"$display_file")"
export DISPLAY=":$display_number"
unset WAYLAND_DISPLAY WAYLAND_SOCKET
export GDK_BACKEND=x11 GTK_THEME=Adwaita:light GDK_SCALE=1 GDK_DPI_SCALE=1 LANG=C.UTF-8
export GSETTINGS_BACKEND=memory XDG_RUNTIME_DIR="$runtime_dir"
mkdir -m 0700 "$runtime_dir/home" "$runtime_dir/config"
export HOME="$runtime_dir/home" XDG_CONFIG_HOME="$runtime_dir/config"
if [[ "$font_candidate" == liberation ]]; then
  [[ -r /usr/share/doc/fonts-liberation2/copyright ]] || {
    printf 'Liberation font package copyright record is missing.\n' >&2
    exit 2
  }
  cat >"$runtime_dir/fonts-liberation.xml" <<'XML'
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">
<fontconfig>
  <include>/etc/fonts/fonts.conf</include>
  <match target="pattern">
    <test name="family" qual="any" compare="eq"><string>Sans</string></test>
    <edit name="family" mode="prepend" binding="strong"><string>Liberation Sans</string></edit>
  </match>
</fontconfig>
XML
  export FONTCONFIG_FILE="$runtime_dir/fonts-liberation.xml" FONTCONFIG_PATH=/etc/fonts
  fontconfig_mode=isolated-process-override
  cp /usr/share/doc/fonts-liberation2/copyright "$artifact_dir/font-candidate-license.txt"
elif [[ "$font_candidate" != default ]]; then
  printf 'Unsupported P2C_FONT_CANDIDATE: %s\n' "$font_candidate" >&2
  exit 2
fi
{
  printf 'XDG_RUNTIME_DIR_mode=%s\nDISPLAY=%s\nGDK_BACKEND=%s\nGTK_THEME=%s\nGTK_THEME_source=isolated process environment; application palette state is controlled separately by the View menu\nP2C_font_candidate=%s\nFONTCONFIG_mode=%s\nGDK_SCALE=%s\nGDK_DPI_SCALE=%s\nXvfb_screen=1024x768x24\n' \
    "$(stat -c '%a' "$runtime_dir")" "$DISPLAY" "$GDK_BACKEND" "$GTK_THEME" "$font_candidate" "$fontconfig_mode" "$GDK_SCALE" "$GDK_DPI_SCALE"
  printf 'GTK_version=%s\nEnglish_font=' "$(pkg-config --modversion gtk+-3.0)"
  printf 'Fontconfig_version=%s\n' "$(fc-match --version | head -n 1)"
  fc-match 'sans-serif:lang=en' | head -n 1
  printf 'Linux_font_match_en=%s\n' "$(fc-match --format '%{family} | %{style}' 'sans-serif:lang=en')"
  printf 'Chinese_font='
  fc-match 'sans-serif:lang=zh-cn' | head -n 1
  printf 'Linux_font_match_zh=%s\n' "$(fc-match --format '%{family} | %{style}' 'sans-serif:lang=zh-cn')"
  if [[ "$font_candidate" == liberation ]]; then
    printf 'Liberation_package_version=%s\n' "$(dpkg-query -W -f='${Version}' fonts-liberation2)"
    printf 'P2C_fontconfig_sha256=%s\n' "$(sha256sum "$runtime_dir/fonts-liberation.xml" | awk '{print $1}')"
  fi
  xdpyinfo | awk '/dimensions:/{print "X11_dimensions=" $2; exit}'
  printf 'Windows source=Mem Reduct v.3.5.2 tag 2f5c55d15f69a2c88859e036b16c6f4c971e1968\n'
  printf 'Windows reference: 574x374 outer / 558x335 client settings window. Linux target: 558x335 client; Canvas fills it. Child template=260x168 DLU measured at 390x273 px on Windows Server 2022, 96 DPI.\n'
  printf 'Theme=Adwaita:light; Xft.dpi=96; X11 server screenshots include the client window, not Openbox decorations.\n'
  printf 'Canvas geometry is read from production draw markers; no Windows pixel claim.\n'
} | tee "$artifact_dir/p2c-environment.txt"
printf 'Xft.dpi: 96\nXft.antialias: 1\nXft.hinting: 1\n' | xrdb -merge
openbox --sm-disable >"$artifact_dir/p2c-openbox.log" 2>&1 &
wm_pid="$!"
for _ in {1..50}; do
  xprop -root _NET_SUPPORTING_WM_CHECK 2>/dev/null | grep -q 'window id' && break
  kill -0 "$wm_pid" 2>/dev/null || break
  sleep 0.1
done
xprop -root _NET_SUPPORTING_WM_CHECK 2>/dev/null | grep -q 'window id' || { cat "$artifact_dir/p2c-openbox.log" >&2; exit 1; }

MEMREDUCT_CI=1 MEMREDUCT_CI_EXIT_MS=600000 stdbuf -oL "$app" >"$artifact_dir/p2c-client.log" 2>&1 &
app_pid="$!"
wait_marker() {
  local marker="$1"
  for _ in {1..120}; do
    grep -Fq -- "$marker" "$artifact_dir/p2c-client.log" && return 0
    kill -0 "$app_pid" 2>/dev/null || break
    sleep 0.1
  done
  printf 'Missing production settings event: %s\n' "$marker" >&2
  cat "$artifact_dir/p2c-client.log" >&2
  return 1
}
wait_new_marker() {
  local marker="$1" count_before="$2" count
  for _ in {1..120}; do
    count="$(grep -Fc -- "$marker" "$artifact_dir/p2c-client.log" || true)"
    (( count > count_before )) && return 0
    kill -0 "$app_pid" 2>/dev/null || break
    sleep 0.1
  done
  printf 'Missing new production settings event: %s\n' "$marker" >&2
  cat "$artifact_dir/p2c-client.log" >&2
  return 1
}
wait_page_theme() {
  local marker="$1" theme="$2"
  for _ in {1..120}; do
    if grep -F -- "$marker" "$artifact_dir/p2c-client.log" | grep -Fq -- "theme=$theme"; then return 0; fi
    kill -0 "$app_pid" 2>/dev/null || break
    sleep 0.1
  done
  printf 'Missing production page/theme state: %s theme=%s\n' "$marker" "$theme" >&2
  cat "$artifact_dir/p2c-client.log" >&2
  return 1
}
window_by_title() {
  local title="$1" result=""
  for _ in {1..100}; do
    result="$(xdotool search --onlyvisible --name "^${title}$" 2>/dev/null | head -n 1 || true)"
    [[ -n "$result" ]] && { printf '%s' "$result"; return 0; }
    kill -0 "$app_pid" 2>/dev/null || break
    sleep 0.1
  done
  return 1
}
geometry() {
  local wid="$1" expected_w="$2" expected_h="$3" output="$4" got_w got_h
  xwininfo -id "$wid" >"$output"
  got_w="$(awk '$1 == "Width:" {print $2; exit}' "$output")"
  got_h="$(awk '$1 == "Height:" {print $2; exit}' "$output")"
  printf 'top-level=%sx%s\n' "$got_w" "$got_h" | tee -a "$output"
  [[ "$got_w" == "$expected_w" && "$got_h" == "$expected_h" ]] || {
    printf 'Unexpected window size; expected %sx%s, observed %sx%s.\n' "$expected_w" "$expected_h" "$got_w" "$got_h" >&2
    return 1
  }
}
capture() {
  local wid="$1" name="$2" expected="$3" theme="${4:-light}" locale="${5:-unknown}"
  sleep 0.12
  import -window "$wid" "$artifact_dir/$name"
  [[ "$(identify -format '%wx%h' "$artifact_dir/$name")" == "$expected" ]] || {
    printf 'Unexpected screenshot dimensions for %s; expected %s.\n' "$name" "$expected" >&2
    return 1
  }
  printf 'theme=%s locale=%s screenshot=%s size=%s sha256=%s\n' \
    "$theme" "$locale" "$name" "$expected" "$(sha256sum "$artifact_dir/$name" | awk '{print $1}')" \
    | tee -a "$artifact_dir/p2c-captures.log"
}
wait_popup_fits() {
  local kind="$1" menu="$2" locale="$3"
  for _ in {1..100}; do
    if grep -F "CI_POPUP kind=$kind menu=$menu locale=$locale" "$artifact_dir/p2c-client.log" | grep -Fq 'fits=true'; then return 0; fi
    kill -0 "$app_pid" 2>/dev/null || break
    sleep 0.1
  done
  printf 'Popup did not fit the production Canvas: kind=%s menu=%s locale=%s\n' "$kind" "$menu" "$locale" >&2
  cat "$artifact_dir/p2c-client.log" >&2
  return 1
}
click() { xdotool mousemove --sync --window "$1" "$2" "$3" click 1; sleep 0.12; }

wait_marker 'CI_PRODUCTION_START'
wait_marker 'CI_TEST_TIMEOUT_MS=600000'
wait_marker 'CI_GTK_FONT source=GtkLabelStyle'
wait_marker 'CI_MEMORY source=/proc/meminfo'
wait_marker 'CI_DRAW width='
window_id="$(window_by_title 'Mem Reduct')"
geometry "$window_id" 337 369 "$artifact_dir/geometry-main.txt"
capture "$window_id" 'production-settings-main-before.png' '337x369'

# Capture the real production root menus in both locales. Separators and
# language navigation are exercised through XTEST; no cleaning command runs.
capture_root_menus() {
  local locale="$1" suffix="$2" index marker before close_marker close_before
  local -a names=(file view settings help) positions=(20 60 120 178)
  for index in 0 1 2 3; do
    marker="CI_MENU root=$((index + 1)) open=true"
    before="$(grep -Fc -- "$marker" "$artifact_dir/p2c-client.log" || true)"
    click "$window_id" "${positions[$index]}" 22
    wait_new_marker "$marker" "$before"
    wait_popup_fits root "$((index + 1))" "$locale"
    capture "$window_id" "production-menu-${names[$index]}-${suffix}.png" '337x369' light "$locale"
    close_marker='CI_MENU closed reason=Escape'
    close_before="$(grep -Fc -- "$close_marker" "$artifact_dir/p2c-client.log" || true)"
    xdotool key --clearmodifiers Escape
    wait_new_marker "$close_marker" "$close_before"
  done
}
capture_root_menus en en

# Language is a real View submenu. Arrow navigation must skip the separator,
# keep the submenu inside the Canvas, and update only the session locale.
marker='CI_MENU root=2 open=true'
before="$(grep -Fc -- "$marker" "$artifact_dir/p2c-client.log" || true)"
click "$window_id" 60 22
wait_new_marker "$marker" "$before"
focus_before="$(grep -Fc -- 'CI_FOCUS item=204' "$artifact_dir/p2c-client.log" || true)"
xdotool key --clearmodifiers Down Down Down
wait_new_marker 'CI_FOCUS item=204' "$focus_before"
before="$(grep -Fc -- 'CI_LANGUAGE_MENU open=true' "$artifact_dir/p2c-client.log" || true)"
xdotool key --clearmodifiers Return
wait_new_marker 'CI_LANGUAGE_MENU open=true' "$before"
wait_popup_fits language 2 en
capture "$window_id" 'production-menu-language-en.png' '337x369' light en
xdotool key --clearmodifiers Down
wait_marker 'CI_FOCUS item=602'
before="$(grep -Fc -- 'CI_LOCALE locale=zh-CN' "$artifact_dir/p2c-client.log" || true)"
xdotool key --clearmodifiers Return
wait_new_marker 'CI_LOCALE locale=zh-CN' "$before"
sleep 0.2
capture "$window_id" 'production-main-zh.png' '337x369' light zh-CN
capture_root_menus zh-CN zh
marker='CI_MENU root=2 open=true'
before="$(grep -Fc -- "$marker" "$artifact_dir/p2c-client.log" || true)"
click "$window_id" 60 22
wait_new_marker "$marker" "$before"
focus_before="$(grep -Fc -- 'CI_FOCUS item=204' "$artifact_dir/p2c-client.log" || true)"
xdotool key --clearmodifiers Down Down Down
wait_new_marker 'CI_FOCUS item=204' "$focus_before"
before="$(grep -Fc -- 'CI_LANGUAGE_MENU open=true' "$artifact_dir/p2c-client.log" || true)"
xdotool key --clearmodifiers Return
wait_new_marker 'CI_LANGUAGE_MENU open=true' "$before"
wait_popup_fits language 2 zh-CN
capture "$window_id" 'production-menu-language-zh.png' '337x369' light zh-CN
before="$(grep -Fc -- 'CI_LOCALE locale=en' "$artifact_dir/p2c-client.log" || true)"
xdotool key --clearmodifiers Return
wait_new_marker 'CI_LOCALE locale=en' "$before"
marker='CI_CLEAN_MENU open=true'
before="$(grep -Fc -- "$marker" "$artifact_dir/p2c-client.log" || true)"
click "$window_id" 288 322
wait_new_marker "$marker" "$before"
capture "$window_id" 'production-menu-clean-dropdown-en.png' '337x369'
xdotool key --clearmodifiers Escape
wait_marker 'CI_MENU closed reason=Escape'

# Open through the actual File > Settings command and verify a separate native top-level.
menu_marker='CI_MENU root=1 open=true'
menu_before="$(grep -Fc -- "$menu_marker" "$artifact_dir/p2c-client.log" || true)"
click "$window_id" 20 22
wait_new_marker "$menu_marker" "$menu_before"
click "$window_id" 45 49
wait_marker 'CI_SETTINGS_WINDOW mode=settings visible=true native_toplevel=true'
window_id="$(window_by_title 'Settings - Mem Reduct')"
settings_window_xid="$window_id"
wait_marker 'CI_GTK_RUNTIME version=3.'
wait_marker 'CI_SETTINGS_WIDGET_TREE root_type=GtkBox'
wait_marker 'holder_candidates=1'
wait_marker 'CI_NATIVE_SETTINGS_DELETE_HANDLER signal_id='
wait_marker 'existing_handler='
wait_marker 'blocked=1 connected=1'
wait_marker 'CI_SETTINGS_CONTENT_BORDER old='
wait_marker 'CI_SETTINGS_PAGE idd=101 page=General locale=en controls=116,117,118,120,119,121,122,123 child_dlu=260x168 provisional_canvas=558.0x335.0'
geometry "$window_id" 558 335 "$artifact_dir/geometry-settings-en.txt"
capture "$window_id" 'production-settings-general-en.png' '558x335'
main_id="$(window_by_title 'Mem Reduct')"
main_window_xid="$main_id"
geometry "$main_id" 337 369 "$artifact_dir/geometry-main-while-settings-open.txt"

# Every child page is selected through the production navigation widget and captured.
page_names=("General" "Memory cleaning" "Appearance" "Tray icon" "Advanced")
for index in 0 1 2 3 4; do
  y=$((23 + 20 * index))
  click "$window_id" 60 "$y"
  page_id=$((101 + index))
  wait_marker "CI_SETTINGS_NAV resource=104 page_idd=$page_id page=${page_names[$index]}"
  wait_marker "CI_SETTINGS_PAGE idd=$page_id page=${page_names[$index]} locale=en"
  capture "$window_id" "production-settings-${index}-en.png" '558x335'
  if [[ "$index" == 1 ]]; then
    for offset in 1 2; do
      before="$(grep -Fc "CI_SETTINGS_REGION_SCROLL resource=124 offset=$offset" "$artifact_dir/p2c-client.log" || true)"
      click "$window_id" 525 136
      wait_new_marker "CI_SETTINGS_REGION_SCROLL resource=124 offset=$offset" "$before"
      if [[ "$offset" == 1 ]]; then
        capture "$window_id" 'production-settings-memory-en-scroll-1.png' '558x335'
      fi
    done
    capture "$window_id" 'production-settings-memory-en-scroll-2.png' '558x335'
    for offset in 1 0; do
      before="$(grep -Fc "CI_SETTINGS_REGION_SCROLL resource=124 offset=$offset" "$artifact_dir/p2c-client.log" || true)"
      click "$window_id" 525 42
      wait_new_marker "CI_SETTINGS_REGION_SCROLL resource=124 offset=$offset" "$before"
    done
  fi
done

# Select Simplified Chinese in the real IDC_LANGUAGE combo and capture the same five pages.
click "$window_id" 60 20
wait_marker 'CI_SETTINGS_NAV resource=104 page_idd=101 page=General'
click "$window_id" 400 190
wait_marker 'CI_SETTINGS_LANGUAGE resource=123 popup_open=true'
click "$window_id" 400 225
wait_marker 'CI_LOCALE locale=zh-CN source=IDC_LANGUAGE_123'
wait_marker 'CI_SETTINGS_PAGE idd=101 page=常规 locale=zh-CN'
nav_marker='CI_SETTINGS_NAV resource=104 page_idd=101 page=常规'
nav_before="$(grep -Fc -- "$nav_marker" "$artifact_dir/p2c-client.log" || true)"
click "$window_id" 60 20
wait_new_marker "$nav_marker" "$nav_before"
capture "$window_id" 'production-settings-0-zh.png' '558x335'
for index in 1 2 3 4; do
  y=$((23 + 20 * index))
  click "$window_id" 60 "$y"
  page_id=$((101 + index))
  page_name="${page_names[$index]}"
  case "$index" in
    0) page_name='常规' ;;
    1) page_name='内存清理' ;;
    2) page_name='外观' ;;
    3) page_name='托盘图标' ;;
    4) page_name='高级' ;;
  esac
  wait_marker "CI_SETTINGS_NAV resource=104 page_idd=$page_id page=$page_name"
  wait_marker "CI_SETTINGS_PAGE idd=$page_id page=$page_name locale=zh-CN"
  capture "$window_id" "production-settings-${index}-zh.png" '558x335'
  if [[ "$index" == 1 ]]; then
    for offset in 1 2; do
      before="$(grep -Fc "CI_SETTINGS_REGION_SCROLL resource=124 offset=$offset" "$artifact_dir/p2c-client.log" || true)"
      click "$window_id" 525 136
      wait_new_marker "CI_SETTINGS_REGION_SCROLL resource=124 offset=$offset" "$before"
      if [[ "$offset" == 1 ]]; then
        capture "$window_id" 'production-settings-memory-zh-scroll-1.png' '558x335'
      fi
    done
    capture "$window_id" 'production-settings-memory-zh-scroll-2.png' '558x335'
    for offset in 1 0; do
      before="$(grep -Fc "CI_SETTINGS_REGION_SCROLL resource=124 offset=$offset" "$artifact_dir/p2c-client.log" || true)"
      click "$window_id" 525 42
      wait_new_marker "CI_SETTINGS_REGION_SCROLL resource=124 offset=$offset" "$before"
    done
  fi
done

# Verify unsupported rows explain the boundary and have no action/state change.
click "$window_id" 60 40
wait_marker 'CI_SETTINGS_NAV resource=104 page_idd=102 page=内存清理'
click "$window_id" 210 40
wait_marker 'CI_SETTINGS_DISABLED resource=124 row=0 action=none'
click "$window_id" 210 186
wait_marker 'CI_SETTINGS_DISABLED resource=125 action=none'
if grep -Eiq 'drop_caches|pkexec|swapoff|zramctl' "$artifact_dir/p2c-client.log"; then
  printf 'Forbidden privileged operation marker found in settings UI run.\n' >&2
  exit 1
fi

# Reset confirmation is real UI feedback only; both paths guarantee no stored state/system change.
click "$window_id" 190 310
wait_marker 'CI_RESET_CONFIRMATION resource=105 resource_string=83'
capture "$window_id" 'production-settings-reset-confirmation-zh.png' '558x335'
click "$window_id" 450 201
wait_marker 'CI_RESET_CANCELLED no_system_change=true'
click "$window_id" 190 310
wait_marker 'CI_RESET_CONFIRMATION resource=105 resource_string=83'
click "$window_id" 370 201
wait_marker 'CI_RESET_CONFIRMED no_system_change=true'

# Keyboard focus, language selection, Reset cancellation, Escape return, and
# both keyboard and mouse IDC_CLOSE paths are separate production checks.
click "$window_id" 60 20
for _ in {1..5}; do xdotool key --clearmodifiers Tab; done
wait_marker 'CI_SETTINGS_KEY name=Tab page_idd=101 focus=105'
xdotool key --clearmodifiers Return
wait_marker 'CI_SETTINGS_LANGUAGE resource=123 popup_open=true source=keyboard'
xdotool key --clearmodifiers Up
wait_marker 'CI_SETTINGS_LANGUAGE_KEY name=Up focus=801'
xdotool key --clearmodifiers Return
wait_marker 'CI_SETTINGS_LANGUAGE_KEY name=Enter selected=801'
wait_marker 'CI_LOCALE locale=en source=IDC_LANGUAGE_123'
xdotool key --clearmodifiers Tab
wait_marker 'CI_SETTINGS_KEY name=Tab page_idd=101 focus=106'
xdotool key --clearmodifiers Return
wait_marker 'CI_RESET_CONFIRMATION resource=105 resource_string=83'
xdotool key --clearmodifiers Escape
wait_marker 'CI_RESET_CANCELLED no_system_change=true'
xdotool key --clearmodifiers Tab
wait_marker 'CI_SETTINGS_KEY name=Tab page_idd=101 focus=107'
xdotool key --clearmodifiers Return
wait_marker 'CI_SETTINGS_CLOSE resource=106 action=return_to_main source=keyboard'
window_id="$(window_by_title 'Mem Reduct')"
geometry "$window_id" 337 369 "$artifact_dir/geometry-main-after-keyboard-close.txt"

# Escape independently closes the settings view and returns to the main view.
xdotool key --clearmodifiers F2
wait_marker 'CI_SETTINGS_WINDOW mode=settings visible=true native_toplevel=true'
window_id="$(window_by_title 'Settings - Mem Reduct')"
xdotool key --clearmodifiers Escape
wait_marker 'CI_SETTINGS_ESCAPE action=return_to_main'
window_id="$(window_by_title 'Mem Reduct')"
geometry "$window_id" 337 369 "$artifact_dir/geometry-main-after-escape.txt"
xdotool key --clearmodifiers F2
wait_marker 'CI_SETTINGS_WINDOW mode=settings visible=true native_toplevel=true'
window_id="$(window_by_title 'Settings - Mem Reduct')"
wait_marker 'CI_SETTINGS_PAGE idd=101 page=General locale=en'
click "$window_id" 507 310
wait_marker 'CI_SETTINGS_CLOSE resource=106 action=return_to_main'
window_id="$(window_by_title 'Mem Reduct')"
geometry "$window_id" 337 369 "$artifact_dir/geometry-main-after-close.txt"

# Alt+F4 on the secondary top-level must return to the main window without
# ending the application's event loop.
xdotool key --clearmodifiers F2
window_id="$(window_by_title 'Settings - Mem Reduct')"
[[ "$window_id" == "$settings_window_xid" ]] || {
  printf 'Settings window was destroyed instead of reused after Close/Escape.\n' >&2
  exit 1
}
request_marker='CI_SETTINGS_CLOSE_REQUEST source=delete_event'
request_before="$(grep -Fc -- "$request_marker" "$artifact_dir/p2c-client.log" || true)"
native_event_marker='CI_NATIVE_SETTINGS_DELETE_EVENT'
native_event_before="$(grep -Fc -- "$native_event_marker" "$artifact_dir/p2c-client.log" || true)"
handled_marker='CI_SETTINGS_CLOSE_REQUEST_HANDLED main_visible=true'
handled_before="$(grep -Fc -- "$handled_marker" "$artifact_dir/p2c-client.log" || true)"
xdotool windowactivate --sync "$window_id"
xdotool key --clearmodifiers Alt+F4
wait_new_marker "$native_event_marker" "$native_event_before"
wait_new_marker "$request_marker" "$request_before"
wait_new_marker "$handled_marker" "$handled_before"
kill -0 "$app_pid"
window_id="$(window_by_title 'Mem Reduct')"
geometry "$window_id" 337 369 "$artifact_dir/geometry-main-after-settings-alt-f4.txt"
if xdotool search --onlyvisible --name '^Settings - Mem Reduct$' >/dev/null 2>&1; then
  printf 'Settings toplevel remained visible after Alt+F4.\n' >&2
  exit 1
fi

# Ask the isolated Openbox instance to send WM_DELETE_WINDOW by unique title,
# avoiding any conversion between xdotool and wmctrl window ID formats.
xdotool key --clearmodifiers F2
window_id="$(window_by_title 'Settings - Mem Reduct')"
[[ "$window_id" == "$settings_window_xid" ]] || {
  printf 'Settings window was destroyed instead of reused after Alt+F4.\n' >&2
  exit 1
}
settings_id="$window_id"
xprop -id "$settings_id" WM_PROTOCOLS | tee "$artifact_dir/p2c-settings-wm-protocols.txt"
grep -Fq 'WM_DELETE_WINDOW' "$artifact_dir/p2c-settings-wm-protocols.txt"
request_before="$(grep -Fc -- "$request_marker" "$artifact_dir/p2c-client.log" || true)"
native_event_before="$(grep -Fc -- "$native_event_marker" "$artifact_dir/p2c-client.log" || true)"
handled_before="$(grep -Fc -- "$handled_marker" "$artifact_dir/p2c-client.log" || true)"
printf 'requesting WM_DELETE_WINDOW by title: Settings - Mem Reduct\n' \
  | tee "$artifact_dir/p2c-wm-close-request.txt"
wmctrl -v -c 'Settings - Mem Reduct' 2>&1 \
  | tee -a "$artifact_dir/p2c-wm-close-request.txt"
wait_new_marker "$native_event_marker" "$native_event_before"
wait_new_marker "$request_marker" "$request_before"
wait_new_marker "$handled_marker" "$handled_before"
kill -0 "$app_pid"
window_id="$(window_by_title 'Mem Reduct')"
geometry "$window_id" 337 369 "$artifact_dir/geometry-main-after-wm-close.txt"
if xdotool search --onlyvisible --name '^Settings - Mem Reduct$' >/dev/null 2>&1; then
  printf 'Settings toplevel remained visible after WM close request.\n' >&2
  exit 1
fi

# A WM close request must hide, not destroy, the reusable settings top-level.
kill -0 "$app_pid"
general_page_marker='CI_SETTINGS_PAGE idd=101 page=General locale=en'
general_page_before="$(grep -Fc -- "$general_page_marker" "$artifact_dir/p2c-client.log" || true)"
xdotool key --clearmodifiers F2
window_id="$(window_by_title 'Settings - Mem Reduct')"
[[ "$window_id" == "$settings_window_xid" ]] || {
  printf 'Settings window was destroyed instead of reused after WM_DELETE_WINDOW.\n' >&2
  exit 1
}
wait_new_marker "$general_page_marker" "$general_page_before"
click "$window_id" 507 310
wait_marker 'CI_SETTINGS_CLOSE resource=106 action=return_to_main'
window_id="$(window_by_title 'Mem Reduct')"
geometry "$window_id" 337 369 "$artifact_dir/geometry-main-after-wm-close-reopen.txt"

# Toggle the application's own dark palette via its View menu with real XTEST
# keyboard events; GTK_THEME remains recorded independently as Adwaita:light.
view_marker='CI_MENU root=2 open=true'
view_before="$(grep -Fc -- "$view_marker" "$artifact_dir/p2c-client.log" || true)"
click "$window_id" 60 22
wait_new_marker "$view_marker" "$view_before"
xdotool key --clearmodifiers Down
wait_marker 'CI_FOCUS item=202'
xdotool key --clearmodifiers Return
wait_marker 'CI_THEME dark=true'
capture "$window_id" 'production-main-dark-en.png' '337x369' dark en

# Capture every settings page in both locales from the same production process.
xdotool key --clearmodifiers F2
wait_marker 'CI_SETTINGS_WINDOW mode=settings visible=true native_toplevel=true'
window_id="$(window_by_title 'Settings - Mem Reduct')"
wait_page_theme 'CI_SETTINGS_PAGE idd=101 page=General locale=en controls=' dark
page_names=("General" "Memory cleaning" "Appearance" "Tray icon" "Advanced")
for index in 0 1 2 3 4; do
  y=$((23 + 20 * index))
  click "$window_id" 60 "$y"
  wait_page_theme "CI_SETTINGS_PAGE idd=$((101 + index)) page=${page_names[$index]} locale=en controls=" dark
  capture "$window_id" "production-settings-${index}-en-dark.png" '558x335' dark en
done
click "$window_id" 60 23
focus_before="$(grep -Fc 'CI_SETTINGS_KEY name=Tab page_idd=101 focus=105' "$artifact_dir/p2c-client.log" || true)"
for _ in {1..5}; do xdotool key --clearmodifiers Tab; done
wait_new_marker 'CI_SETTINGS_KEY name=Tab page_idd=101 focus=105' "$focus_before"
capture "$window_id" 'production-settings-focus-reset-en-dark.png' '558x335' dark en
focus_before="$(grep -Fc 'CI_SETTINGS_KEY name=Tab page_idd=101 focus=106' "$artifact_dir/p2c-client.log" || true)"
xdotool key --clearmodifiers Tab
wait_new_marker 'CI_SETTINGS_KEY name=Tab page_idd=101 focus=106' "$focus_before"
capture "$window_id" 'production-settings-focus-close-en-dark.png' '558x335' dark en
click "$window_id" 60 20
click "$window_id" 400 190
wait_marker 'CI_SETTINGS_LANGUAGE resource=123 popup_open=true'
click "$window_id" 400 225
wait_marker 'CI_LOCALE locale=zh-CN source=IDC_LANGUAGE_123'
for index in 0 1 2 3 4; do
  y=$((23 + 20 * index))
  case "$index" in
    0) page_name='常规' ;;
    1) page_name='内存清理' ;;
    2) page_name='外观' ;;
    3) page_name='托盘图标' ;;
    4) page_name='高级' ;;
  esac
  click "$window_id" 60 "$y"
  wait_page_theme "CI_SETTINGS_PAGE idd=$((101 + index)) page=$page_name locale=zh-CN controls=" dark
  capture "$window_id" "production-settings-${index}-zh-dark.png" '558x335' dark zh-CN
done
click "$window_id" 60 23
focus_before="$(grep -Fc 'CI_SETTINGS_KEY name=Tab page_idd=101 focus=105' "$artifact_dir/p2c-client.log" || true)"
for _ in {1..5}; do xdotool key --clearmodifiers Tab; done
wait_new_marker 'CI_SETTINGS_KEY name=Tab page_idd=101 focus=105' "$focus_before"
capture "$window_id" 'production-settings-focus-reset-zh-dark.png' '558x335' dark zh-CN
focus_before="$(grep -Fc 'CI_SETTINGS_KEY name=Tab page_idd=101 focus=106' "$artifact_dir/p2c-client.log" || true)"
xdotool key --clearmodifiers Tab
wait_new_marker 'CI_SETTINGS_KEY name=Tab page_idd=101 focus=106' "$focus_before"
capture "$window_id" 'production-settings-focus-close-zh-dark.png' '558x335' dark zh-CN
click "$window_id" 507 310
wait_marker 'CI_SETTINGS_CLOSE resource=106 action=return_to_main'
window_id="$(window_by_title 'Mem Reduct')"

xdotool key --clearmodifiers Alt+F4
wait_marker 'CI_WINDOW_CLOSE'
for _ in {1..100}; do
  kill -0 "$app_pid" 2>/dev/null || break
  sleep 0.1
done
set +e
wait "$app_pid"
app_status="$?"
set -e
app_pid=""
printf 'production application exit status=%s\n' "$app_status" | tee "$artifact_dir/p2c-client-exit.txt"
[[ "$app_status" == 0 ]] || { cat "$artifact_dir/p2c-client.log" >&2; exit "$app_status"; }
grep -Fq 'CI_APP_RETURNED' "$artifact_dir/p2c-client.log"
! grep -Eiq 'drop_caches|pkexec|swapoff|zramctl' "$artifact_dir/p2c-client.log"
for title in 'Mem Reduct' 'Settings - Mem Reduct'; do
  if xdotool search --onlyvisible --name "^${title}$" >/dev/null 2>&1; then
    printf 'A production top-level window remained after the application exited: %s\n' "$title" >&2
    exit 1
  fi
done
for xid in "$main_window_xid" "$settings_window_xid"; do
  if xwininfo -id "$xid" >/dev/null 2>&1; then
    printf 'Production X11 window resource remained after application exit: %s\n' "$xid" >&2
    exit 1
  fi
done
printf 'PASS both production X11 windows released after app exit.\n' | tee "$artifact_dir/p2c-window-resource-release.txt"
sha256sum "$artifact_dir"/production-*.png | sed "s#$artifact_dir/##" | tee "$artifact_dir/p2c-screenshots-sha256.txt"
(
  cd "$root"
  sha256sum src/main.zm src/settings_ui.zm src/ui_geometry.zm native/gui_window.c native/gui_test.c \
    scripts/ci/p2c-settings-xvfb.sh scripts/ci/compare_p2c_visuals.py \
    scripts/ci/compare_p2c_dark.py scripts/ci/compare_font_candidates.py \
    scripts/ci/windows-native-baseline.ps1
) | tee "$artifact_dir/p2c-source-sha256.txt"
printf 'PASS: production XTEST captured File/View/Settings/Help in English and Simplified Chinese, the Language submenu in both selection states, and the safe clean dropdown; keyboard navigation skipped native menu separators and locale changed session-only; application dark palette toggled through View; 5 settings pages x 2 locales captured in light and dark states, plus Reset/Close focus screenshots in both locales; existing Windows region scroll, Reset, language, keyboard, Escape, Close, Alt+F4 and WM_DELETE_WINDOW tests passed; main remained available and both windows were released on exit.\n' \
  | tee "$artifact_dir/p2c-result.txt"
