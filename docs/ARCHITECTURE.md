# Architecture / 技术架构 v1.0（Zamak）

> Status: Accepted design, existing code at prototype maturity. See `docs/ADR/` and `memory/MEDIUM_TERM.md`.

```text
                USER SPACE (UID = invoking user)
┌────────────────────────────────────────────────┐
│ Zamak native GUI (`std/ui` → libui-ng → GTK3) │
│ ├─ UI/controller: src/main.zm                  │
│ ├─ Snapshot/parser: src/memory.zm              │
│ ├─ Decision only: src/policy.zm                │
│ ├─ Settings/history: planned                  │
│ └─ Nonblocking C bridge: native/async_privileged.c│
└──────────────────────┬─────────────────────────┘
              Fixed action (enum; never a shell command)
                      ↓
                pkexec / Polkit
                      ↓
          PRIVILEGED PROCESS, ROOT-OWNED
┌────────────────────────────────────────────────┐
│ /usr/libexec/memreduct-zamak-helper            │
│ - validate args = pagecache | metadata | all │
│ - kernel drop_caches = 1 | 2 | 3              │
│ - return explicit error; exit                 │
└────────────────────────────────────────────────┘

Optional future: ZRAM status (read-only) → independently approved manager
           ↳ sysfs/zramctl, conflict probe, policy, rollback
```

## Domain boundaries
1. **Presentation**: window + commands only, no direct privileged writes; native Wayland support has to be proved on Hyprland (GTK may use XWayland).
2. **Application/domain**: sampling, policy, events and state. Deterministic functions tested with fixture `/proc/meminfo` inputs; no time-dependent hidden globals.
3. **Linux backend**: bridge to native `/proc`, `/sys`, `process_madvise` only when approved; dedicated privilege helper for kernel-write operations.
4. **Infrastructure**: packaging, Polkit integration, logs, CI, third-party notices, system integration.

## Source of truth
- Product requirements → `docs/PRD.md`.
- Trusted privilege model → `docs/SECURITY.md`.
- Accepted major choices → ADRs.
- Status and test truth → medium / short memory.

## Cache reclaim operation state machine
`IDLE → PREFLIGHT → AUTHORIZING → RUNNING → COMPLETED|FAILED|CANCELED → IDLE`.
- Distinguish `spawn success` from `helper success`.
- UI remains responsive; single operation max; the helper exits after each request.
- Authorization can be declined; never retry secretly. The user can cancel operations before authorization; no implicit replay.
- Display observed available RAM delta with timestamp and concurrent workload disclaimer. Preserve negative deltas.
- Instrument latency and error reason; no claims of exact reclaimed bytes.

## Memory data contracts
- `/proc/meminfo` values are KiB; convert to MiB only at presentation layer.
- `MemAvailable` approximates additional memory available for allocation; `MemFree` alone is misleading.
- `Cached + SReclaimable` includes objects that might not all be immediately reclaimable; rename UI label to **缓存占用估计**, not guaranteed reclaimable.
- `SwapTotal - SwapFree` is allocation estimate, *not* ZRAM compression savings.
- Any new metric must document its source (`/proc`, `/sys`, smaps) and validate missing/malformed values.

## Security boundary
- GUI never runs as root. Polkit helper must be system-installed with root ownership and mode 0755; do not package writeable privileged scripts.
- Actions must be constant enum values and fixed target paths, no arbitrary argument traversal and no `sh -c`.
- Check parent/child exit handling and session scope; include negative tests and CI non-privileged baseline.
- Automatic mode must not ask users for repeated privilege prompts or silently maintain privileged credentials.

## Dependences and portability
- Toolchain pin: `@veyralux/zamak@0.2.0`, C compiler, gtk3 + pkgconf + polkit. This is a hypothesis until build passes on target workstation.
- Zamak 0.2.0 is an early language/runtime; lock version and run CI compatibility checks before upgrading.
- `std/ui` on Linux uses GTK 3 / libui-ng. Wayland compositor integration is **not proven** in the current artifact.

## Options rejected or deferred
- **Python/PySide6 as main runtime**: replaced by Zamak to honor selected stack; retain no runtime Python requirement (Python only development tests).
- **Mem Reduct Windows API clone**: impossible on Linux as-is; Linux semantics must be designed separately.
- **Background drop_caches as default**: excluded; unpredictable I/O and performance penalties.
- **Direct arbitrary process reclaim**: deferred until privilege, availability, and process-boundary review.

## Referenced primary docs
- Zamak: https://github.com/VeyraLux/Zamak and `book/en/src/gui.md`.
- Mem Reduct: https://github.com/henrypp/memreduct.
- Kernel cache behavior: https://docs.kernel.org/admin-guide/sysctl/vm.html#drop-caches.
