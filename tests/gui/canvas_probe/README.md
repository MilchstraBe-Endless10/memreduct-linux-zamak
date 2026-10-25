# P2A-R4 isolated GTK Canvas test fixture

This is a non-privileged CI probe, not production Mem Reduct UI. It uses placeholder labels and has no memory-reading, cleanup, Polkit, Swap, or ZRAM code.

## Compact geometry

The probe requests a compact 336×348 logical-pixel top-level window. The official Mem Reduct v.3.5.2 `IDD_MAIN` resource declares 224×214 **Dialog Units** and `FONT 8, "Ms Shell Dlg"`. At 96 DPI, an estimated 6×13 px dialog base unit gives `224×6/4 = 336` by `214×13/8 ≈ 348`. GTK allocates a 312×324 Canvas viewport because the toolkit reserves 12 px on each edge. This top-level area is about 12% smaller than the previous 338×395 fixture. The base-unit estimate has not been measured on a real Windows installation, so it is a compact approximation, not authenticated Windows geometry or visual parity. The earlier 941×1030 result was a tiled desktop viewport, not the intended window size. Exact shipping-window dimensions still require a real Windows capture with recorded font and DPI metrics.

## What is exercised

- `zam check`, all nine pure event-routing tests, and `zam build` under pinned Zamak 0.2.0.
- Weston Headless/Pixman Wayland socket connection, GTK Canvas draw, timer-requested `ui.quit()`, event-loop return, process exit status, and protocol log checks.
- Independent Xvfb/XTEST GTK input: hover-only and held-press captures at the same pointer location, non-zero pixel deltas limited to the button ROI, release event/state, split-button and menu selection, English/简体中文 layout and restoration, Tab focus, Enter, Escape, resize geometry, F12 quit, and a separate `xdotool windowclose` run.
- Three additional geometry/locale tests verify the 336×348 top-level to 312×324 Canvas contract, the split-button and popup bounds, and the resized Canvas geometry. The original nine routing test functions remain unchanged.

The Wayland lane does not inject pointer or keyboard events. The X11 lane is reported separately and is not a Wayland or Hyprland acceptance test.

The Weston Headless client log is retained verbatim. R5 recorded one GTK `gdk_seat_get_keyboard` critical before the client created its surface; `wayland-info` did not list a `wl_seat` global, while the client subsequently drew and exited normally. This is consistent with the headless compositor lacking input, but does not prove causality and did not interrupt the event loop. Weston 13's help offers existing-seat selection and flags to continue without or disable input, but no virtual input-device creation option; removing the warning requires a separate synthetic-input provider, not a GTK warning filter. The test does not suppress GTK diagnostics or claim the lane is warning-free.

## Runner usage

The workflow runs on pushes to `test/issue-3-isolated-gui` and pull requests targeting `fix/issue-1-zamak-build`. Dependencies are installed only in GitHub-hosted Ubuntu 24.04 jobs. Each script creates a private temporary runtime directory, tracks and cleans only its own processes, and uploads sanitized probe evidence. A timeout is a failure, not a pass. Uploaded evidence has 14-day retention; source, hashes, assertions, and result summary remain in the repository.
