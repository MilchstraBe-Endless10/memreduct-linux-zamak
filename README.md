# Mem Reduct Linux — Zamak Native (unofficial)

> **Status (2026-10-30):** Draft [PR #7](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/7) targets `main`. Published PR HEAD `019a7f1e0755570c2c3a0b1202a6c764540f14e2` passed its eight main-base CI jobs; those runs do not cover the local R8 changes. Local code commit `3c41879d7589472fd64b7f8563268d60bc0b99bb` adds a source-backed Win32 settings-control map, custom GroupBox/Checkbox/ComboBox rendering and input state, and scroll-event forwarding. Local verification: Zamak 68/68, independent Canvas probe 12/12, governance 34, C/Python 13/13, Shell 12/12, Arch/native and production GUI/helper builds. This machine lacks Xvfb, Weston, Wine, `winegcc`, and `wrc`, so current-source GUI screenshots, isolated GUI interaction, Weston lifecycle, and a fresh Winelib A/B run are **NOT RUN**; no new GitHub CI is attached to this commit. The previous English/Light General-page screenshot is from an earlier source hash and is not an exact-head screenshot for the ComboBox arrow change. Its GroupBox correction lowered the measured page difference from 9.3388% to 7.6649%; whole-page 1:1 remains **NOT PASS**. The current frontend is Zamak `std/ui` → libui-ng → GTK3/Pango/Cairo. Same-source text metrics still have 95/120 samples over 2 px; no Windows-only font is bundled. See [P2C-4 and R8 visual evidence](docs/P2C-4_FONT_THEME_MENU_REPORT.md). The page-cache helper is not connected to the GUI; Polkit and real kernel writes remain unapproved and untested. This is **not** an official Henry++ release.

A free open-source, native desktop Linux memory utility inspired by [Mem Reduct](https://github.com/henrypp/memreduct). The project aims to offer a genuine one-click reclaim action and later controlled RAM compression features on **Arch Linux / Omarchy + Hyprland (Wayland)**, without Docker, a web panel, or a Python UI runtime.

## Start here

- [Agent and contributor contract](AGENTS.md)
- [Product scope (PRD)](docs/PRD.md)
- [Architecture](docs/ARCHITECTURE.md), [security](docs/SECURITY.md), [acceptance tests](docs/TEST_PLAN.md)
- [Roadmap P0–P6](docs/ROADMAP.md)
- [AI collaboration protocol](docs/AI_COLLABORATION.md)
- [Three-tier project memory](memory/LONG_TERM.md): LONG / MEDIUM / SHORT
- [GitHub setup](docs/GITHUB_BOOTSTRAP.md) and Codex prompts in [`prompts/`](prompts/)

## Current implementation and limits

| Included | Not included / unverified |
|---|---|
| Zamak 0.2.0 GTK3 production GUI, built by the pinned CI and local build script | Release packaging and installation validation |
| `/proc/meminfo` sampling; stale values are cleared when sampling fails | Full-page Windows 3.5.2 pixel identity; this remains NOT PASS |
| English/Simplified Chinese menus and five read-only settings pages | Tray service, saved preferences, automatic cleaning, ZRAM management |
| Xvfb/XTEST production interaction and private Weston client lifecycle checks | Real Wayland keyboard/mouse input is not covered by Weston CI |
| Strict `/proc/meminfo` parser and isolated helper/bridge tests using temporary files and a fake authorization launcher | GUI-connected cleaning, Polkit authorization, or real cache-reclaim behavior/performance |

Unsupported cleaning and Windows-only settings remain disabled. The production GUI smoke test does not need the helper and must not be run as root. No cleaning action is enabled or invoked by the steps below.

## Verification before building

```bash
python3 scripts/verify_project.py
python3 -m unittest discover -s tests -v
for f in scripts/*.sh scripts/ci/*.sh; do bash -n "$f"; done
```

## Build and try the GUI without installation

Use an ordinary user account in a graphical session. The build needs Zamak 0.2.0, a C compiler, GTK 3 development files, and `pkg-config`; install normal development dependencies using your distribution's package manager if they are missing. The production UI uses the system's normal GTK font resolution. Liberation Sans is tested only as an isolated Latin font candidate; it is not the production default because the bilingual comparison did not justify trading CJK rendering for Latin width matches. The application does not bundle Windows fonts, and CJK glyphs continue to use the system fallback. The build also compiles the helper for CI/package verification, but the trial command does not install or execute it.

```bash
npm install -g @veyralux/zamak@0.2.0
zam --version
python3 scripts/verify_project.py
python3 -m unittest discover -s tests -v
for f in scripts/*.sh scripts/ci/*.sh; do bash -n "$f"; done
zam check .
zam test .
bash scripts/ci/build-production-gui.sh
GDK_BACKEND=wayland ./artifacts/p2b/memreduct-zamak-production
```

The launcher above uses the current Wayland session and runs as the current user. This trial is for UI and read-only memory sampling only. Close the window normally. Do not click any future or experimental cleaning action.

`bash scripts/install-arch.sh` installs a privileged helper under a system path and is **not** part of this unprivileged trial. Do not use it as a prerequisite for testing the GUI; privileged actions, Polkit, Swap/ZRAM changes, and real cache cleaning require a separate security review and authorization.

The Windows comparison baseline is Mem Reduct v3.5.2. Current CI records 184/184 settings-control geometry checks and separate text/raster comparisons, but full-page pixel identity is still NOT PASS. A local main-window visual pair was captured; its Linux capture is the GTK client surface, while the Windows reference includes native frame/titlebar, so it is not a pixel-aligned pair or a 1:1 result. A real Hyprland run must be reported separately from Xvfb/Weston results.

## Public GitHub repository

The public repository is [MilchstraBe-Endless10/memreduct-linux-zamak](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak). PR #7 contains the settings UI and remains Draft until its review and integration gates are complete.

## License and attribution

Project independently implemented under GPL-3.0-or-later. Henry++ original Mem Reduct is GPL-3.0; no Windows-native memory-management code was copied into this Linux prototype. Zamak compiler MIT / Apache-2.0; Zamak `std/ui` uses libui-ng MIT. See [attribution](ATTRIBUTION.md), [license](LICENSE), and [third party notices](THIRD-PARTY-NOTICES.md).
