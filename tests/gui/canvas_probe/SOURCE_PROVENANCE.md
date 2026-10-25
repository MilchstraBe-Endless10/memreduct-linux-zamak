# P2A-R4 Canvas probe provenance

This is a CI-only, non-privileged fixture. It is not production application code and contains no memory-reading or cleanup implementation.

## Preserved source

- R2 event-coverage source: `canvas_probe_r2_event_coverage_20261025/src/main.zm`
- R2 source SHA-256: `37f6b5488c39caeb0c5e1d172809a0faa35340831cd6e132c83684f0c8609628`
- R4 CI fixture: `tests/gui/canvas_probe/src/main.zm`
- R4 fixture SHA-256 after the CI-only harness changes: `9f5524136179ef0d32c09ec3ca80faf1f3f2afba430bd6a8c8b9991b67641e7f`
- Built fixture executable SHA-256 on this workstation: `487951393a279535bd859edfac0157f5e5f0653cb713c76f1e82eaf187a954d2`

The nine pure routing tests below are retained verbatim from the R2 event-coverage probe:

1. `test_split_button_hit_regions`
2. `test_root_menu_rows`
3. `test_split_menu_rows`
4. `test_language_text_is_single_locale`
5. `test_mouse_release_requires_same_target`
6. `test_keyboard_focus_wraps`
7. `test_enter_routes_to_focused_action`
8. `test_logical_geometry_tracks_viewport`
9. `test_language_toggle_reaches_both_locales`

R4 adds observable CI markers for mouse/key/locale/menu state, `F12` as a test-only explicit quit event, an `app.on_close` marker, a marker after `app.run` returns, and a bounded 10-second fallback quit. The production app is not changed. The X11 lane sends F12 and requires the app event loop to return and exit 0; the Wayland smoke accepts only the timer-requested clean return, never a timeout exit. `CI_WINDOW_CLOSE` is recorded if the UI framework invokes the close callback, but acceptance does not assume `ui.quit()` is equivalent to a user window-manager close request.

`tests/gui/canvas_probe/source-sha256.txt` pins the exact R4 fixture source and is checked before every probe build. The build script writes the executable hash using a stable basename so artifacts do not expose local absolute paths.

## Evidence interpretation

The nine tests exercise event-routing helpers only. Xvfb/XTEST in the proposed workflow is the real GTK/X11 input lane. Weston Headless/Pixman proves isolated Wayland socket connection, Canvas drawing and clean window/app lifecycle only; it does not provide real pointer/keyboard input or Hyprland parity. Neither lane tests real memory metrics, memory cleanup, Polkit, Swap/ZRAM, or Windows visual parity.
