# P2C-1 — Five-page settings UI and Windows baseline acquisition

Date: 2026-10-26
Upstream: Henry++ Mem Reduct `v.3.5.2`, source tag commit `2f5c55d15f69a2c88859e036b16c6f4c971e1968`
Status: Linux settings UI is a provisional source-based implementation on Draft PR #7. Production Xvfb/XTEST and Weston lifecycle checks pass at tested application-code head `396c3126fbb8b9af33755ebf8f2eb67942c71f83`. Windows-native screenshot evidence remains blocked; no pixel comparison is claimed.

## Scope and geometry

The settings pages use the five official child templates `IDD_SETTINGS_GENERAL` through `IDD_SETTINGS_ADVANCED`, each `260×168 DLU`. The Linux UI creates a separate Zamak/GTK top-level window containing the shared settings shell, navigation, page canvas, status text, Reset, and Close. The estimated shell is `374×214 DLU`, producing a provisional X11 top-level of `547×345` and Canvas of `523×321` at the test runner's nominal scale 1. This shell estimate is not a measured Windows dialog size, and none of these GTK dimensions is an approved Windows pixel target.

The main window remains at its existing provisional `337×369` X11 top-level while settings is open. `native/gui_window.c` finds the two application-owned GTK top-levels by their fixed titles and shows/hides the settings window. On first settings display it replaces the previous `delete-event` handlers on that settings window with a callback that reports the close request to Zamak; CI observed GTK signal `66`, one previous handler disconnected, and the new handler connected. The callback keeps the reusable settings top-level alive while Zamak hides it and returns to the main window. It does not connect a settings action to a privileged operation.

## Resource coverage and state

| Resource IDs | Page/control | Implemented UI | Linux behavior status |
|---|---|---|---|
| `IDD_SETTINGS_GENERAL=101`, `IDC_NAV=104`, `IDC_TITLE_1=107`, `IDC_TITLE_2=108` | General page and five-page navigation | Independent settings top-level, navigation, localized labels, focus and page selection | `EQUIVALENT` |
| `IDC_ALWAYSONTOP_CHK=116`, `IDC_LOADONSTARTUP_CHK=117`, `IDC_STARTMINIMIZED_CHK=118`, `IDC_REDUCTCONFIRMATION_CHK=120`, `IDC_SKIPUACWARNING_CHK=119`, `IDC_CHECKUPDATES_CHK=121` | General checkboxes | Drawn disabled; clicking reports the limitation; no setting is applied | `NOT_IMPLEMENTED`; `IDC_SKIPUACWARNING_CHK` is `UNSUPPORTED` |
| `IDC_LANGUAGE_HINT=122`, `IDC_LANGUAGE=123` | English / 简体中文 | Live language selection by mouse or keyboard; both settings and main window redraw | `EQUIVALENT`; not persisted |
| `IDD_SETTINGS_MEMORY=102`, `IDC_TITLE_3=109`, `IDC_TITLE_4=110`, `IDC_TITLE_5=111` | Memory cleaning page and groups | Eight source-ordered choices, threshold, interval, and hotkey fields are drawn disabled | UI retained; actions `UNSUPPORTED` or `NOT_IMPLEMENTED` by underlying resource |
| `IDC_REGIONS=124` | Eight Windows cleanup regions | All eight rows retained in original order, disabled, and report a Linux semantic limitation when clicked | `UNSUPPORTED` |
| `IDC_AUTOREDUCTENABLE_CHK=125`, `IDC_AUTOREDUCTVALUE_CTRL=126`, `IDC_AUTOREDUCTVALUE=127`, `IDC_AUTOREDUCTINTERVALENABLE_CHK=128`, `IDC_AUTOREDUCTINTERVALVALUE_CTRL=129`, `IDC_AUTOREDUCTINTERVALVALUE=130`, `IDC_HOTKEY_CLEAN_CHK=131`, `IDC_HOTKEY_CLEAN=132` | Automatic cleanup and global shortcut | Template values shown, controls disabled; no automatic operation or global binding | `NOT_IMPLEMENTED` |
| `IDD_SETTINGS_APPEARANCE=103`, `IDC_TITLE_6=112`, `IDC_TITLE_7=113` | Appearance page | Style, font, and color rows retained in upstream order | UI retained; tray rendering actions `NOT_IMPLEMENTED` |
| `IDC_TRAYUSETRANSPARENCY_CHK=133`, `IDC_TRAYSHOWBORDER_CHK=134`, `IDC_TRAYROUNDCORNERS_CHK=135`, `IDC_TRAYCHANGEBG_CHK=136`, `IDC_TRAYUSEANTIALIASING_CHK=137`, `IDC_FONT_HINT=138`, `IDC_FONT=139`, `IDC_COLORS=140` | Appearance controls | Drawn disabled; clicking reports that the Linux tray renderer is unavailable | `NOT_IMPLEMENTED` |
| `IDD_SETTINGS_TRAY=104`, `IDC_TITLE_8=114`, `IDC_TITLE_9=115` | Tray icon page | Warning/danger values, click choices, result notice, and sound controls retained | UI retained; SNI behavior `NOT_IMPLEMENTED` |
| `IDC_TRAYLEVELWARNING_HINT=141`, `IDC_TRAYLEVELWARNING_CTRL=142`, `IDC_TRAYLEVELWARNING=143`, `IDC_TRAYLEVELDANGER_HINT=144`, `IDC_TRAYLEVELDANGER_CTRL=145`, `IDC_TRAYLEVELDANGER=146`, `IDC_TRAYACTIONSC_HINT=147`, `IDC_TRAYACTIONSC=148`, `IDC_TRAYACTIONMC_HINT=149`, `IDC_TRAYACTIONMC=150`, `IDC_NOTIFICATIONSOUND_CHK=151`, `IDC_SHOW_CLEAN_RESULT_CHK=152` | Tray icon controls | Drawn disabled and report unavailable StatusNotifierItem behavior | `NOT_IMPLEMENTED` |
| `IDD_SETTINGS_ADVANCED=105`, `IDC_ALLOWSTANDBYLISTCLEANUP_CHK=153`, `IDC_LOGRESULTS_CHK=154` | Advanced page | Both upstream controls retained and disabled | cleanup option `UNSUPPORTED`; logging `NOT_IMPLEMENTED` |
| `IDC_RESET=105`, `IDS_RESET=82`, `IDS_QUESTION_RESET=83` | Reset and confirmation | Confirmation can be opened, cancelled, or confirmed; both outcomes explicitly report no stored settings changed | `EQUIVALENT` confirmation flow only |
| `IDC_CLOSE=106`, `IDS_CLOSE=28` | Close button and window close | Button, Escape, and GTK window-manager close hide the separate settings top-level and leave the main window running | `EQUIVALENT` |

Every disabled control is a visual/input affordance only: it does not toggle a saved value, start an updater, write a log, register an autostart entry, create a tray service, start cleanup, invoke Polkit, or write kernel state. The canvas geometry is centralized in `src/ui_geometry.zm`; the shell and DLU conversion are `PROVISIONAL` pending authentic Windows measurements.

## Unprivileged production UI checks

The Xvfb lane runs the built production executable and injects events through the isolated X server. At head `396c3126fbb8b9af33755ebf8f2eb67942c71f83`, it captured all five pages in English and 简体中文, tested page navigation, disabled cleanup rows, mouse and keyboard language selection, Reset confirmation and cancellation, Escape, the Close button, the settings window-manager close request, and a normal main-window exit. The window-manager close produced `CI_NATIVE_SETTINGS_DELETE_EVENT`, `CI_SETTINGS_CLOSE_REQUEST_HANDLED main_visible=true`, and then the explicit main-window close returned the app with exit status 0. The main top-level remained `337×369` while the settings top-level was `547×345`. Each page screenshot and source-file SHA-256 is in the GitHub Actions artifact. These are test-runner measurements for this provisional build, not Windows measurements.

The Weston Headless/Pixman lane starts the production program on a private Wayland socket and verifies main/settings surface creation and normal timeout-driven lifecycle. Weston advertises no physical input seat in this test path; it does not validate pointer or keyboard interaction. No test writes `/proc/sys/vm/drop_caches`, invokes Polkit, modifies Swap/ZRAM, or runs as root.

| Check | Local result before CI | Evidence/limit |
|---|---|---|
| `zam --version` | PASS | `0.2.0` |
| `zam check .` | PASS | `CHECK OK memreduct_zamak` |
| `zam test .` | PASS | 21 passed, 0 failed |
| `zam build .` | PASS | Production executable linked with the separate GTK settings top-level bridge |
| `python3 scripts/verify_project.py` | PASS | 33 required project/governance checks |
| `python3 -m unittest discover -s tests -v` | PASS | 3 C safety tests |
| `bash -n` on all 8 files returned by `rg --files -g '*.sh'` | PASS | All project and CI shell scripts parse |
| `git diff --check` | PASS | No whitespace errors before commit; pushed worktree is clean |
| `bash scripts/build-arch.sh` | PASS | Project build entrypoint ran Zamak check/test/build and native helper build |
| Local Xvfb/XTEST | NOT RUN | `xorg-server-xvfb` and `openbox` are not installed; no software was installed on the host |
| Local Windows/PowerShell run | NOT RUN | `powershell` is not installed and the development host is not a Windows desktop |
| GitHub-hosted production GUI | PASS | PR run `40261537129`; Xvfb and Weston jobs both pass at tested head `396c3126fbb8b9af33755ebf8f2eb67942c71f83` |
| GitHub-hosted Zamak/build-and-test | PASS | PR run `40261541370`; push build run `40261537096` also passed |
| GitHub-hosted Windows release/screenshot attempt | BLOCKED | Initial runs timed out in Chocolatey GnuPG installation; follow-up runs `40263501261` / `40263504561` verified tag/assets but failed on MSYS GnuPG path handling. A `cygpath` correction is prepared, awaiting rerun |

## Verified GitHub Actions evidence

- Draft PR: [#7](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/7), base `feat/issue-3-p2b-main-window` at `911a3d0b99b05f15bd0109dca475b0b5174e7888`; tested head `396c3126fbb8b9af33755ebf8f2eb67942c71f83`.
- Production GUI PR run: [40261537129](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40261537129), Xvfb/XTEST and Weston Headless/Pixman both PASS. X11 artifact [12624986143](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40261537129/artifacts/12624986143); Wayland artifact [12623629586](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40261537129/artifacts/12623629586).
- Build-and-test PR run: [40261541370](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40261541370), PASS. The push GUI run [40261541380](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40261541380) also passed both jobs.
- Xvfb environment: Ubuntu 24.04 hosted runner; Xvfb `1024×768×24`, GTK 3.24.41, X11 scale 1, `GTK_THEME=Adwaita:light`, `Xft.dpi=96`, DejaVu Sans and Noto Sans CJK SC. Screenshots include the client window and exclude Openbox decorations. Window geometry was main `337×369`, settings `547×345`; the settings canvas was `523×321`.
- Tested UI source SHA-256: `src/main.zm` `39caaa075a19c250bbdabc121251d217daa1dff94c3ae144581a3ab7b33da37c`; `src/settings_ui.zm` `42c751013335811a1570c4ffd4b4351f185528fa3fe4635ff71127367a0541f2`; `src/ui_geometry.zm` `ade56cbdac04612636910a1c2591f61cbb6f5bd0b397875d066fa339e4a5c676`; `native/gui_window.c` `29ac8bfc397756c904928fb05841190986b9544784c1073463e869a340917b81`; Xvfb script `scripts/ci/p2c-settings-xvfb.sh` `690542d3d7af47fce8e81e99dd6db7438e3dacc6cd4249ae97200c8edf856d3e`.

### Production settings screenshots (X11 artifact 12624986143)

| Page | English SHA-256 | 简体中文 SHA-256 |
|---|---|---|
| General | `47a578e8edc797c7ec2f3ac7ec2d10d829090ceb4e0610f9f0c1273d0c3e8d9c` | `913a647c80023f41c81a038723651b9ef12b4768a7aa52cdb89b80d89a3d213d` |
| Memory cleaning | `5fad21b07fd5f154beae187ef9893065a2e5c7c0e68432b3f8d812e1d7ebe839` | `530f24617b2f4a76df6077a1dbe9faa57d329fca530406753d08f39910326c17` |
| Appearance | `a952d1fdea4418b1d2c60f5f335ef96440f8958e152146c380c002f7d583bff1` | `ee8bce6e316ca6f3b9369c3e06ddcd26db5fde4ea1012607f722de54dd496466` |
| Tray icon | `ecf08524ae4c4872bcee16950539560bb92a44918a399eb398abfa1e00571d62` | `6f386be8974c21187a08eb1b24377bf6b5292d8905d31205636e0d8eb2db20e4` |
| Advanced | `72539595e7b9307a157eee7572aa7b2e879d27db7c0bb82c2d5e08a84be7a110` | `dfef9d561996291b7343f999993fd5c2621c8e8fe2924a1e503d1e4534846d7e` |

The artifact also includes the production main window in both locales, View/language/cleanup menus, Reset confirmation, before/after sampling-failure screenshots, client logs, source hashes, and process exit status. Weston used a private Headless/Pixman socket; main and settings surfaces drew and exited 0. Its log retains one `gdk_seat_get_keyboard` critical and records that no `wl_seat` global was advertised. Pointer/keyboard input is NOT RUN in this Wayland lane.

The earlier PR GUI run `40260614636` at head `988513ad7c9afe5fd79cb0fe6239f7759cbdaf86` failed at the WM-close check: the settings close request caused the application loop to return before `CI_SETTINGS_CLOSE_REQUEST` appeared. This is preserved as a failed historical run, not counted as a pass; the native signal-routing fix is verified by the later run above.

## Windows release provenance and screenshot gate

`.github/workflows/p2c-windows-native-baseline.yml` runs on an isolated `windows-2022` GitHub-hosted runner. `scripts/ci/windows-native-baseline.ps1` resolves the official `v.3.5.2` GitHub Release, verifies its tag commit against the frozen source SHA, downloads the official x64 archive and setup asset, checks the published SHA-256 manifest and fixed hashes, verifies the setup asset detached signature using runner-provided GnuPG, checks the extracted executable SHA and file/product version, and probes whether the runner exposes an interactive input desktop. It attempts screenshots only if that isolated runner reports an accessible interactive desktop; otherwise it records `BLOCKED` and uploads provenance/environment data without fabricating images. The script does not install GnuPG or other packages; it fails early if `gpg.exe` is unavailable.

Pinned expected release evidence used by the workflow:

- Portable archive `memreduct-3.5.2-bin.7z`: SHA-256 `ec41ca15ff623f53850d9ee01986d3f8ff457ec93457a6ceeb993b7164023d15`.
- Setup `memreduct-3.5.2-setup.exe`: SHA-256 `025e5c06f7beccf5c85cbe931bc058f53a4642ca83a00855a475026a5e6334c4`; detached signature signer fingerprint `D98523611524AB29BE7330AC288120A75635B5FD`.
- Extracted portable `memreduct.exe`: SHA-256 `dd55a81d56e8c32918e5190b2ec2a1f2a7edbbca884627439b4b619fdfb5602d`.
- Native Windows screenshot completeness is `BLOCKED` until actual PNGs for the main window and five settings pages, release binary SHA, Windows build, DPI/scaling, display resolution, theme, locale, capture date/method, and image SHA-256 are present in an artifact.

On PR head `396c3126fbb8b9af33755ebf8f2eb67942c71f83`, Windows push run [40261537106](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40261537106) and PR run [40261541252](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40261541252) were both cancelled at the 20-minute workflow limit. The push artifact [12625127901](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40261537106/artifacts/12625127901) contains only `result.txt` (394 bytes): the official tag commit, archive SHA-256, and setup SHA-256 checks passed; execution then stalled during Chocolatey GnuPG installation. The runner log records an orphan `gnupg-w32` installer at cancellation. Signature verification, executable extraction/version inspection, desktop probing, and screenshot capture were not reached.

At PR head `82aa001e501e14ce6a2fc4a2da58341bc7ee0f8e`, the follow-up Windows runs [40263501261](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40263501261) and [40263504561](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40263504561) failed quickly after confirming the official tag and release asset hashes. Git for Windows `gpg.exe` was found, but treated the Windows `GNUPGHOME` and key file paths as MSYS paths; the keyring import failed. Artifacts `12624828971` and `12625208164` record that failure. The desktop probe and screenshots were again not reached.

The working copy now converts the temporary GnuPG home, public key, setup, and signature paths with the runner's adjacent `cygpath.exe` before invoking Git's GnuPG. This correction has not yet been validated by a Windows Actions rerun. Authentic Windows screenshot completeness remains `BLOCKED`.

Wine is not used. Windows native pixel comparison remains `BLOCKED / NOT RUN` until the source and Linux target screenshots exist under controlled, recorded display conditions. No 2-pixel claim is made.

## Open limitations

- Settings controls other than navigation, language, Reset confirmation, and Close are still disabled and non-persistent.
- The selected shell geometry and UI font metrics are provisional; original Windows child-page DLU coordinates and real Windows font/DPI conversion need screenshot-backed calibration.
- The settings top-level uses GTK decoration and a Canvas surface; glyph rasterization, accessibility semantics, and Windows-native controls differ and are not claimed pixel-equivalent.
- Tray/SNI, autostart, update service, configuration persistence, and every privileged cleanup action remain unimplemented or unsupported.
- Real Hyprland/Wayland input and Windows 3.5.2 visual equivalence are not tested by the hosted headless lifecycle lane.
