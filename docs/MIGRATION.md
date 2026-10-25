# Migration from v0.1.0 Python/PySide6 to v0.2.0 Zamak

| Responsibility | Old | New |
|---|---|---|
| Desktop UI | Python + PySide6 | Zamak `std/ui` via native GTK 3 on Linux |
| Stats | Python reading `/proc/meminfo` | `src/memory.zm` |
| Auto policy | Python AutoPolicy | `src/policy.zm` |
| Privilege entry | Python root helper | root-owned, fixed-argument compiled C helper |
| GUI interaction | Blocking `subprocess.run(pkexec)` | non-blocking `posix_spawn`, `waitpid(WNOHANG)` via narrow FFI bridge |
| Packaging | Python venv in `/opt` | standalone Zamak native GUI + GTK3 shared libraries |
| Permissions | pkexec | pkexec, absolute binary path, fixed actions |

**Kept**: File cache / metadata cache modes, auto threshold policy and no process termination. **Not yet migrated**: persistent configuration, visual redesign beyond native controls, optional ZRAM compression configuration, cgroup reclamation, installable binary releases.

## Definition of done for public stable release

- `zam check`, `zam test`, and `zam build` pass on pinned toolchain.
- Native GUI boots under Arch / Hyprland and Polkit agent installed.
- Behavior tested on deliberately generated reclaimable page cache with before/after and latency metrics; no data loss and no process termination.
- ZRAM configuration guarded by state detection, rollback path, and unprivileged preview.
- Open-source licensing notices, binary provenance and dependency audits complete.
