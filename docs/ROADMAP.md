# Roadmap / Implementation order

## P0 — Governance and GitHub (current)
Deliver `AGENTS.md`, memory, PRD, architecture, ADRs, security, test plan, PR/Issue templates, local source migration. Exit: public repo created (not yet), `main` pushed and verified SHA, first tracker Issues created.

## P1 — Compile the existing Zamak project **before adding features**
Fix `zam check .`, `zam test .`, native GUI build, make CI reproducible. On Omarchy/Hyprland open window and inspect Polkit baseline. Exit: first verified GUI screenshot / logs, no untruthful build assertions.

## P2 — Data and usability correctness
Fix missing meminfo keys, capacity boundary conditions; UI labels accurately distinguish caches vs ZRAM; Chinese strings and errors; freeze-free refresh. Exit: fixture tests and recorded UI smoke.

## P3 — Manual reclaim, least privilege
Harden `pkexec` helper, launch result, cancel/error; characterize actual cache-drop behavior in isolated host. Exit: action and negative-path matrix passing, no process termination.

## P4 — Safe automation, persisted settings
Author ADR for unattended privilege permissions; default-off, cooldown/backoff, pressure trigger, cancel and history; do not use an endless stream of `pkexec` dialogs. Exit: malicious/edge-case regression tests and UI stop switch.

## P5 — Compression, smarter strategies
Add **read-only** current ZRAM status; separate ZRAM management proposal with conflict detection, no blind swapoff; evaluate cgroup reclaim only with explicit allowlist. Exit: rollback and safety evidence, actual improvement vs control benchmark.

## P6 — Release and community feedback
PKGBUILD first, then AppImage if GUI dependencies and libraries can be packaged correctly; issues, CHANGELOG, reproducible checksums, GPL/third-party notices. Gate: P1–P5 complete to scope and actual platform verified.

Stop rule: If P1 native toolchain cannot build, fix the toolchain or reduce experimental UI scope; **do not** rewrite core in another language without a user-approved ADR.
