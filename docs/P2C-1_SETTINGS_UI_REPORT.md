# P2C-1 — Five-page settings UI and Windows baseline

## P2C-2 latest exact-head navigation calibration (`20c8fbc8fc80c481965fecbde17aa507eb7247fe`)

PR #7 remains OPEN / Draft, based on `feat/issue-3-p2b-main-window` at `911a3d0b99b05f15bd0109dca475b0b5174e7888`. This exact-head update calibrates the production settings navigation against the official Windows Mem Reduct `v.3.5.2` tag `2f5c55d15f69a2c88859e036b16c6f4c971e1968`.

The production Canvas now draws the native selected TreeView row in `#D9D9D9`, to the measured label width for each English and Simplified Chinese item, without the previous full-row focus outline. The dotted branch guide uses the sampled native `#6D6D6D` pixels. `src/ui_geometry.zm` centralizes the measured selection rectangles and has a bilingual geometry test; `scripts/ci/compare_p2c_visuals.py` compares Windows and Linux selection bounds and checks dotted guide pixels independently of broader control geometry and text raster comparison.

The current head retains the earlier P2C-2 corrections: no added full-page frame (resource-defined GroupBoxes remain), source-default Appearance swatches (`#FFFFFF`, `#008040`, `#FF8040`, `#EC1C24`) are read-only, checked defaults are drawn but remain disabled, and the non-original permanent footer is absent. The limitation remains available through disabled-control feedback rather than an extra permanent layout row.

| Evidence | Run / artifact | Result |
|---|---|---|
| Windows 3.5.2 native captures and Win32 geometry | [run 40294897392](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40294897392), artifact [12636924611](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40294897392/artifacts/12636924611) | PASS; Windows Server 2022 interactive runner, original release, 96 DPI / 100%, light theme, 8 pt `Ms Shell Dlg`; settings window 574×374 outer and 558×335 client |
| Production settings Xvfb/XTEST and pixel comparison | [run 40294897392](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40294897392), Linux artifact [12638110426](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40294897392/artifacts/12638110426), comparison artifact [12637747957](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40294897392/artifacts/12637747957) | PASS; five settings pages in both locales; comparison images, heatmaps, metrics, Win32 geometry, navigation geometry, and PNG SHA-256 manifest generated |
| Production Xvfb/XTEST plus private Weston/Pixman lifecycle | [run 40294897349](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40294897349), X11 artifact [12638002197](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40294897349/artifacts/12638002197), Weston artifact [12637667843](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40294897349/artifacts/12637667843) | PASS; isolated runner GUI and private compositor only; this is not Hyprland or physical Wayland input acceptance |
| Zamak / project PR build-and-test | [PR run 40294897341](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40294897341), matching push run [40294892563](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40294892563) | PASS on both events; all jobs completed successfully at this exact source HEAD |

Both sides used the same 558×335 client area at 96 DPI / scale 1. Windows source images were cropped only at measured client origin `(8,31)`; no source or Linux screenshot was resized, stretched, shifted, or resampled. Runner metadata and raw/derived PNG SHA-256 entries are retained in the linked artifacts, including `windows-environment.txt`, `p2c-environment.txt`, and `visual-sha256.csv`. Linux production executable SHA-256: `882bc18ca76c3b494802e4a1614f0558e4b4c8a41b87dc88c3eff1c3f67a7b61`; production source commit is the run head above.

| Comparison | Result |
|---|---|
| Resource-ID keyed control geometry | 172 PASS within ≤2 px; 12 `NOT_COMPARABLE_NATIVE_STATE` for the intermediate Windows-uncollected memory-list scroll state; 0 FAIL |
| Page / control raster samples | 12 PASS, 0 FAIL |
| `IDC_NAV=104` selected rows | 10/10 PASS, all `dx/dy/dw/dh = 0` for five pages × two locales |
| Dotted navigation guide | 102/102 exact native `#6D6D6D` pixel samples PASS |
| Full-page changed pixels | 6.8528%–16.8042%; comparison only, not pixel equivalence |

Text baselines, font metrics, antialiasing, and remaining native-control raster differences are still visible in the overlays. Windows uses `Ms Shell Dlg` at 8 pt; Linux uses DejaVu Sans and Noto Sans CJK SC under GTK 3.24.41. Geometry passes do not convert these raster differences into a 1:1 result. Dark-theme native references and uncollected intermediate Windows scroll-state evidence remain missing; pixel-equivalence remains NOT PASS.

The production settings Xvfb log records GTK 3.24.41, `delete-event` signal id 66, only the single pre-existing unblocked handler id 70 blocked before installing the targeted handler, and one verified direct `GtkBox` holder. XTEST exercised repeated settings show/close, delete-event handling, return to the visible main window, and final application exit 0. The matching private Weston/Pixman client also exited 0; its retained log still contains one `gdk_seat_get_keyboard` critical and a Weston `finalizing a layer with views still on it` message during SIGTERM shutdown. These diagnostics are not suppressed, and the available evidence does not establish a causal link or compositor crash.

The production CI is isolated from the user's desktop. No Hyprland IPC, local GUI launch, privileged memory action, Polkit authorization, or Swap/ZRAM mutation was used for this calibration.

## P2C-2 previous exact-head calibration (`06be74b740e8906e64d837d8ac77e0255bea1081`)

The source correction was captured by isolated GitHub-hosted runners. The Windows reference is the official Mem Reduct `v.3.5.2` release, tag SHA `2f5c55d15f69a2c88859e036b16c6f4c971e1968`; no image was scaled, stretched, shifted, or resampled. Windows capture: Server 2022, 1024×768, 96 DPI / 100%, light theme, 8 pt `Ms Shell Dlg`. Linux capture: Ubuntu 24.04, GTK 3.24.41, Xvfb 1024×768×24, scale 1 / 96 DPI, Adwaita light, DejaVu Sans and Noto Sans CJK SC. The measured Windows client crop starts at `(8,31)` and is 558×335; Linux Canvas is 558×335. This is the earlier GroupBox calibration checkpoint; the later navigation calibration above supersedes it for current-head evidence.

| Evidence | Run / artifact | Result |
|---|---|---|
| Windows native source verification and captures | [run 40290615170](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40290615170), artifact [12636291172](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40290615170/artifacts/12636291172) | PASS; official 3.5.2 binary provenance verified; main and five settings pages captured in English and Simplified Chinese |
| Production settings Xvfb / XTEST | [run 40290615170](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40290615170), artifact [12635727110](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40290615170/artifacts/12635727110) | PASS; production executable, both locales, real isolated X11 mouse/keyboard events and clean exit 0 |
| Windows/Linux comparison | [run 40290615170](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40290615170), artifact [12635877774](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40290615170/artifacts/12635877774) | PASS; overlays, differences, metrics, SHA manifest and resource-ID geometry table generated |
| Production Wayland lifecycle, same source SHA | [push run 40290609997](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40290609997), artifact [12635732860](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40290609997/artifacts/12635732860) | PASS; private Weston Headless/Pixman main and settings clients connected and exited 0; no Wayland input claim |
| PR event production UI run | [run 40290615331](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40290615331), artifact [12636587415](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40290615331/artifacts/12636587415) | Xvfb/XTEST PASS; the matching push run's Weston job passed. The PR-event Weston install step was still in progress when this report snapshot was prepared. |
| Zamak/project CI | PR run [40290615246](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40290615246) and push run [40290610030](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40290610030) | PASS |

The tested code source commit is `06be74b740e8906e64d837d8ac77e0255bea1081`; `src/settings_ui.zm` SHA-256 is `e5fcaaf1388fb14103b694fc5540c799b095cff1602197dd67fd45e0ba223385`, `native/gui_window.c` is `63c313bed8205e14266148b262064900dacf838e03c1aa0134dc2facff6f6326`, and the production executable is `ac4c1a82ffd9605dfcb9f7072c3efb938a4dafb437b1f1d4fb1b8118bdbed399` (all from X11 artifact `12635727110`).

The compare job reported 184 geometry rows: 172 PASS within the 2 px bounds, 0 FAIL, and 12 `NOT_COMPARABLE_NATIVE_STATE` rows for the intermediate memory-list scroll position not captured on Windows. All 12 sampled control/page raster checks and all 8 source-color swatch samples passed. This does not mean every Win32 control is pixel-identical: changed-pixel rates remain 7.3958%–17.6612%, chiefly text/font rasterization and native-control rendering. Pixel-equivalence verdict remains NOT PASS.

The only code change since the previous comparison was the GroupBox edge stroke in `src/settings_ui.zm`. At client `(156,20)`, the first run sampled Linux `#E5E5E5` against Windows `#DCDCDC`: the 1 px Cairo stroke path was centered on an integer coordinate and antialiased against the `#F0F0F0` page. The follow-up moves the path to half-pixel coordinates and contracts the rectangle by 1 px; the fresh production captures now sample `#DCDCDC` on both locales. The dark-theme edge remains provisional because no authentic dark Windows screenshot is available.

Raw PNG SHA-256 values from these exact-head artifacts (Windows source PNGs and Linux production PNGs):

| Page | Windows EN | Linux EN | Windows 简中 | Linux 简中 |
|---|---|---|---|---|
| General | `8799f7e15081873b4531f8408abc84b56b29c3179d7c8ed97d6945f59a4a1a1c` | `d3da8509ff563fb83d5409923979e97caadd7f9181898c67c53c05cba2b5c46c` | `99c1bb2ba1ff0fdd31b032666c7181352d60e0b5646c100f7935a8770ab397c1` | `d178edd804adfaa45f316987f8fb468c656b85d115fe919e37504315ba0786b9` |
| Memory cleaning | `4a12e0166abf0d9ad2b7a2072a746a47b18aec74d1699aded5b60f5ee386eb26` | `931b6758c25471f5b444d5d82c68ffc41a465f9b6f0cd5015623a1ecf2c72134` | `ab2718a0e09ca7d1c9b815c63b8ed929e6d251307bd37f6633874d54c4071532` | `e2cf2c780ecc68eadcacd0a961bebd8656553d37b8484c82e00d749238ed0225` |
| Appearance | `5defc2a738949f2cfabf60c1c60f728d5e8d24a3ecf40bdc833cf599207fed1e` | `f2a93915065fbb7cc47da8042f43298ea5689909fa588168a613e78cc60ffc85` | `b16684fed99010106228adf20d421006adc8452c87fd4d4afb2244518601ed2d` | `14d42231cd86aad735946ec9aaeb5135f83ea5be677dcbea2de232f6cfa8d7f2` |
| Tray icon | `00f5117749549cbf94aafd99a9995bb1b0b6f4ac3e6184d9b4eac977e75c6c6e` | `57176242e1dc0482a2fecea925851afa7172d176aad6a04295f15318702da3d0` | `16307331ab9847accc929192c4e68f8834e2e2c0e0eb2f80621487c0a162ded6` | `8dfefa715266e269038ebf2092aa597765e1bb81765679ebf0fec01cb7ff4b07` |
| Advanced | `eeba3ff2fd139fd31edf3eb4d6323f6690744fe47e5c81ae069c898d56c9bda6` | `41e1074f682eecf13c21891dce795691f7fdb29f7ccf5bb0f9d31e1b423106a2` | `94d2c197df710baa3980e4117548948f6b7e129688888f98d0756f6b083c386d` | `604df69cbb247b8b8a85d57caa0ba1aa88df4423b7ebf303a64d9e1ddde4d101` |

## P2C-2 prior exact-head calibration evidence (`1d1d6562cb27ddb63fb38a2bf71756b5e88d63ee`)

The Windows and Linux captures below were generated from the same PR source head. The Linux production capture is from Xvfb run `40284909745`; the fresh Windows baseline is from run `40284909821`. Both are isolated hosted-runner captures. The Windows baseline job passed, but the separate Xvfb capture job in `40284909821` reached its 25-minute job timeout and the hosted comparison job was skipped. The duplicate push run `40284904057` also timed out in Xvfb and skipped comparison. An exact-head local comparison was run from the successful downloaded artifacts with the bundled Pillow 12.3.0 runtime; no host package was installed. The workflow now runs on the PR event only, avoiding a duplicate push run, and gives Xvfb/comparison jobs 45/20 minutes.

| Evidence | Run / artifact | Result |
|---|---|---|
| Zamak/project build and tests | PR run `40284909782` | PASS |
| Production Xvfb/XTEST and lifecycle | PR run `40284909745`, artifact `12633129180` | PASS; five pages in both languages; clean exit and window resource release |
| Production Weston Headless/Pixman lifecycle | PR run `40284909745`, artifact `12633864035` | PASS; main and settings clients exited 0; no Wayland input claim |
| Official Windows 3.5.2 baseline/captures | PR run `40284909821`, artifact `12633108874` | Windows job PASS |
| Dedicated hosted Windows/Linux comparison | PR run `40284909821` | SKIPPED because its required Xvfb capture job timed out at 25 minutes |
| Duplicate push baseline run | `40284904057` | CANCELLED at the Xvfb job timeout; Windows baseline passed; comparison SKIPPED |
| Exact-head local artifact comparison | Windows `12633108874` + Linux `12633129180` | PASS: 172 geometry rows within 2 px, 0 geometry failures, 12 middle-scroll rows `NOT_COMPARABLE_NATIVE_STATE`, 10 raster checks PASS, 8 color swatches matched |

The local comparison generated unscaled client crops, side-by-side images, 50% overlays, difference heatmaps, metrics, resource-ID geometry deltas, raster checks and SHA-256 manifests. The Windows image was cropped only at measured client origin `(8,31)`; neither source image was resized, stretched or shifted. Pixel percentages are comparison-only, not a 1:1 pass:

| Page | English changed pixels | 简体中文 changed pixels |
|---|---:|---:|
| General | 14.7558% | 13.0856% |
| Memory cleaning | 18.4080% | 18.0501% |
| Appearance | 16.6153% | 15.8225% |
| Tray icon | 18.6990% | 17.7446% |
| Advanced | 8.6128% | 7.9548% |

The production renderer has no full-page child-dialog outline; it draws only resource-defined group boxes. Appearance `IDC_COLORS=140` displays read-only source defaults (`#FFFFFF`, `#008040`, `#FF8040`, `#EC1C24`). `upstream_default_checked()` renders the verified 3.5.2 check states; the 28-test suite checks these defaults and verifies disabled clicks do not change them. The permanent “Template defaults only” footer is absent; limitations appear as interaction feedback and in the Reset confirmation without adding a permanent row. No settings are persisted or enabled.

The narrow GTK close interception was exercised on GTK 3.24.41. Logs record `delete-event` signal id `66`, the single pre-existing unblocked handler id `70`, only that handler blocked, then the targeted application handler connected. Runtime tree inspection found exactly one direct `GtkBox` holder with the expected 12 px border and adjusted only that verified holder to 0 px. XTEST exercised Close, Escape, Alt+F4, WM_DELETE_WINDOW, repeated hide/show, re-open of the same settings window, main-window availability and final resource release; the application exited 0. This does not validate other GTK/libui-ng versions.

The Linux runner records Ubuntu 24.04, GTK 3.24.41, Xvfb 1024×768×24, scale 1 / 96 DPI, Adwaita light, DejaVu Sans and Noto Sans CJK SC. Windows records Windows Server 2022, 1024×768, 96 DPI / 100%, light theme and 8 pt `Ms Shell Dlg`. Font and raster differences remain visible and were not counted as geometry errors.

Current Linux production screenshot SHA-256 (artifact `12633129180`):

| Page | English | 简体中文 |
|---|---|---|
| General | `254add6b1f447a519f9dd04adb74128106e0706cd22ae5abb7879cae2920a979` | `5d10063d95d0478da0b7c1dd057f857f82ddbf6b3ccebb8f81af4b7cbd0f7523` |
| Memory cleaning | `0b2d49fa4c3b1d2c5164a91f5819752c691f92141c2ccbe87b4b4f2a07af8aa9` | `45160cb34aa625949c5bd6d405fb86c7fac420c90bcc8608514278c21f9ce055` |
| Appearance | `ec1a3156dd79b776cf48762b3c39bbcec424a0037b4a01dab10da41087527af6` | `deec7d59446db84a490daaf89ecf16d29bbb6fd8ac6f4d7d85d18a0bdae95345` |
| Tray icon | `bc7bacb4b4b473aff943a493373de500cacbf2cd96f3d4c3fc201071812b7f86` | `3d48ada55696c135515cf52ec7b8e13ad4a9bdd5a8152525a3bed8d7f4e70236` |
| Advanced | `0a8e2e2ab351605c3fd28e7d09dbdd79f7d06a06a1dbbf612216dd16e24f0abc` | `d5554793d9869a69e0c3752fe4a6ab48e5e53c56930efddae45d00ac68be70d8` |

Current Windows-native screenshot SHA-256 (artifact `12633108874`):

| Page | English | 简体中文 |
|---|---|---|
| General | `22333ba98bdb72b58a20de718b68ba9c50192883ad36a07f191fccfbcf1a7c4e` | `499d8f110c86ef435f1ac0e6929a07e83bdb0a7ab2bb0cc345d0e006cb983d53` |
| Memory cleaning | `643fe50fe22d0aaebb30ac75f15d411e45c93a03473dfffab2b8c172a990c677` | `c3565daa379f78b9dc61e08a2a14793ab765c30fe50b71ed786b6fd8f2500ac9` |
| Appearance | `4b3b11e4daace5daa61c9e72ccb333c50d3fc5830341887ba6292f0050c07958` | `f7edd76780322ae721d36775811d822329e8356e6d4234b11de04a2f8d13b4e9` |
| Tray icon | `20f665322221dbfa8bde2ee638893f55ac2cda5a10daf97b5b4d9eb3c5b45bf7` | `cbaa725431964ff081f41bb2ee2bc9aef1e1b97f11fc3008d6ef008e1c197e8` |
| Advanced | `5873fad6dc6b96630b76351cbb46fc4a2cbb723f7b335935581083f1bf0f7f3f` | `7cc65d5076b8769cbd012b35198d4ca3b109273f7d9e33638583a81440d5b64c` |

The Windows artifact verifies source tag `2f5c55d15f69a2c88859e036b16c6f4c971e1968`, portable executable version `3.5.2`, and SHA-256 `dd55a81d56e8c32918e5190b2ec2a1f2a7edbbca884627439b4b619fdfb5602d`; the executable is not Authenticode-signed. Full capture timestamp and per-file checksums are in the linked Actions artifacts.

### Follow-up GroupBox color calibration

Sampling the native and Linux `General configuration` GroupBox left edge at client coordinate `(156,20)` found Windows `(220,220,220)` and Linux `(187,187,187)` before the correction. Run `40289475248` then showed that changing only the color left a rasterized `(229,229,229)` edge on Linux, while 172 geometry rows, 10 other raster checks, and the production Xvfb job passed. The half-pixel follow-up is validated in the exact-head comparison above: the edge sample now matches `#DCDCDC` in English and Simplified Chinese. Dark-theme calibration remains provisional because no authentic dark Windows screenshot is available.

Typography remains a separate visual difference: Windows uses 8 pt `MS Shell Dlg`, while the hosted Linux capture uses DejaVu Sans and Noto Sans CJK SC. The current settings renderer calls select size/weight without selecting a font family; disabled text also remains visually grayed to communicate that these settings are read-only. No pixel or typography parity is claimed from the geometry results.

## P2C-2 visual calibration addendum (2026-10-26)

P2C-2 continues the existing Draft PR #7 on `feat/issue-3-p2c-settings-ui`; it does not create another PR. The authenticated Windows reference remains Mem Reduct `v.3.5.2`, tag SHA `2f5c55d15f69a2c88859e036b16c6f4c971e1968`, captured on Windows Server 2022 at 96 DPI / 100% scaling. Settings client target remains 558×335. No capture is resized, stretched, or shifted to improve a match.

The exact pre-calibration source head was `90f5b31a9dede8896a44bd356c43f38862a005db`. Its Windows source/geometry artifact was `12632986603`, production Xvfb screenshot artifact `12632328288`, and comparison artifact `12632129496` from Actions runs `40282499292` (Windows + Xvfb + comparison), `40282499286` (Xvfb/Weston), and `40282499313` (build/test). That comparison reported 172 control-rectangle rows within 2 px, 12 Linux-only intermediate scroll rows marked `NOT_COMPARABLE_NATIVE_STATE`, and zero geometry failures. Raw pixel comparison was not a pass: changed pixels ranged from 61.7290% to 80.1728% across the five pages and two locales.

The pinned screenshots showed three actionable raster differences, now corrected in the production renderer:

- The page surface and blank footer now use the measured Windows light-theme button-face color RGB `(240,240,240)`; the navigation list stays white. Group-box title knockouts use the same page surface.
- Resource checkboxes now use a centralized 13 px square and matching checkmark proportions, measured against the native screenshots.
- The memory-region list checkbox and label insets now follow the observed Win32 list item placement: checkbox inset 4 px and label inset 22 px from the exported row rectangle (previous Linux values were 14 px and 32 px).

The four Appearance color rows retain source-verified read-only previews (`#FFFFFF`, `#008040`, `#FF8040`, `#EC1C24`). Source-default checkmarks remain a read-only presentation snapshot; they are not loaded from or saved as Linux user settings. All non-language configuration controls remain disabled and cannot trigger settings persistence or system actions.

**Superseding exact-head result:** the narrowed checkbox ROI on `1d1d6562cb27ddb63fb38a2bf71756b5e88d63ee` passes the same-head local comparison: 10/10 raster checks, 8/8 swatch samples and 172/172 comparable geometry rows within 2 px. The hosted comparison job did not run because the required Xvfb job timed out; local comparison is not a substitute for a successful hosted job. A corrected workflow is queued with the next PR update.

The visual comparator separately validates reference page-face samples and checkbox raster bounds, in addition to the resource-ID keyed Win32/Linux geometry table. It keeps text antialiasing in the pixel-difference output rather than misclassifying it as control geometry. The first exact-head run `f5c5488b4f4e0871311c52dfe4bde61d89be7b7a` generated captures in Windows artifact `12633352398`, production Xvfb artifact `12633925245`, and comparison artifact `12634010092`. Its production GUI/XTEST and Weston run `40284199595` and build/test run `40284199576` passed. The Windows/Xvfb/comparison run `40284199572` failed only the raster comparator: two Memory row samples included part of the next list row. This historical failure is superseded by the exact-head run `40290615170` documented above, where both Memory row samples and all other raster checks pass.

Date: 2026-10-26
Upstream: Henry++ Mem Reduct `v.3.5.2`, source tag commit `2f5c55d15f69a2c88859e036b16c6f4c971e1968`
PR: [#7 — P2C-1 five-page settings UI](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/7), OPEN / Draft
Base: `feat/issue-3-p2b-main-window` at `911a3d0b99b05f15bd0109dca475b0b5174e7888`
Production code tested: `d4f1ca97ac42bc1516a484163b9d4536ea9194b2`
P2C-1 full PR validation at that documentation checkpoint: head `b473700f0513e91fc6f171fa2b0a8fc3abd3f7df`; production source files were unchanged from the tested code checkpoint.

## P2C-1 initial result (production code checkpoint `d4f1ca97ac42bc1516a484163b9d4536ea9194b2`)

The production Zamak settings window presents all five upstream pages, source resource IDs, and independent English / 简体中文 selection. Xvfb/XTEST confirms navigation, keyboard actions, Reset and Close flows. Controls for features that have no safe Linux implementation remain visibly disabled and do not write settings, start cleanup, invoke Polkit, or modify kernel state.

An isolated Windows-hosted runner successfully captured the official v3.5.2 app. At 96 DPI / 100% scaling, its settings window measures **574×374 outer and 558×335 client**. The production Linux Canvas now measures **558×335** in Xvfb at scale 1. This verifies matching client-area dimensions under the recorded conditions; it does not establish matching pixels, fonts, borders, behavior, or Wayland decoration.

## Upstream release and capture provenance

The isolated `windows-2022` GitHub-hosted runner resolved the official `v.3.5.2` Release and verified the tag points to the frozen source SHA. It ran the portable official executable without invoking memory cleanup.

| Evidence | Verified value |
|---|---|
| Windows runner | Microsoft Windows Server 2022 Datacenter, build 20348 / version 10.0.20348 |
| Capture time | `2026-10-26T04:31:57Z` |
| Desktop | Interactive session 2; Explorer in current session; input desktop accessible |
| Display | 1024×768; 96 DPI; 100% scaling |
| Theme and font | Light theme; resource font `8pt Ms Shell Dlg` |
| Windows native main window | 352×407 PNG/client window geometry |
| Windows settings dialog | 574×374 outer, 558×335 client |
| Settings navigation | `IDC_NAV=104`, client rect `(12,10)`, `132×312` |
| Child page area | client rect `(156,10)`, `390×273` |
| Reset / Close | `(153,299) 75×23` / `(471,299) 75×23` |
| Official portable archive | SHA-256 `ec41ca15ff623f53850d9ee01986d3f8ff457ec93457a6ceeb993b7164023d15` |
| Official setup asset | SHA-256 `025e5c06f7beccf5c85cbe931bc058f53a4642ca83a00855a475026a5e6334c4`; detached PGP signature verified, signer `D98523611524AB29BE7330AC288120A75635B5FD` |
| Extracted executable | version/product version `3.5.2`; SHA-256 `dd55a81d56e8c32918e5190b2ec2a1f2a7edbbca884627439b4b619fdfb5602d`; Authenticode status `NotSigned` |

The portable archive has no detached signature. Provenance is the official release source, pinned release hash, verified signature on the setup asset, and pinned upstream tag. No user workstation was used. The screenshots are hosted as Actions artifacts rather than committed to the public repository.

## Geometry correction

The initial Linux settings content was 523×321 because libui-ng's GTK content-holder child had a 12 px container border. Changing the settings top-level border, GTK style, or Canvas margins did not remove this inset. `native/gui_window.c` now requires a GTK box root, enumerates its direct children, and selects exactly one child that is itself a `GtkBox` with the verified 12 px border; it does not assume a child index or that the last child is the holder. The Xvfb run records the widget tree, `CI_SETTINGS_CONTENT_BORDER old=12 new=0`, and measures a 558×335 Canvas. Unsupported GTK trees fail closed with diagnostics.

The earlier `547×345` top-level / `523×321` Canvas values are superseded and retained only in earlier-run history. Current measured Linux Canvas and Windows client area both equal 558×335 at scale 1 / 96 DPI. GTK/Wayland client-side decorations may change the visible outer window; only the Canvas/client rectangle is compared here. The five `260×168 DLU` Windows resources remain child-page templates, not the settings outer frame.

## Resource coverage and Linux behavior

| Resource IDs | Page/control | Implemented UI | Linux behavior status |
|---|---|---|---|
| `IDD_SETTINGS_GENERAL=101`, `IDC_NAV=104`, `IDC_TITLE_1=107`, `IDC_TITLE_2=108` | General page and navigation | Separate settings top-level, navigation, localized labels, focus and page selection | `EQUIVALENT` |
| `IDC_ALWAYSONTOP_CHK=116`, `IDC_LOADONSTARTUP_CHK=117`, `IDC_STARTMINIMIZED_CHK=118`, `IDC_REDUCTCONFIRMATION_CHK=120`, `IDC_SKIPUACWARNING_CHK=119`, `IDC_CHECKUPDATES_CHK=121` | General checkboxes | Drawn disabled; clicks report the limitation; no setting is applied | `NOT_IMPLEMENTED`; skip-UAC item `UNSUPPORTED` |
| `IDC_LANGUAGE_HINT=122`, `IDC_LANGUAGE=123` | English / 简体中文 | Live mouse/keyboard selection and redraw | `EQUIVALENT`; not persisted |
| `IDD_SETTINGS_MEMORY=102`, `IDC_TITLE_3=109`, `IDC_TITLE_4=110`, `IDC_TITLE_5=111` | Memory cleaning page | Source-ordered choices, threshold, interval, and hotkey fields | UI retained; underlying actions `UNSUPPORTED` / `NOT_IMPLEMENTED` |
| `IDC_REGIONS=124` | Eight Windows cleanup regions | All rows retained in source order and disabled | `UNSUPPORTED`; no generic `drop_caches` mapping |
| `IDC_AUTOREDUCTENABLE_CHK=125`, `IDC_AUTOREDUCTVALUE_CTRL=126`, `IDC_AUTOREDUCTVALUE=127`, `IDC_AUTOREDUCTINTERVALENABLE_CHK=128`, `IDC_AUTOREDUCTINTERVALVALUE_CTRL=129`, `IDC_AUTOREDUCTINTERVALVALUE=130`, `IDC_HOTKEY_CLEAN_CHK=131`, `IDC_HOTKEY_CLEAN=132` | Automatic cleanup and global shortcut | Template controls retained, disabled; no auto action or global binding | `NOT_IMPLEMENTED` |
| `IDD_SETTINGS_APPEARANCE=103`, `IDC_TITLE_6=112`, `IDC_TITLE_7=113` | Appearance page | Style, font, and color rows retained in source order | UI retained; tray rendering `NOT_IMPLEMENTED` |
| `IDC_TRAYUSETRANSPARENCY_CHK=133`, `IDC_TRAYSHOWBORDER_CHK=134`, `IDC_TRAYROUNDCORNERS_CHK=135`, `IDC_TRAYCHANGEBG_CHK=136`, `IDC_TRAYUSEANTIALIASING_CHK=137`, `IDC_FONT_HINT=138`, `IDC_FONT=139`, `IDC_COLORS=140` | Appearance controls | Drawn disabled; report that the Linux tray renderer is unavailable | `NOT_IMPLEMENTED` |
| `IDD_SETTINGS_TRAY=104`, `IDC_TITLE_8=114`, `IDC_TITLE_9=115` | Tray icon page | Thresholds, click actions, result notice, and sound controls retained | UI retained; SNI behavior `NOT_IMPLEMENTED` |
| `IDC_TRAYLEVELWARNING_HINT=141`, `IDC_TRAYLEVELWARNING_CTRL=142`, `IDC_TRAYLEVELWARNING=143`, `IDC_TRAYLEVELDANGER_HINT=144`, `IDC_TRAYLEVELDANGER_CTRL=145`, `IDC_TRAYLEVELDANGER=146`, `IDC_TRAYACTIONSC_HINT=147`, `IDC_TRAYACTIONSC=148`, `IDC_TRAYACTIONMC_HINT=149`, `IDC_TRAYACTIONMC=150`, `IDC_NOTIFICATIONSOUND_CHK=151`, `IDC_SHOW_CLEAN_RESULT_CHK=152` | Tray icon controls | Drawn disabled and report unavailable StatusNotifierItem behavior | `NOT_IMPLEMENTED` |
| `IDD_SETTINGS_ADVANCED=105`, `IDC_ALLOWSTANDBYLISTCLEANUP_CHK=153`, `IDC_LOGRESULTS_CHK=154` | Advanced page | Both upstream controls retained and disabled | Cleanup option `UNSUPPORTED`; logging `NOT_IMPLEMENTED` |
| `IDC_RESET=105`, `IDS_RESET=82`, `IDS_QUESTION_RESET=83` | Reset | Confirm/cancel flow exists; confirmation states no saved settings are changed | Confirmation only, `EQUIVALENT` |
| `IDC_CLOSE=106`, `IDS_CLOSE=28` | Close, Escape, window close | Hides settings top-level and leaves main window running | `EQUIVALENT` |

Settings other than language are not persisted. Disabled rows cannot start an updater, create autostart entries or tray services, write a log, launch cleanup, invoke Polkit, or write kernel state. Reset does not mutate user settings.

## Validation evidence

### Local non-GUI validation at production code head `d4f1ca9`

| Command | Result |
|---|---|
| `zam --version` | PASS — `0.2.0` |
| `zam check .` | PASS — `CHECK OK memreduct_zamak` |
| `zam test .` | PASS — 22 passed, 0 failed |
| `zam build .` | PASS — output `build/memreduct_zamak-1cd233708f715abadab9e41fd55d78cf68a9c9ead9fb903091b1b3ecd104f2e6` |
| `bash scripts/build-arch.sh` | PASS — Zamak checks/tests/build and native helper build |
| `python3 scripts/verify_project.py` | PASS — 33 governance/project checks |
| `python3 -m unittest discover -s tests -v` | PASS — 3 C safety tests |
| `bash -n` for the two changed CI scripts | PASS — `p2c-settings-xvfb.sh`, `p2b-production-wayland.sh` |
| `git diff --check` | PASS — no whitespace errors at code checkpoint |
| Local GUI/Xvfb/Weston | NOT RUN — no local GUI or compositor was started |

The GitHub build-and-test workflow also ran the full project shell syntax step and native build for the same code SHA.

### GitHub Actions at code head `d4f1ca97ac42bc1516a484163b9d4536ea9194b2`

| Lane | Run and outcome |
|---|---|
| Production Xvfb/XTEST, push at code checkpoint | [40268142444](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40268142444) — PASS; artifact [12627741039](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40268142444/artifacts/12627741039) |
| Weston Headless/Pixman, push at code checkpoint | [40268142444](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40268142444) — PASS; artifact [12626827840](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40268142444/artifacts/12626827840) |
| Zamak/build-and-test, push at code checkpoint | [40268142555](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40268142555) — PASS |
| Windows native 3.5.2 baseline, push at code checkpoint | [40268142370](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40268142370) — PASS; artifact [12626872531](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40268142370/artifacts/12626872531) |
| Production Xvfb/XTEST, pull request at docs head `b473700` | [40269534094](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40269534094) — PASS; artifact [12627279867](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40269534094/artifacts/12627279867) |
| Weston Headless/Pixman, pull request at docs head `b473700` | [40269534094](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40269534094) — PASS; artifact [12627990285](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40269534094/artifacts/12627990285) |
| Zamak/build-and-test, pull request at docs head `b473700` | [40269534153](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40269534153) — PASS |
| Windows native 3.5.2 baseline, pull request at docs head `b473700` | [40269534205](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40269534205) — PASS; artifact [12627309561](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40269534205/artifacts/12627309561) |

Xvfb environment: Ubuntu 24.04 hosted runner, Xvfb 1024×768×24, GTK 3.24.41, scale 1, DPI 96, Adwaita light, DejaVu Sans and Noto Sans CJK SC. The production settings Canvas is 558×335. XTEST drives the isolated X server; it covers all five pages in both languages, navigation, disabled rows, language controls, Reset, Escape, Close, WM delete handling, and normal process exit. Settings exit returns to the main window; main exit status is 0.

The push Weston artifact uses Weston 13.0.0 Headless/Pixman with a private Wayland socket. Both production main/settings clients connect, create surfaces, draw, and exit with status 0. The headless runner advertises no `wl_seat`; no Wayland pointer/keyboard input is claimed. Logs retain one `gdk_seat_get_keyboard` GTK critical and Weston shutdown's `BUG: finalizing a layer with views still on it.` No cause is attributed to the application from these observations.

### Production Linux settings screenshots

X11 artifact: [12627279867](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40269534094/artifacts/12627279867). These are screenshots of the production Zamak program at docs head `b473700f0513e91fc6f171fa2b0a8fc3abd3f7df` with production sources identical to code head `d4f1ca97ac42bc1516a484163b9d4536ea9194b2`; they are not screenshots of a simulation.

| Page | English SHA-256 | 简体中文 SHA-256 |
|---|---|---|
| General | `5fec8731e6b945d4b31974f879beb6e9719af391864dbb746f28dd5ba598c1fd` | `49de8bcce8398a18f0a8dc77e8d5b9750ad27485a34ec9a18c0762d74577dda4` |
| Memory cleaning | `e207d08a47702b11c5de6296bebac79f6c1c8211f0f711962076d6a34382796e` | `e0910fde574df2682f9c5d49414d2dbdabf91ab9dfc0a2a0695def0c6ff6d1c0` |
| Appearance | `d5fd131690e21fd8cd0907beed20aa4091d32045572f6c7e3ef52e924e76bd64` | `0386811b1181e11e102bf8d003175e7738eb1479d16bccacd8d10ce9b055371d` |
| Tray icon | `36d9160bc51d090cd45303e214b8b80e1259048f08deede590afa4a1dd8e0ce7` | `555c7581a66c67a0711db56a9dff1f3028358edb71875407ea2a20e88e824729` |
| Advanced | `0c2c383abf848958dbf78154143644f6191940d0b6451d86a598930dd8a090cf` | `5966bdc6250556c7be0c2e23a760dd00bfcbc5724742ebaccfeed533e5b73628` |

Current production UI source/script SHA-256:

| File | SHA-256 |
|---|---|
| `src/main.zm` | `39caaa075a19c250bbdabc121251d217daa1dff94c3ae144581a3ab7b33da37c` |
| `src/settings_ui.zm` | `8b89c5f3217ef25b20c3b9c4241e744bf2e7ac19ee078c75ba5e5adb342df391` |
| `src/ui_geometry.zm` | `ed9c77fb20a5931f7b5a2dd8f0f797a961a20c43bf8bb44d573442c10baa7e52` |
| `native/gui_window.c` | `89602a35c021f01e9f9a04c15e74c58fada1cd5b3e83e6108a22e54dba0066c6` |
| `scripts/ci/p2c-settings-xvfb.sh` | `740c0af3e4f13523edc93b2372723f2d040073df04df8f2bad6b20edab05bc2d` |
| `scripts/ci/p2b-production-wayland.sh` | `7c978267e3acd3ee01c6236dcdccd6b6761c2736a113f82c92386834882687b5` |

### Windows-native screenshot index

Windows artifact: [12627309561](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40269534205/artifacts/12627309561). It contains the native main window in English and Simplified Chinese, plus every settings page in both locales. PNG SHA-256 values were recomputed from the downloaded artifact.

| Page | English SHA-256 | 简体中文 SHA-256 |
|---|---|---|
| Main window | `7db5c0eb8c8d22a00103b4c17739516a077e3c2d6e7439ab9715bd9820738780` | `d781449593bf8122c4b812612f39ed2babeb0edb7197748eae53c013d10aca87` |
| General | `a5670413aa059b67b027295a98bab477bff22f3cc440329f6af00700b0c8d651` | `50a8b7e2aca25d1044c3f547fbf40df301c21f946a12873f13b57ab2f4853df5` |
| Memory cleaning | `a13f9effaef9cd1065a980c019a8edd81dd06244699736b0e3ff6c0c98f3dbd4` | `0ef7d01e21fc7703226c988c339843f6e04c32c40bcc8f0f720b54acb2ca8298` |
| Appearance | `d05bf0a5e2ef8cd79ca5804e65587d257575a113096fccb8c88bf186bcc3484a` | `0c69f32f6483933446ceb704daa69393754bd51da3fe097b9fbb82da25957735` |
| Tray icon | `ed72bb3408ee80d5c6856708c280c4a707b1c29adf65682a88e331af96c20428` | `1e7ab737fb2761b19b00d81fb190f34b8e33c8c3c7a42b5c4bff30581978a737` |
| Advanced | `929fae137f5e86fbfa487221c56e57d7967eddc5997e7563062f579007d6d742` | `05b35d0ef4367c3e5c75511ce1728b2acd4607966f91d8ec3a2e7148502ad5b5` |

## Fidelity status and remaining work

- **Client geometry at 96 DPI / scale 1:** PASS — Windows and Linux settings client areas both 558×335; Windows child page is 390×273 at client origin `(156,10)`.
- **Screenshot acquisition:** PASS — source-verified Windows-native main and ten settings captures, plus production Linux captures for all five settings pages in both locales.
- **Functional settings shell:** PASS for navigation, language selection, Reset confirmation-only flow, Close, Escape, and WM close. Xvfb uses real isolated XTEST events.
- **Control geometry at pre-calibration head `90f5b31`:** PASS for 172 directly comparable rows within 2 px; 12 intermediate Linux scroll rows are `NOT_COMPARABLE_NATIVE_STATE`; 0 geometry failures. This result is specific to that artifact and does not validate later source changes.
- **Pixel comparison at pre-calibration head `90f5b31`:** overlays and difference maps were generated, but pixel equivalence is not passed. Changed pixels ranged from 61.7290% to 80.1728% and mean channel difference from 16.2256 to 19.9119. The follow-up page-face and checkbox corrections require a new exact-head run.
- **P2C-2 exact-head raster/geometry result:** initial exact-head run completed; the only failure was the Memory checkbox test ROI including part of the next list row. The comparator crop was narrowed; this correction needs a new Actions result.
- **Pixel-level 1:1:** NOT ACCEPTED. Remaining typography, disabled-state contrast, native group-box/button/tree rendering, and antialiasing differences require review even if geometry passes.
- **Not captured:** dark theme, menus, tray/submenus, and cleanup confirmation/result dialogs. The Windows screenshot workflow currently captures main + five settings pages, not these states.
- **Not implemented:** settings persistence, active tray/SNI service, autostart/update checks, notification/sound, automatic cleanup, global hotkey, Linux-equivalent advanced cleaning, Polkit authorization, and any real memory cleanup. Disabled controls report limits rather than claiming success.
- **Wayland:** private Weston lifecycle only. No physical Hyprland, Wayland pointer/keyboard, or HiDPI input validation was performed.
- **Diagnostics:** one GTK `gdk_seat_get_keyboard` critical with no `wl_seat` advertised, and a Weston shutdown layer diagnostic remain in artifacts; causal relation is unproven.

Earlier Windows runner attempts and pre-correction geometry are historical only: runs `40261537106` / `40261541252` timed out during Chocolatey GnuPG installation; runs `40263501261` / `40263504561` failed because MSYS GnuPG received Windows-style paths. The current workflow uses runner-provided GnuPG with `cygpath` conversion, and the later Windows run above passes provenance, signature, executable-version, desktop, and native screenshot checks.
