# P2A-R4 Canvas probe provenance

This is a CI-only, non-privileged fixture. It is not production application code and contains no memory-reading or cleanup implementation.

## Preserved source

- R2 event-coverage source: `canvas_probe_r2_event_coverage_20261025/src/main.zm`
- R2 source SHA-256: `37f6b5488c39caeb0c5e1d172809a0faa35340831cd6e132c83684f0c8609628`
- R4 CI fixture: `tests/gui/canvas_probe/src/main.zm`
- R4 fixture SHA-256 at P2A-R5 base commit `91959c05aeaa2bfb15ecea0b7d60c621f31693f2`: `16c79f49be5733562679484f8de810e3bbe4e7967e87c1f410a536794c35d029`
- Current fixture SHA-256 after GTK key-name compatibility and compact official-size estimate: `29dab87e5e3d34929b2e6d0e0f7e4ccb1ec7fa1b7fd28d3f7846716ac50976af`
- Built compact fixture executable SHA-256 on this workstation (Zamak 0.2.0, GTK 3.24.52): `f9e0f44069533fe700f08687dee457e92299ab908ae846c2c7d6b5740a2b0fa9`

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

`tests/gui/canvas_probe/source-sha256.txt` pins the current probe source and is checked before every build. Its main window requests 336×348 logical pixels, an estimate from the official 224×214 Dialog Unit resource using the 8 pt `Ms Shell Dlg` font at 96 DPI; the actual GTK Canvas viewport is separately checked at 312×324. This is not a Windows screenshot measurement or pixel-parity claim. The build script writes the executable hash using a stable basename so artifacts do not expose local absolute paths.

## Evidence interpretation

The nine tests exercise event-routing helpers only. Xvfb/XTEST in the proposed workflow is the real GTK/X11 input lane. Weston Headless/Pixman proves isolated Wayland socket connection, Canvas drawing and clean window/app lifecycle only; it does not provide real pointer/keyboard input or Hyprland parity. Neither lane tests real memory metrics, memory cleanup, Polkit, Swap/ZRAM, or Windows visual parity.
