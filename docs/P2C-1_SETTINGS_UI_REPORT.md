# P2C-1 — Five-page settings UI and Windows baseline

## P2C-2 visual calibration addendum (2026-10-26)

P2C-2 continues the existing Draft PR #7 on `feat/issue-3-p2c-settings-ui`; it does not create another PR. The authenticated Windows reference remains Mem Reduct `v.3.5.2`, tag SHA `2f5c55d15f69a2c88859e036b16c6f4c971e1968`, captured on Windows Server 2022 at 96 DPI / 100% scaling. Settings client target remains 558×335. No capture is resized, stretched, or shifted to improve a match.

The exact pre-calibration source head was `90f5b31a9dede8896a44bd356c43f38862a005db`. Its Windows source/geometry artifact was `12632986603`, production Xvfb screenshot artifact `12632328288`, and comparison artifact `12632129496` from Actions runs `40282499292` (Windows + Xvfb + comparison), `40282499286` (Xvfb/Weston), and `40282499313` (build/test). That comparison reported 172 control-rectangle rows within 2 px, 12 Linux-only intermediate scroll rows marked `NOT_COMPARABLE_NATIVE_STATE`, and zero geometry failures. Raw pixel comparison was not a pass: changed pixels ranged from 61.7290% to 80.1728% across the five pages and two locales.

The pinned screenshots showed three actionable raster differences, now corrected in the production renderer:

- The page surface and blank footer now use the measured Windows light-theme button-face color RGB `(240,240,240)`; the navigation list stays white. Group-box title knockouts use the same page surface.
- Resource checkboxes now use a centralized 13 px square and matching checkmark proportions, measured against the native screenshots.
- The memory-region list checkbox and label insets now follow the observed Win32 list item placement: checkbox inset 4 px and label inset 22 px from the exported row rectangle (previous Linux values were 14 px and 32 px).

The four Appearance color rows retain source-verified read-only previews (`#FFFFFF`, `#008040`, `#FF8040`, `#EC1C24`). Source-default checkmarks remain a read-only presentation snapshot; they are not loaded from or saved as Linux user settings. All non-language configuration controls remain disabled and cannot trigger settings persistence or system actions.

The visual comparator now separately validates reference page-face samples and checkbox raster bounds, in addition to the resource-ID keyed Win32/Linux geometry table. It keeps text antialiasing in the pixel-difference output rather than misclassifying it as control geometry. The first exact-head run `f5c5488b4f4e0871311c52dfe4bde61d89be7b7a` generated fresh captures: Windows artifact `12633352398`, production Xvfb artifact `12633925245`, and comparison artifact `12634010092`. The production GUI/XTEST and Weston run `40284199595` and build/test run `40284199576` passed. The Windows/Xvfb/comparison run `40284199572` failed only the raster comparator: all six page-face samples and both General checkbox samples passed; the two Memory row samples were false failures because their test crop extended into the next list row. Geometry remained 172 PASS, 0 FAIL, plus 12 `NOT_COMPARABLE_NATIVE_STATE`; raw changed pixels improved to 7.9548%–18.6990%, still not pixel-equivalent. The crop has been narrowed and requires a fresh exact-head Actions run before its result can be accepted.

Date: 2026-10-26
Upstream: Henry++ Mem Reduct `v.3.5.2`, source tag commit `2f5c55d15f69a2c88859e036b16c6f4c971e1968`
PR: [#7 — P2C-1 five-page settings UI](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/7), OPEN / Draft
Base: `feat/issue-3-p2b-main-window` at `911a3d0b99b05f15bd0109dca475b0b5174e7888`
Production code tested: `d4f1ca97ac42bc1516a484163b9d4536ea9194b2`
Latest full PR validation: documentation-only head `b473700f0513e91fc6f171fa2b0a8fc3abd3f7df`; production source files are unchanged from the tested code checkpoint.

## Current result

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

The initial Linux settings content was 523×321 because libui-ng's final GTK content-holder child had a 12 px container border. Changing the settings top-level border, GTK style, or Canvas margins did not remove this inset. `native/gui_window.c` now finds the last child of the settings root box, confirms the holder, changes its border width to zero, and queues resize on the holder, root, and settings window. The Xvfb run records `CI_SETTINGS_CONTENT_BORDER old=12 new=0` and measures a 558×335 Canvas.

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
