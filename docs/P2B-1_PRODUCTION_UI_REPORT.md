# P2B-1 Production Main Window Candidate

Date: 2026-10-25  
Upstream reference: Mem Reduct `v.3.5.2`, tag commit `2f5c55d15f69a2c88859e036b16c6f4c971e1968`  
PR: [#6](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/6), Draft, base `docs/issue-3-p2a-ui-spec`  
Production-code test head: `a6df90389a69fc9cad12272c2daa2f273c834385`

This is the first compiled Zamak production-window candidate. Its compact DLU conversion is provisional. It is not Windows pixel calibration or 1:1 visual acceptance.

## Candidate geometry and test environment

- Source resource: `IDD_MAIN=224×214 DLU`; conversion factors are centralized in `src/ui_geometry.zm` (5.6 logical px per horizontal DLU and 12 per vertical DLU at the 96-DPI candidate setting).
- GitHub-hosted runner: Ubuntu 24.04.5, Xvfb 1024×768×24, GTK 3.24.41, Adwaita light, `GDK_SCALE=1`, `GDK_DPI_SCALE=1`, Xft DPI 96, Noto Sans CJK SC.
- X11 run observed a 337×369 top-level client window and a 313×345 Canvas. This conversion is a temporary engineering estimate, not a measured Windows `Ms Shell Dlg` mapping.
- Weston run: 13.0.0, Headless backend, Pixman renderer, 800×600 output at scale 1, private socket, temporary `XDG_RUNTIME_DIR` mode 0700, `DISPLAY` unset.
- In the Wayland protocol log, GTK reports an xdg surface geometry of 337×406 including its Wayland-side window decorations. That lane verifies lifecycle only; it does not certify cross-backend pixel geometry.

## Control status

Status meanings are those in `UI_INTERACTION_MATRIX.md`: `NATIVE`, `EQUIVALENT`, `UNSUPPORTED`, and `NOT_IMPLEMENTED`.

| Windows 3.5.2 control/resource | Production candidate behavior | Status | Evidence and remaining gap |
|---|---|---|---|
| `IDD_MAIN=100`, `IDC_LISTVIEW=102` — main statistics area | Canvas rows preserve the three-group order and Usage / Available / Total available columns. | `EQUIVALENT` | Geometry verified in isolated Xvfb; Windows font/DPI measurement is still missing. |
| Physical memory row | Reads `MemTotal` and `MemAvailable` from `/proc/meminfo`; displays used as total minus available. | `EQUIVALENT` | CI log includes source and raw KiB values; refresh is timer-driven every 3 seconds. |
| Pagefile row | Reads `SwapTotal` and `SwapFree`; English label is `Pagefile / Swap`. | `EQUIVALENT` | Swap is not Windows Pagefile. No `swapoff` or ZRAM write is called. |
| System working set row | Displays em dashes and an explanation that Linux has no same-definition system-wide metric. | `UNSUPPORTED` | Does not substitute Cached or SReclaimable for Windows working set. |
| `IDM_MAIN=100`, File / View / Settings / Help menu bar | Canvas-drawn menu roots respond to XTEST pointer and keyboard events. | `EQUIVALENT` | X11 root menu, language submenu, keyboard focus, arrows, Enter and Escape were exercised. No Windows pixel comparison. |
| File → `IDM_SETTINGS=155` | Shows a clear not-implemented status. | `NOT_IMPLEMENTED` | Settings pages are not part of this PR. |
| File → `IDM_EXIT=156` | Exits the application event loop. | `NATIVE` | X11 close lifecycle separately verified with Alt+F4. |
| View → `IDM_ALWAYSONTOP_CHK=157` | Disabled with an explanatory state. | `NOT_IMPLEMENTED` | Wayland compositor behavior is not asserted. |
| View → `IDM_USEDARKTHEME=159` | Toggles the Canvas light/dark palette. | `NATIVE` | Keyboard Enter path and state marker exercised; visual calibration remains pending. |
| View → `IDM_REDUCTCONFIRMATION_CHK=161` | Disabled with an explanatory state. | `NOT_IMPLEMENTED` | No cleaning action is enabled in this candidate. |
| View → `IDX_LANGUAGE=1000` | Switches between English and Simplified Chinese; does not concatenate locales. | `NATIVE` | XTEST selected both locale states; screenshot pair differs by 7,420 pixels. |
| Settings → startup/minimize/update commands (`IDM_LOADONSTARTUP_CHK=158`, `IDM_STARTMINIMIZED_CHK=160`, `IDM_CHECKUPDATES_CHK=163`) | Disabled with not-implemented feedback. | `NOT_IMPLEMENTED` | No autostart, update network request, or service is configured. |
| Help → Website / update / About (`IDM_WEBSITE=164`, `IDM_CHECKUPDATES=165`, `IDM_ABOUT=166`) | Disabled with not-implemented feedback. | `NOT_IMPLEMENTED` | No browser launch or update check is triggered. |
| `IDC_CLEAN=103` — split button primary area | Disabled; click reports that cleaning remains closed pending safety review. | `NOT_IMPLEMENTED` | XTEST click confirmed `action=none`. No Polkit, helper execution, `drop_caches`, Swap or ZRAM operation. |
| `IDC_CLEAN=103` — split button arrow and eight Windows cleanup choices | Arrow opens the original-order choice list; every entry is disabled and explains that no approved Linux equivalent exists. | `UNSUPPORTED` | XTEST opened the list and selected one disabled entry; event log records `action=none`. |
| `IDA_MAIN=1` keyboard behavior | Tab focus, arrow navigation, Enter, Escape, F1/F2 status feedback; normal window close. | `EQUIVALENT` | XTEST exercised menu navigation and close. Global Wayland shortcuts and mnemonic keys are not claimed. |

Settings pages, tray, notifications, update flow, persistence, and cleanup remain outside this first main-window candidate. The complete remaining source mapping is in `UI_INTERACTION_MATRIX.md`.

## Production screenshots

These PNGs are actual screenshots of the compiled production Zamak application in isolated GitHub-hosted Xvfb, not screenshots of the independent R5/R6 probe. Capture resolution is 337×369 for the window image; runner scale is 1. SHA-256 values are from the uploaded artifact manifest.

| Screenshot | State | SHA-256 |
|---|---|---|
| `production-main-en.png` | Main window, English | `880d051b960c29ce041ef48227af517d466e616e3399a0edf17747e2408e881b` |
| `production-main-zh.png` | Main window, 简体中文 | `f2c6ffa75ba41c931086c04d37440e3864ed2add7137238fd0a681602103076f` |
| `production-view-menu-en.png` | View menu open, English | `8fa71b6eb1885c69c9c9e911b28aa9385ba47837fc257af2d498649b5662c10f` |
| `production-language-menu-en.png` | Language submenu open | `e3061eb952dc5a3670460f0da70ddc3f45fc03c8e2f9e58143fece38e81fd082` |
| `production-clean-menu-zh.png` | Cleanup choices open, Simplified Chinese | `8f7331e3fcac04ddbcb91d784fd962430d593c5365bb79b2a3a0b181a3e1b5d0` |

Artifacts and test logs:

- X11 screenshots/logs: [PR run 40199653180, artifact 12601156413](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40199653180/artifacts/12601156413)
- Weston/client logs: [PR run 40199653180, artifact 12601781592](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40199653180/artifacts/12601781592)
- Production GUI workflow: [PR run 40199653180](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40199653180)
- Build/test workflow: [PR run 40199653440](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40199653440)

The build artifact records production binary SHA-256 `1a5f94923b81be4aaa0ab3012f0f646b78db05047727ae8207afeceb20685efb` and `src/main.zm` SHA-256 `33afa462553a022a525c55615a0ed48962002ccf5f42c8d49091af3c5df71bc6`.

## Verification results

| Check | Result | Evidence |
|---|---|---|
| `python3 scripts/verify_project.py` | PASS | 29 project/governance checks. |
| `python3 -m unittest discover -s tests -v` | PASS | 3 C safety tests; no real kernel write. |
| Shell syntax | PASS | `scripts/*.sh` and `scripts/ci/*.sh`. |
| Zamak | PASS | Version 0.2.0; `zam check .`; `zam test .` 7/7; `zam build .`. |
| `bash scripts/build-arch.sh` | PASS | Zamak application and C helper compiled. |
| Production UI X11 | PASS | XTEST mouse, split regions, disabled item, English/zh-CN, keyboard navigation, menu close, `WM_DELETE_WINDOW`, app exit 0 and window gone. |
| Production UI Wayland | PASS with diagnostics | Client connected to private Weston, drew, read live meminfo, returned from `app.run`, exit 0. Headless lane does not test input. |
| Privileged action guard | PASS | Test log contains no privileged action; source keeps cleaning disabled. |
| Windows native screenshot / 2 px comparison | BLOCKED / NOT RUN | No authenticated Windows v3.5.2 capture or paired overlay. |
| Hyprland physical input / HiDPI | NOT RUN | Active user compositor was not accessed; only scale 1 in CI. |

The Weston log contains one GTK `gdk_seat_get_keyboard` critical while `wl_seat` is not advertised, and logs `BUG: finalizing a layer with views still on it` when the isolated compositor receives SIGTERM during cleanup. The client exits 0 before compositor cleanup. The correlation with missing input-seat support is plausible but unproven; neither diagnostic is suppressed. No Hyprland crash or active compositor was involved.

PR #4's independent 12/12 probe tests remain on PR #4 and were not copied or replaced here. The production candidate has its own 7 Zamak tests and production X11/Wayland CI paths.

## Pixel acceptance and next review

The candidate follows the compact 224×214 DLU source template through centralized estimated conversion. There are no genuine Windows 3.5.2 screenshots or measured Windows font/DPI data in this evidence set. The 2 px geometry gate, paired image overlay/diff, Windows visual match, Hyprland input, HiDPI, full settings, and tray remain `BLOCKED` or `NOT RUN`. Review this PR as a first production-window candidate only; do not treat the CI screenshots as Windows goldens.
