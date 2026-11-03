# Mem Reduct Linux — Zamak Native (unofficial)

> **Status (2026-11-03):** Owner-reported M1.2 evidence records one real `MADV_PAGEOUT` against a project-created 1 MiB child mapping: the kernel accepted 1 MiB of advice, residency changed from 256/256 to 0/256 pages, and the child remained alive with its data intact. This does **not** establish 1 MiB of net RAM freed, and does not validate reclaiming a daily application. Local M1.3 code adds a read-only same-user app picker, explicit one-target confirmation, a fixed helper bridge, and outcome reporting. A private Xvfb run passed the picker, cancellation, keyboard selection, mock authorization/helper, partial-result feedback, and normal close with `call_count=0`; it used fixed fixtures and issued no syscall or system write. The run also found and fixed misleading “kernel accepted” simulation text and GTK titlebar warnings. Python safety tests pass 190 (one existing path-length test is skipped), Zamak 0.2.0 tests pass 80/80, and default/opt-in GUI builds succeed. The default GUI still has privileged gates disabled; the local opt-in review build enables only cross-process reclaim and keeps page-cache cleaning disabled. No live process list or Polkit request was used. M1.3 remains incomplete pending review of real same-user enumeration and a separately authorized real daily-app test. Full-page Windows 3.5.2 pixel identity remains **NOT PASS**. This is **not** an official Henry++ release.

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
| Local-only M1.3 candidate: read-only app picker, explicit one-process choice and confirmation; fixed-fixture Xvfb interaction passes through mock result | Real same-user process enumeration, Polkit/helper execution, and daily-app reclaim are not verified |
| Strict `/proc/meminfo` parser and isolated helper/bridge tests using temporary files and a fake authorization launcher | Polkit behavior for an everyday app, system-wide reclaimed RAM, and reclaim performance |

The default GUI build keeps the privileged page-cache and cross-process gates disabled; the unprivileged self-target action is a separate feature. The opt-in M1.3 review binary is a local engineering candidate, not a release; it enables only cross-process reclaim and leaves page-cache cleaning disabled. Its Xvfb simulation uses fixed mock processes, not the host process list. Do not use the opt-in binary against an everyday process without a separate target-specific approval. The production GUI smoke test does not need the helper and must not be run as root.

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
