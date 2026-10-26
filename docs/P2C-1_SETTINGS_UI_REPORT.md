# P2C-1 — Five-page settings UI and Windows baseline acquisition

Date: 2026-10-26
Upstream: Henry++ Mem Reduct `v.3.5.2`, source tag commit `2f5c55d15f69a2c88859e036b16c6f4c971e1968`
Status: Linux settings UI is a provisional source-based implementation. Windows-native screenshot evidence and pixel comparison are not accepted until a Windows runner provides verifiable screenshots and environment metadata.

## Scope and geometry

The settings pages use the five official child templates `IDD_SETTINGS_GENERAL` through `IDD_SETTINGS_ADVANCED`, each `260×168 DLU`. The Linux UI creates a separate Zamak/GTK top-level window containing the shared settings shell, navigation, page canvas, status text, Reset, and Close. The estimated shell is `374×214 DLU`, producing a provisional X11 top-level of `547×345` and Canvas of `523×321` at the test runner's nominal scale 1. This shell estimate is not a measured Windows dialog size, and none of these GTK dimensions is an approved Windows pixel target.

The main window remains at its existing provisional `337×369` X11 top-level while settings is open. `native/gui_window.c` only finds the two application-owned GTK top-levels by their fixed titles, shows/hides the settings window, and intercepts its window-manager close request. It does not connect a settings action to a privileged operation.

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

The Xvfb lane runs the built production executable and injects events through the isolated X server. It captures all five pages in English and 简体中文, tests page navigation, disabled cleanup rows, mouse and keyboard language selection, Reset confirmation and cancellation, Escape, the Close button, the settings window-manager close request, and a normal main-window exit. The main top-level is checked to remain `337×369` while the settings top-level is `547×345`. Each page screenshot and source file SHA-256 is written into the GitHub Actions artifact. The settings dimensions are test-runner measurements for this provisional build, not Windows measurements.

The Weston Headless/Pixman lane starts the production program on a private Wayland socket and verifies main/settings surface creation and normal timeout-driven lifecycle. Weston advertises no physical input seat in this test path; it does not validate pointer or keyboard interaction. No test writes `/proc/sys/vm/drop_caches`, invokes Polkit, modifies Swap/ZRAM, or runs as root.

| Check | Local result before CI | Evidence/limit |
|---|---|---|
| `zam --version` | PASS | `0.2.0` |
| `zam check .` | PASS | `CHECK OK memreduct_zamak` |
| `zam test .` | PASS | 20 passed, 0 failed after keyboard focus regression was added |
| `zam build .` | PASS | Production executable linked with the separate GTK settings top-level bridge |
| `python3 scripts/verify_project.py` | PASS | 33 required project/governance checks |
| `python3 -m unittest discover -s tests -v` | PASS | 3 C safety tests |
| `bash -n` on `scripts/*.sh` and `scripts/ci/*.sh` | PASS | Every project and CI shell script parses; rerun after the X11 locale-marker and relative source-hash fixes |
| `git diff --check` | PASS | No whitespace errors after the latest script corrections |
| `bash scripts/build-arch.sh` | PASS | Project build entrypoint ran Zamak check/test/build and native helper build |
| Local Xvfb/XTEST | NOT RUN | `xorg-server-xvfb` and `openbox` are not installed; no software was installed on the host |
| Local Windows/PowerShell run | NOT RUN | `powershell` is not installed and the development host is not a Windows desktop |
| GitHub-hosted production GUI and Windows release workflows | PENDING | Record final run IDs, screenshot hashes, and Windows runner desktop result after push |

## Windows release provenance and screenshot gate

`.github/workflows/p2c-windows-native-baseline.yml` runs on an isolated `windows-2022` GitHub-hosted runner. `scripts/ci/windows-native-baseline.ps1` resolves the official `v.3.5.2` GitHub Release, verifies its tag commit against the frozen source SHA, downloads the official x64 archive and setup asset, checks the published SHA-256 manifest and fixed hashes, verifies the setup asset detached signature against the pinned signer fingerprint, checks the extracted executable SHA and file/product version, and probes whether the runner exposes an interactive input desktop. The script attempts screenshots only if that isolated runner reports an accessible interactive desktop; otherwise it records `BLOCKED` and uploads the provenance and environment record without fabricating images.

Pinned expected release evidence used by the workflow:

- Portable archive `memreduct-3.5.2-bin.7z`: SHA-256 `ec41ca15ff623f53850d9ee01986d3f8ff457ec93457a6ceeb993b7164023d15`.
- Setup `memreduct-3.5.2-setup.exe`: SHA-256 `025e5c06f7beccf5c85cbe931bc058f53a4642ca83a00855a475026a5e6334c4`; detached signature signer fingerprint `D98523611524AB29BE7330AC288120A75635B5FD`.
- Extracted portable `memreduct.exe`: SHA-256 `dd55a81d56e8c32918e5190b2ec2a1f2a7edbbca884627439b4b619fdfb5602d`.
- Native Windows screenshot completeness is `BLOCKED` until actual PNGs for the main window and five settings pages, release binary SHA, Windows build, DPI/scaling, display resolution, theme, locale, capture date/method, and image SHA-256 are present in an artifact.

Wine is not used. Windows native pixel comparison remains `BLOCKED / NOT RUN` until the source and Linux target screenshots exist under controlled, recorded display conditions. No 2-pixel claim is made.

## Open limitations

- Settings controls other than navigation, language, Reset confirmation, and Close are still disabled and non-persistent.
- The selected shell geometry and UI font metrics are provisional; original Windows child-page DLU coordinates and real Windows font/DPI conversion need screenshot-backed calibration.
- The settings top-level uses GTK decoration and a Canvas surface; glyph rasterization, accessibility semantics, and Windows-native controls differ and are not claimed pixel-equivalent.
- Tray/SNI, autostart, update service, configuration persistence, and every privileged cleanup action remain unimplemented or unsupported.
- Real Hyprland/Wayland input and Windows 3.5.2 visual equivalence are not tested by the hosted headless lifecycle lane.
