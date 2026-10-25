# P2A-R4 isolated GTK Canvas test fixture

This is a non-privileged CI probe, not production Mem Reduct UI. It uses placeholder labels and has no memory-reading, cleanup, Polkit, Swap, or ZRAM code.

## Compact geometry

The probe requests a compact 338×395 logical-pixel top-level window. On the Ubuntu GTK runner, the Canvas viewport is 314×371 because GTK reserves 12 px on each edge; the earlier 941×1030 result was a tiled viewport, not the intended window size. The authoritative Mem Reduct v.3.5.2 resource declares the main dialog as 224×214 **Dialog Units**, not pixels. Windows converts those units using the selected dialog font and DPI. This fixture keeps a compact, repeatable test viewport; it does not claim that 338×395 is an authenticated Windows pixel conversion or a visual-parity result. Exact shipping-window dimensions must be based on a real Windows capture with recorded font and DPI metrics.

## What is exercised

- `zam check`, all nine pure event-routing tests, and `zam build` under pinned Zamak 0.2.0.
- Weston Headless/Pixman Wayland socket connection, GTK Canvas draw, timer-requested `ui.quit()`, event-loop return, process exit status, and protocol log checks.
- Independent Xvfb/XTEST GTK input: pointer hover/press/release, split-button and menu selection, locale toggle, Tab focus, Enter, Escape, fixed client geometry, screenshot dimensions/pixel delta, and clean F12 app quit.

The Wayland lane does not inject pointer or keyboard events. The X11 lane is reported separately and is not a Wayland or Hyprland acceptance test.

## Runner usage

The workflow runs on pushes to `test/issue-3-isolated-gui` and pull requests targeting `fix/issue-1-zamak-build`. Dependencies are installed only in GitHub-hosted Ubuntu 24.04 jobs. Each script creates a private temporary runtime directory, tracks and cleans only its own processes, and uploads sanitized probe evidence. A timeout is a failure, not a pass.
