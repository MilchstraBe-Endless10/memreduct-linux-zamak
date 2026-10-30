# P2B Production Main Window — P2B-2 Correction Report

Date: 2026-10-25<br>
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

| Control/data | Candidate status at this report's source HEAD | Evidence / limit |
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
| Authentic Windows 3.5.2 screenshot / paired 2 px comparison | At this historical source HEAD, BLOCKED / NOT RUN. A later official Windows screenshot is recorded in the 2026-10-28 addendum below; the main-window images still do not form a state- and frame-aligned 2 px pair. |
| Hyprland physical input / HiDPI | NOT RUN — active user desktop remains untouched |

Weston logs retain one GTK `gdk_seat_get_keyboard` critical while no `wl_seat` is advertised, and a `BUG: finalizing a layer with views still on it` diagnostic during controlled compositor shutdown after the client exits. The relationship to the missing input seat is unproven; neither diagnostic is suppressed. The client exit was 0. No Hyprland compositor was started, queried, or restarted.

## Remaining fidelity gate

At the historical source HEAD, Xvfb verified the production layout and interactions but no authentic Windows main-window capture was yet available. The later official screenshot and local Wayland image are documented below. They establish a visual reference, not a pixel-aligned pair or a 1:1 pass.

## 2026-10-28 initial local Hyprland correction check (historical interim values; not pushed)

The earlier tiled-window report is now confirmed by code and a bounded real-session run. Main-window drawing and hit testing had scaled every DLU coordinate by the live Canvas allocation. A Hyprland tiled allocation could therefore enlarge the UI. The local correction requests the resource-derived candidate size and sets both GTK top-levels non-resizable; a direct `GDK_BACKEND=wayland` run reported `main_found=1`, `main_resizable=0`, and a `313×345` main Canvas. A separate General-settings launch reported the settings shell at `558×335`. Both bounded runs exited 0 after six seconds and logged no GTK warning, critical, or error. These dimensions remain provisional estimates from the DLU conversion, not Windows pixel acceptance.

The same inspection found a semantic display defect: upstream `src/main.c` supplies `percent_f` for each `Usage` value, while the previous Linux candidate displayed MiB there. The local source now formats the Linux `MemTotal - MemAvailable` and `SwapTotal - SwapFree` ratios to two decimal places; zero Swap total and failed samples show an em dash. This is a Linux-derived ratio, not a claim of equivalence with Windows memory APIs. The formatter has unit coverage and the Xvfb lane now checks that the physical-memory `Usage` marker is a percentage.

This local working tree passed governance 33/33, Python/C safety 3/3, Shell syntax, Zamak 0.2.0 check, Zamak tests 32/32, Zamak build, and production GUI build. `git diff --check` passed. Local Xvfb/XTEST and Weston could not run because `Xvfb`, `openbox`, and `weston` are absent; no system packages were installed. The changes are not in the published PR HEAD and have no new GitHub CI result.

At that interim checkpoint, no window-only screenshot or physical Wayland keyboard/mouse input had been obtained. Its `313×345` Canvas size was superseded by the following R1 geometry correction.

## 2026-10-28 P2B-R1 main-window visual and Wayland follow-up (local, unpushed)

**Windows reference provenance.** Official Mem Reduct v3.5.2 tag `2f5c55d15f69a2c88859e036b16c6f4c971e1968`; the isolated Windows Server 2022 runner was at 96 DPI / 100% scaling and captured the native `Mem Reduct` HWND at `352×407`. Artifact: [Windows baseline run 40269534205](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40269534205), artifact `12627309561`, GitHub artifact digest `sha256:918f8e65bf3833c602ef685e8380b5d87e15812d472500a7131a80e96c16ccfc`. Official executable SHA-256: `dd55a81d56e8c32918e5190b2ec2a1f2a7edbbca884627439b4b619fdfb5602d`; English main PNG SHA-256: `7db5c0eb8c8d22a00103b4c17739516a077e3c2d6e7439ab9715bd9820738780`.

**Linux capture.** On the real Omarchy/Hyprland Wayland session, GTK 3.24.52 with Zamak 0.2.0 launched as the invoking user using `GDK_BACKEND=wayland`. The application-owned GTK capture reports a fixed `337×369` top-level allocation, `325×357` Canvas, and a `558×335` settings window. The run exited 0. The capture does not include compositor decorations, so it must not be compared as if it were the complete Windows HWND image. Adwaita Sans resolved from the GTK label style; the Canvas requests 8 pt, Pango ascent/descent measured `11/3 px`. No user theme or Hyprland setting was changed.

| Evidence | SHA-256 | State |
|---|---|---|
| Linux English main capture | `0d47ec6e711f0840f890a5ae87805e733a6201dc09a502cc0659f83b5bee94c7` | All groups expanded; live Linux `/proc/meminfo`; safety-disabled clean button |
| Linux English General settings capture | `ee33df415eacd6e69d448cc269b44be7fd17092ee436bab616e0d494798e1551` | General page; app language English; light application theme |
| Linux runtime log | `17bff1e58ec360dd4bac2afd0f09e87bd6b9537bbc181b73e64fbb96f0a20b3e` | R1 launch with settings visible; both app-owned captures succeeded; no GTK warning/critical; exit 0 |

R1 source and executable hashes: `src/main.zm` `3d165806c29779e258c725275e12d2a4b593db698343af5de68f9d6553712488`; `src/ui_geometry.zm` `d678628e68ac344154730df93c58cd81e4ce15ea65e5fd6051e8a5d112ab5f49`; `native/gui_test.c` `fff6ea85f1288c32c999f2774043a952a380db47bfe8445b5c7e1b472b3e2d66`; production executable `b8951ec7b9b97d26835f629ee2a2cba8a22f980143136b665b0f80eab3fbeeea`. These are local worktree artifacts, not a pushed PR commit.

The R1 main-window source adds the original-style per-group disclosure arrows and click-driven collapse/expand state, compacts later groups when a group is collapsed, and extends the isolated Xvfb script to click and restore a group. It also aligns the group divider's left edge with the Windows reference. The previous `gtk_widget_draw` critical came from the test capture trying to draw a hidden, unallocated settings window; capture now skips a window unless GTK reports it visible and mapped. Repeated real Wayland main-only and settings-visible capture launches both exited 0 without GTK warning/critical output; the hidden settings window is explicitly logged as skipped.

The Windows image shows the third group highlighted and populated from Windows APIs, while the Linux image is the default unselected state and correctly shows unsupported System working set as em dashes. Dynamic memory values, group-selection state, native decorations, Win32/GDI versus GTK/Pango rasterization, and the Linux safety state differ. Therefore there is no honest full-page pixel-difference percentage for this pair and no 1:1 pass. Geometry/labels are closer, but exact Windows font/control raster parity remains unverified.

Local verification after the R1 source changes: governance `33/33`, Python/C safety `3/3`, Zamak 0.2.0 check PASS, Zamak tests `37/37`, production GUI build PASS, and shell syntax PASS. The independent Canvas probe's Zamak suite is `12/12`; its Xvfb input lane and production Xvfb/XTEST lane are not runnable locally because Xvfb/Openbox are absent. Weston is also absent. The updated Xvfb assertions have not run on GitHub because this local source has not been pushed; there is no new CI result for it.

**Still NOT RUN / NOT PASS:** physical Wayland mouse/keyboard input, collapsed-group interaction on a compositor, matched Windows/Linux main-window state comparison, whole-page 1:1 pixels, HiDPI, and Windows-native versus GTK raster parity. Settings-page `184/184` geometry results do not certify the main window. Real memory cleaning and privileged operations remain out of scope.

## 2026-10-28 R2/R3 real Wayland size and rendering follow-up (local, unpushed)

**Startup result.** The Zamak 0.2.0 / GTK 3.24.52 production executable starts under `GDK_BACKEND=wayland`, opens the main and General settings windows, captures both application-owned client surfaces, and exits 0 through the bounded test timer. Final R4 source/binary are local only: `src/main.zm` SHA-256 `9eb251c6335b63f7f1eb5267ff0b718b6e0b4d79518c1f22f70b486cea40201e`, `native/gui_test.c` SHA-256 `5a2bac54252a594c338954574faee6066a4d9dc5234099dc5c8f1efbccb9ef127` (recorded exactly in the build manifest), production executable SHA-256 `466459f85f341d41d1b85207031e38bab9c0ee0ca04772eb2309ed4a0a596bf7`. The final bounded run logged main client `337×369`, Canvas `325×357`, settings client `558×335`, GTK `scale_factor=1`, successful captures, timer quit and `CI_APP_RETURNED`; no `Gtk-WARNING` or `Gtk-CRITICAL` appeared. Final screenshot hashes: main `68c9f133bc73a38dabd485a3bfb299d45f9cf21e281934e6264e6e55d910241f`; General settings `ee33df415eacd6e69d448cc269b44be7fd17092ee436bab616e0d494798e1551`; runtime log `aad95dd5179e0871e91f8079fe658077bd5f1d4bc9ebd330d5f556fca4a2a570`.

**Size investigation and correction of an earlier inference.** The shell environment contains `GDK_SCALE=2`, but GTK reports a widget scale factor of 1. Separate bounded Wayland runs with `GDK_SCALE=1` and `GDK_SCALE=2` both report factor 1 and the same client sizes (`337×369` and `558×335`). Therefore `GDK_SCALE` did not explain the perceived large window in this run. I retract the earlier inference that it doubled the window's physical dimensions. The app-owned Wayland capture does not expose the compositor's full decorated frame; no desktop-wide screenshot or Hyprland IPC was used, so the complete outer on-screen size remains unmeasured. The resource-derived fixed client sizes were not arbitrarily reduced.

**Visual comparison.** The Windows reference is the official v3.5.2 HWND screenshot at 96 DPI / 100%, `352×407`, SHA-256 `7db5c0eb8c8d22a00103b4c17739516a077e3c2d6e7439ab9715bd9820738780`. The Linux capture is the client surface only. A natural-size side-by-side is stored in the `P2B-Real-Wayland-20261028-R4` research artifact as `main-side-by-side-not-pixel-aligned-v2.png`, SHA-256 `c2532ecab0fca3c78037402ade102403e16dd78cd5a941d61d65dd819f366e87`. It is illustrative only: the capture boundaries are different, Linux live memory values differ, Windows shows its System working set data while Linux correctly shows em dashes, the Windows screenshot has that group highlighted, and Clean memory is enabled on Windows but safely disabled on Linux. No whole-page changed-pixel rate is reported for this semantically and geometrically mismatched pair.

One code-level drawing mismatch was found and corrected: `draw_menu_shortcut` measured shortcut width at 10 pt but rendered at 8 pt, shifting the shortcut's left edge. Both now use 8 pt. This final code compiles and passes the project suite; its menu-open pixel state was not captured locally because Xvfb/Openbox are unavailable.

**Verification and limits.** On the final local source, `scripts/ci/build-production-gui.sh` passed governance 33/33, C/Python safety 3/3, Zamak 0.2.0 `check`, `test` 37/37, `build`, GTK production link, and C helper build. The independent Canvas probe passed `12/12` on the same source tree; all shell scripts passed `bash -n`; `git diff --check` passed before the documentation addendum. `Xvfb`, Openbox and Weston are not installed locally; the updated XTEST and Weston lifecycle workflows were not run on this unpushed source. No new GitHub Actions run exists for these local commits. Real Wayland physical mouse/keyboard input, opening and switching all five pages by user input, WM_CLOSE/Alt+F4 on this live run, HiDPI matching, and whole-window 1:1 remain NOT RUN / NOT PASS. No memory cleaning, Polkit, Swap/ZRAM, root GUI, or non-test desktop input was performed.

## 2026-10-28 R6 main-window focus and matched-size comparison (local, unpushed)

**Actual visual defect and fix.** The first same-size pair showed an outlined `File` menu item on Linux even though the application had just opened and no keyboard-navigation action occurred. `src/main.zm` initialized the logical focus index to `0`, and the draw routine therefore painted a keyboard focus rectangle around `File`. The initial focus is now `-1` (none); Tab advances to `File`, the arrow/Enter path selects `File` when starting with no logical focus, and keyboard focus is still painted after navigation. A new Zamak regression test checks the hidden initial state and first Tab/wrap behavior. This removes a spurious focus state without suppressing real keyboard feedback.

**Paired visual evidence.** The source is the client crop from the verified official Windows Mem Reduct v3.5.2 capture: source tag `2f5c55d15f69a2c88859e036b16c6f4c971e1968`, Windows run [40269534205](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40269534205), artifact `12627309561`; the verified executable SHA-256 is `dd55a81d56e8c32918e5190b2ec2a1f2a7edbbca884627439b4b619fdfb5602d`. The Windows crop is `337×369`, SHA-256 `d61e4c22d72c7fe647a5ed347873370140db1fd36256524ce600b0c95bd37857`. The revised Linux screenshot is also `337×369`, English, light theme, third group selected, SHA-256 `8536d5de806916afeb11310b65572eb363fea11aa712f287804d59200e5fa9fc`; it came from a bounded ordinary-user run of the built production binary under `GDK_BACKEND=wayland` on the real Wayland session. GTK reported allocated/client dimensions `337×369`, scale factor `1`, and a `325×357` Canvas. Although GTK's frame-extents API also returned `337×369`, the app-owned capture does not include the compositor's visible decoration, so this is not a measured full outer size. It exited `0` after the six-second test timer, and the log contains no `Gtk-WARNING` or `Gtk-CRITICAL`.

The earlier Linux image with the erroneous initial outline is retained unchanged at SHA-256 `69a6d35a4756ddf94de6ab39c54233a885ef326ef4674c8c7a47b6c0d9c5e0f4`. In the exact-size pre/post comparison, 244 pixels in the 24-pixel menu band changed; body pixels also changed because `/proc/meminfo` was sampled live between the two captures. The combined comparison artifacts and exact RGB metrics are stored in the R6 evidence directory listed in `memory/SHORT_TERM.md`.

**What the pixel figure means.** The equal-size, same-language, same-theme and same-selected-group pair has 26,044 changed pixels out of 124,353 (`20.943604%`) with no resize or alignment transform. The Windows capture's File hover/focus state was not recorded, so the pair is not fully state-matched. This is a raw reference figure, not a valid 1:1 acceptance result: the memory readings are live and different, Windows supplies a System working set that Linux does not implement (Linux correctly shows em dashes), the Windows clean button is enabled while Linux must keep it disabled, and Win32/GDI versus GTK/Pango rasterization differs. The measured Linux text style is Adwaita Sans, Canvas 8 pt, Pango ascent/descent `11/3 px`; the Windows baseline uses the logical `MS Shell Dlg 2` font at `8.25 pt`, GDI ascent/descent `11/2 px`. No font substitution was introduced in this follow-up. The app-owned capture does not include the compositor's visible title bar/frame, so this does not measure the full on-screen decorated window.

**Current candidate verification.** The local production build script passed governance `33/33`, C/Python safety `3/3`, Shell syntax `4/4`, Zamak 0.2.0 `check`, `test` `41/41`, `build`, GTK production GUI link, and C helper build. `scripts/ci/build-canvas-probe.sh` passed its preserved independent Canvas probe `12/12`, check, and build. The real Wayland bounded capture exited `0` and was visually reviewed. Local `Xvfb`, `xvfb-run`, Openbox, and Weston are unavailable, so the production XTEST, five-page GUI automation, and isolated Weston lifecycle tests did not run on this unpushed candidate; the previously green GitHub runs are for published HEAD `019a7f1e0755570c2c3a0b1202a6c764540f14e2` and do not validate these local changes. Physical Wayland keyboard/mouse navigation and close/reopen remain NOT RUN. Whole-page 1:1 remains NOT PASS. No real memory cleaning, privilege operation, or unrelated desktop input was performed.

## 2026-10-29 bounded real Wayland startup for local P2C candidate (unpushed)

**Exact source and launch.** Local code commit `640fb5df044475df827308388c3da5be25d4ffad` was built as the ordinary-user production executable; source SHA-256 `src/main.zm` `eb52cebf384df8e3dec1a2670fd37952d4cd18e941c51e934e76f19a4dc69c06`, `tests/test_native.py` `51f3e3119ab2ea8602d517e02db3f5b0695c75ee297e216fe40c4d5626270092`, executable `fe5c609e2718a2f348c7724ff44ccee43349488dcf9780afd1762075d9b08658`. The launch used `GDK_BACKEND=wayland`, `MEMREDUCT_CI=1`, `MEMREDUCT_CI_EXIT_MS=7000`, `MEMREDUCT_CI_CAPTURE=1`, and `MEMREDUCT_CI_START_SETTINGS=1`. The session environment had Wayland and Hyprland markers. No Hyprland IPC or keyboard/mouse injection was used.

**Observed result.** GTK 3.24.52 launched the main window and English/Light General settings page, sampled `/proc/meminfo`, captured both application-owned surfaces, emitted `CI_QUIT source=bounded_test_timer` / `CI_APP_RETURNED`, and exited with status 0 after 7 seconds. No Gtk-WARNING, Gtk-CRITICAL, or Gtk-ERROR appeared. GTK reported scale factor 1, Adwaita Sans label style at 11 pt, Canvas text request 8 pt and ascent/descent `11/3 px`. Main allocation/client was `337×369`; settings allocation/client was `558×335`. GDK frame extents were equal to the app client dimensions, so the app-owned capture still does not measure the visible Hyprland decoration.

| Capture | Size | SHA-256 |
|---|---:|---|
| `hyprland-main-en-light.png` | `337×369` | `fe90da01aac4cfa96861d8db42bdbbe0127087b694071f893ec9d7b21f0e0f3a` |
| `hyprland-settings-general-en-light.png` | `558×335` | `ee33df415eacd6e69d448cc269b44be7fd17092ee436bab616e0d494798e1551` |

The captures and runtime log are stored in the R7 `verification-640fb5df/real-wayland` research artifact. This is a bounded launch/draw/sample/normal-exit result only. It does not validate page switching, locale/theme switching, reset, Alt+F4, repeated reopen, or physical pointer/keyboard behavior. No whole-window Windows/Linux pixel rate is claimed; the 1:1 target remains NOT PASS. No helper, Polkit, cache-clean operation, Swap/ZRAM, or root GUI was run.
