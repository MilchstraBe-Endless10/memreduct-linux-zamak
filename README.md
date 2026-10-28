# Mem Reduct Linux — Zamak Native (unofficial)

> **Status (2026-10-29):** Draft [PR #7](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/pull/7) targets `main`; published HEAD `019a7f1e0755570c2c3a0b1202a6c764540f14e2` passed its eight main-base CI jobs. Local unpushed code HEAD `640fb5df044475df827308388c3da5be25d4ffad` includes strict memory-parser/helper hardening and a fix that redraws open settings after language/theme changes. Local checks: Zamak 44/44, independent Canvas probe 12/12, governance 34, C/Python 12/12, Shell syntax, production GUI/helper build. No GitHub CI is attached to this local HEAD. The current production frontend is Zamak `std/ui` → libui-ng → GTK3/Pango/Cairo. Settings rectangles are 184/184 within 1.5 px, but 95/120 same-source text metrics still exceed 2 px and whole-page 1:1 remains **NOT PASS**. An isolated Winelib reuse of the upstream General-page resource measured 13.2531% English and 12.2725% Chinese changed pixels; it is a promising frontend path, not a production migration or 1:1 result. The GTK main client is 337×369; the decorated Hyprland outer frame has not been measured, and the historical 20.9436% image difference is not a valid acceptance pair. See [UI fidelity and third-party toolkit findings](docs/ZAMAK_UI_FEASIBILITY.md) and [production main-window evidence](docs/P2B-1_PRODUCTION_UI_REPORT.md). The page-cache helper is not connected to the GUI; Polkit and real kernel writes remain unapproved and untested. This is **not** an official Henry++ release.

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

Use an ordinary user account in a graphical session. The build needs Zamak 0.2.0, a C compiler, GTK 3 development files, and `pkg-config`; install normal development dependencies using your distribution's package manager if they are missing. The build also compiles the helper for CI/package verification, but the trial command does not install or execute it.

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
