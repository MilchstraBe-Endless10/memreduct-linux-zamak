# P2B Production Main Window — P2B-2 Correction Report

Date: 2026-10-25  
Upstream UI reference: Mem Reduct `v.3.5.2`, tag commit `2f5c55d15f69a2c88859e036b16c6f4c971e1968`
PR: [#6](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/6), OPEN / Draft
Base: `docs/issue-3-p2a-ui-spec` at `d89d74fde433da1863b4de0223f974c824fb590e`

The Windows 3.5.2 source is the structural reference. This report records a Linux production candidate and its isolated-runner evidence; it does not claim Windows pixel equivalence.

## Historical pre-correction evidence

The review baseline was `c76ca82e420ea391a208c5a7afc3a3377f1006ef`. Its screenshots are retained only as the pre-fix comparison set:

| Evidence | Run / artifact | SHA-256 |
|---|---|---|
| English main window | GUI run [40200556938](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40200556938), X11 artifact `12601523788` | `86492abcf79c2f05575bd69ede9d1dd6efdc58933229eaf7b41f45fa3844507a` |
| Simplified Chinese main window | same run / artifact | `d914a1f6bab58e746caaa0c900d6d3c6f7e5f3439424e2f60ec30b93dd67e981` |
| Build workflow | run [40200556939](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40200556939) | historical build evidence |
| Weston lifecycle | artifact `12600974078` from GUI run | historical Wayland evidence |

The review confirmed that the pre-fix main window arranged the three memory objects as rows under shared horizontal column headings, and that its English language popup exceeded the 313 px Canvas width. These historical hashes are not used for the corrected-source evidence below.

## Intermediate correction snapshot

The initial P2B-2 correction commit `0a7e9a0def48a75fc1a12d42d87d64f1dba9cb48` was validated by GUI run `40206181064` and build/test run `40206180942`. That evidence is retained as an intermediate history only; the final code and screenshot evidence below supersede it.

## Latest corrected source and exact-run evidence

| Item | Value |
|---|---|
| Tested source commit / PR HEAD | `e4fccb6293c698015c8f4105947669bfdc4693d6` |
| Production UI PR run | [40208121223](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40208121223), PASS: Production UI (Xvfb/XTEST) and lifecycle (Weston Headless/Pixman) |
| X11 screenshots and logs | artifact [12604402691](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40208121223/artifacts/12604402691) |
| Wayland logs | artifact [12604102361](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40208121223/artifacts/12604102361) |
| Build/test PR run | [40208121224](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40208121224), PASS |
| `src/main.zm` SHA-256 | `8f14fe5793159646d87deb39f053c46ff90397b29866d8d98f3e33bc0f63b30c` |
| `src/ui_geometry.zm` SHA-256 | `6cbf4c666cf658f516ed9d254411983667ad0e3d98cfeece637923ce8dd4aa33` |
| `native/gui_test.c` SHA-256 | `9a299f8653f7e97c455c1c38cf138e2fd3e6c8440e6612e36ddd443c24f20c9e` |
| Built production executable SHA-256 | `33d7042aab2c84f231562a641d3ae84f43733ad02ee8d5df6417db51d285d522` |
| Built helper SHA-256 | `30cac693f610cd0bec92de495f6c6e4ab352f8a6ec43bc807d08a2d27f2acf08` |

The run metadata reports the exact tested source SHA above. The workflow artifact’s `source-and-binary-sha256.txt` records the source and executable hashes; the PNG hashes below come only from this same run’s `screenshots-sha256.txt`.

## Corrected production screenshots

All PNGs are captures of the compiled production Zamak application in the isolated Xvfb runner. Window images are 337×369 px; Canvas is 313×345 logical px. Hashes are recorded in the artifact manifest.

| Screenshot | Run state | SHA-256 |
|---|---|---|
| `production-main-en.png` | English main window | `7ff1bbf86fb388f8522b3eada4a65c82ac1d99a4884f18b7325d98ad80ccbf5f` |
| `production-main-zh.png` | 简体中文 main window | `3a531078d137d63593cf73388cf5060c8ce7ccc1023e43ca288cf2237baa6d32` |
| `production-view-menu-en.png` | English View menu | `058971a2e2a05f279369d050344a3ce10c7ccd0f8c52947a92096803ff8568db` |
| `production-language-menu-en.png` | English language popup | `b4682690b5807ef86d15dea611317734b64b062ad4af49173e91f89e08ffc5d3` |
| `production-language-menu-zh.png` | 简体中文 language popup | `6fca2c47d0e62ab2458a1fa001aafa9a2ac1317c8f4b817e6c5da46faeabc6a6` |
| `production-clean-menu-zh.png` | disabled cleanup choices | `9424e404c085d876a29780feab86042ac793fc3cfbce557c3dc7b76b01bbff98` |
| `production-sample-failure-zh.png` | injected sampling failure after prior live samples | `b1d376b5a4d5a94ac4eb2aa728c8d6d47c2b5779c98a669ce42483349b3f6898` |
| `production-memory-before-failure-zh.png` | live values immediately before injected failure | `f4a519e5cf1db36515c6dd19266b398e6ded1a528c69eb1b5301cb5f8ff28d7a` |

## P2B-2 changes

- Replaced the incorrect horizontal summary table with three independent groups: Physical memory, Pagefile / Swap, and System working set. Each group has its own vertically stacked Usage, Available, and Total available label/value pairs, with separators between groups. Simplified Chinese localizes the swap heading as `交换空间`.
- The unavailable System working set stays `—`. Its Linux compatibility explanation is shown in the status area, not as an extra metric row.
- Centralized the provisional DLU conversion and main content, row, separator, button, menu, and popup rectangles in `src/ui_geometry.zm`. Drawing and hit testing use the same popup rectangles. Language and cleanup popups are fitted to the Canvas viewport; Xvfb logs report `fits=true` for all four root menus, both locale submenus, and the cleanup menu.
- Corrected the CI locale markers and made the Xvfb script wait for fresh layout/menu events before asserting or capturing. The runner also verifies the injected memory error is exactly the expected non-privileged test failure; any other sampling error still fails the run.
- Required memory fields are parsed as required values. Missing or malformed fields, negative values, MemAvailable greater than MemTotal, and SwapFree greater than SwapTotal are rejected; real `SwapTotal=0` remains valid. A failed later sample clears the prior values, sets the unavailable status, and requests redraw. The Xvfb artifact records the injected failure, invalidation, em-dash redraw, and visible pixel difference.

## Geometry and environment limits

The upstream main resource is `IDD_MAIN=224×214 DLU`. Current conversion factors and GTK insets remain explicitly `PROVISIONAL`; they are not measured Windows font/DPI conversion and not a 2 px acceptance result. The observed X11 window is 337×369 px with a 313×345 Canvas. This is a candidate generated from the centralized DLU model, not a Windows-authenticated target. The Weston protocol reports a distinct decorated surface geometry; X11 and Wayland geometry are not treated as interchangeable.

The Xvfb lane ran on GitHub-hosted Ubuntu 24.04.5 with Xvfb 1024×768×24, GTK X11 backend, Adwaita light, `GDK_SCALE=1`, `GDK_DPI_SCALE=1`, and Noto Sans CJK SC. The Weston lane used Weston 13.0.0 Headless with Pixman, a private mode-0700 runtime directory and socket, and no `DISPLAY`. No host Hyprland IPC or user desktop was accessed.

## Control and data status

| Control/data | Current candidate status | Evidence / limit |
|---|---|---|
| Physical memory | `EQUIVALENT` | MemTotal and MemAvailable from `/proc/meminfo`; Usage = Total − Available. |
| Pagefile | `EQUIVALENT` | SwapTotal and SwapFree; Linux Swap is not definitionally identical to Windows Pagefile. |
| System working set | `UNSUPPORTED` | Three values remain em dashes; Cached/SReclaimable are not substituted. |
| File / View / Settings / Help roots | `EQUIVALENT` | XTEST opens all four in English and Chinese; popup bounds fit the Canvas. Individual unfinished commands remain disabled or show not-implemented feedback. |
| View → Language | `NATIVE` | Independent English and Simplified Chinese states; both popup states captured and logged. |
| Clean memory split button | `NOT_IMPLEMENTED` | Both primary and arrow regions stay non-operational. |
| Windows cleanup choices | `UNSUPPORTED` | Original-order choices display disabled; selection records `action=none`. No Linux operation is substituted. |
| Sample failure | `NATIVE` | Deterministic non-privileged test injection clears stale data and redraws em dashes. |

Settings pages, tray, notifications, persistence, and privileged cleanup are not implemented by this candidate. No Polkit request, `drop_caches` write, Swap/ZRAM modification, or root GUI was used. The test log checks for privileged-operation markers; that is not a system-call trace.

## Verification results

| Check | Result |
|---|---|
| `python3 scripts/verify_project.py` | PASS — 29 checks |
| `python3 -m unittest discover -s tests -v` | PASS — 3 C safety tests |
| `bash -n` for `scripts/*.sh` and `scripts/ci/*.sh` | PASS |
| `zam --version` | PASS — 0.2.0 |
| `zam check .` | PASS |
| `zam test .` | PASS — 13/13, including parser and geometry cases |
| `zam build .` | PASS |
| `bash scripts/build-arch.sh` | PASS — Zamak GUI and C helper compiled |
| Production Xvfb/XTEST | PASS — bilingual layout, menus, boundaries, disabled cleanup, injected failure redraw, keyboard and WM close; exit code 0 |
| Production Weston Headless/Pixman | PASS — connected, drew, sampled live `/proc/meminfo`, returned from `app.run`, exit code 0 |
| GitHub Actions | PASS — production UI run `40208121223`; build/test run `40208121224` on source HEAD `e4fccb6293c698015c8f4105947669bfdc4693d6` |
| `git diff --check` | PASS |
| Authentic Windows 3.5.2 screenshot / paired 2 px comparison | BLOCKED / NOT RUN — no genuine Windows captures or measured Windows font/DPI data |
| Hyprland physical input / HiDPI | NOT RUN — active user desktop remains untouched |

Weston logs retain one GTK `gdk_seat_get_keyboard` critical while no `wl_seat` is advertised, and a `BUG: finalizing a layer with views still on it` diagnostic during controlled compositor shutdown after the client exits. The relationship to the missing input seat is unproven; neither diagnostic is suppressed. The client exit was 0. No Hyprland compositor was started, queried, or restarted.

## Remaining fidelity gate

The information hierarchy now matches the source structure at code level, and Xvfb screenshots verify the production layout and interactions. Final Windows visual acceptance remains `BLOCKED / NOT RUN`: there is no authentic Windows 3.5.2 screenshot set or measured font/DPI conversion to compare against, so no claim of pixel-level or 1:1 completion is made. Review this PR as a corrected, still-provisional Linux main-window candidate.
