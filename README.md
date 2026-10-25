# Mem Reduct Linux — Zamak Native (unofficial)

> **Status: architecture/prototype; Zamak executable and real Arch/Hyprland GUI are NOT YET VERIFIED.** Do not install a privileged service as a production app yet. This is **not** an official Henry++ release.

A free open-source, native desktop Linux memory utility inspired by [Mem Reduct](https://github.com/henrypp/memreduct). The project aims to offer a genuine one-click reclaim action and later controlled RAM compression features on **Arch Linux / Omarchy + Hyprland (Wayland)**, without Docker, a web panel, or a Python UI runtime.

## Start here

- [Agent and contributor contract](AGENTS.md)
- [Product scope (PRD)](docs/PRD.md)
- [Architecture](docs/ARCHITECTURE.md), [security](docs/SECURITY.md), [acceptance tests](docs/TEST_PLAN.md)
- [Roadmap P0–P6](docs/ROADMAP.md)
- [AI collaboration protocol](docs/AI_COLLABORATION.md)
- [Three-tier project memory](memory/LONG_TERM.md): LONG / MEDIUM / SHORT
- [GitHub setup](docs/GITHUB_BOOTSTRAP.md) and Codex prompts in [`prompts/`](prompts/)

## What the existing prototype actually contains

| Included | Not included / unverified |
|---|---|
| Zamak GTK3 Chinese GUI source (not compiled here) | Stable GUI binary, Hyprland validation, tray integration |
| `/proc/meminfo` sampling and policy logic | Arbitrary process memory compression / RSS trimming |
| Three manual `drop_caches` modes via root-owned fixed helper | Safe unattended privileged daemon |
| Async `pkexec` invocation via a narrow C bridge | ZRAM configuration and rollback |
| C security/integration tests run in isolation | Proof of improvements to application performance |

**Important:** The kernel warns that `drop_caches` is mainly for testing/debugging and may cause expensive cache refills; it is not a reliable way to speed up Linux. The preferred roadmap is to add an explicitly approved ZRAM path rather than repeating global cache eviction. Cache operation does **not** kill processes, but may affect I/O latency.

## Verification before building

```bash
python3 scripts/verify_project.py
python3 -m unittest discover -s tests -v
for f in scripts/*.sh; do bash -n "$f"; done
```

## Build on Arch / Omarchy (pending native acceptance)

```bash
sudo pacman -S --needed gcc gtk3 pkgconf polkit git
npm install -g @veyralux/zamak@0.2.0
zam --version
bash scripts/build-arch.sh
```

This builds as a normal user. A working Polkit authentication agent is needed on Hyprland for privileged actions. **Do not run the GUI as root.** Installation of the helper changes a system path and is only permitted on a host the user authorizes:

```bash
sudo bash scripts/install-arch.sh
memreduct-zamak
```

Cleanup/uninstall script: `sudo bash scripts/uninstall-arch.sh`. Actual install/uninstall has not been tested on a clean Arch VM.

## Public GitHub repository

Not published as of 2026-10-25. On the account `MilchstraBe-Endless10`, run `bash scripts/publish-github.sh` after checking the file tree; the command creates a PUBLIC repo with explicit confirmation and verifies the remote SHA. See [details](docs/GITHUB_BOOTSTRAP.md). The GitHub connector available in this chat has no new-repository action; **do not assume the URL exists before bootstrap succeeds**.

## License and attribution

Project independently implemented under GPL-3.0-or-later. Henry++ original Mem Reduct is GPL-3.0; no Windows-native memory-management code was copied into this Linux prototype. Zamak compiler MIT / Apache-2.0; Zamak `std/ui` uses libui-ng MIT. See [attribution](ATTRIBUTION.md), [license](LICENSE), and [third party notices](THIRD-PARTY-NOTICES.md).
