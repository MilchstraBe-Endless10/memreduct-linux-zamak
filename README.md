# Mem Reduct Linux — Zamak Native (unofficial)

> **Status (2026-11-05):** The owner has reported a successful M0 page-cache operation and one controlled M1.2 `MADV_PAGEOUT` against a project-created 1 MiB child mapping (256/256 resident pages observed before and 0/256 after; the child survived with intact data). These observations do **not** measure net RAM freed or verify a daily application. A prior M1.3 run reported one accepted 16 MiB advice, but the operated PID did not match the one-time authorized PID; this is a failed target-integrity result, not a daily-app success. The local fix now binds the selection snapshot (PID, start time, UID, command/executable/scope identity, and reviewed mappings) through confirmation and helper revalidation, and refuses stale targets rather than substituting a same-name process. The M0+M1 candidate builds with both action gates enabled and no test hooks. An isolated Xvfb test exercised mouse selection of a 64 MiB ceiling, keyboard selection of 32 MiB, cancellation, and a mock request; it made zero helper or kernel calls. The production candidate was also opened and closed in isolated Xvfb with no CI hooks; this was a no-action smoke test, not a Hyprland or cleaning test. The candidate has not been installed. A fresh daily-app `MADV_PAGEOUT`, host install/rollback, current-build Hyprland interaction, and full-page Windows 3.5.2 pixel identity remain unverified; the last remains **NOT PASS**. This is **not** an official Henry++ release.

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
| English/Simplified Chinese menus, five settings pages, and saved language/theme preferences | Tray service, automatic cleaning, ZRAM management |
| Xvfb/XTEST production interaction and private Weston client lifecycle checks | Real Wayland keyboard/mouse input is not covered by Weston CI |
| Local M1.3 candidate: same-user process picker, explicit one-process choice, identity snapshot checks, and per-operation 16/32/64 MiB PAGEOUT ceiling (16 MiB default; helper ceiling 64 MiB) | The new snapshot-bound code has not yet passed a real daily-app reclaim; any stale target is refused |
| Strict `/proc/meminfo` parser; isolated helper/bridge tests using fixtures and a fake authorization launcher | Current candidate install/rollback on a host, everyday-app Polkit flow, net RAM reclaimed, and reclaim performance |

The default GUI build keeps the privileged page-cache and cross-process gates disabled; the unprivileged self-target action is separate. `m0+m1-candidate` is a local engineering build profile with both gates enabled and no CI/test hooks. It can invoke the fixed page-cache helper and Crossproc helper if those are installed, so treat it as a real-action binary. Its automated GUI simulation uses fixed mock processes; prior real Wayland candidate-selection evidence did not execute a helper or syscall. Never run the GUI as root.

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

The ordinary unprivileged smoke build does not need installation. To build the M0+M1 candidate, first run `bash scripts/build-arch.sh` to build both fixed helpers, then run `ARTIFACT_DIR=/absolute/output/path bash scripts/ci/build-production-crossproc-gui.sh m0+m1-candidate`. This candidate exposes a per-operation choice of 16, 32, or 64 MiB; 16 MiB remains the default, 64 MiB is the helper ceiling, and the effective request is further capped by the selected target's reviewed mapping bytes. Its build uses a profile-specific gate and marker; the installer rejects ordinary production binaries even if they contain the two individual gate symbols. Check it without system writes with `bash scripts/install-arch.sh --check-m0-m1-candidate /absolute/path/memreduct-zamak-production-m0-m1-candidate`.

Installation is deliberately a separate owner decision. The explicit command is `sudo bash scripts/install-arch.sh --m0-m1-candidate /absolute/path/memreduct-zamak-production-m0-m1-candidate`. It may replace these eight destinations: `/usr/bin/memreduct-zamak`, `/usr/libexec/memreduct-zamak-helper`, `/usr/libexec/memreduct-zamak-crossproc-helper`, `/usr/share/applications/memreduct-zamak.desktop`, `/usr/share/licenses/memreduct-zamak/LICENSE`, `/usr/share/doc/memreduct-zamak/THIRD-PARTY-NOTICES.md`, and the two `io.github.milchstrasse_endless10.memreduct.zamak*.policy` files under `/usr/share/polkit-1/actions/`. Before installation, back up every existing destination and record which paths did not previously exist. To roll back, run `sudo bash scripts/uninstall-arch.sh`, then restore the backed-up files with their original owners and modes; remove only newly introduced files that were absent before installation. Keep the backup until the restored installation has been checked. The build and preflight do not write to system paths. Do not use the candidate for real reclaim until target-selection, Polkit, and installation review are complete; Swap/ZRAM writes remain out of scope.

The Windows comparison baseline is Mem Reduct v3.5.2. Current CI records 184/184 settings-control geometry checks and separate text/raster comparisons, but full-page pixel identity is still NOT PASS. A local main-window visual pair was captured; its Linux capture is the GTK client surface, while the Windows reference includes native frame/titlebar, so it is not a pixel-aligned pair or a 1:1 result. A real Hyprland run must be reported separately from Xvfb/Weston results.

## Public GitHub repository

The public repository is [MilchstraBe-Endless10/memreduct-linux-zamak](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak). PR #7 contains the settings UI and remains Draft until its review and integration gates are complete.

## License and attribution

Project independently implemented under GPL-3.0-or-later. Henry++ original Mem Reduct is GPL-3.0; no Windows-native memory-management code was copied into this Linux prototype. Zamak compiler MIT / Apache-2.0; Zamak `std/ui` uses libui-ng MIT. See [attribution](ATTRIBUTION.md), [license](LICENSE), and [third party notices](THIRD-PARTY-NOTICES.md).
