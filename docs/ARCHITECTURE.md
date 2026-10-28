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

## Upstream UI fidelity contract

- The UI/UX source of truth is Henry++ Mem Reduct `v.3.5.2`, commit `2f5c55d15f69a2c88859e036b16c6f4c971e1968`. Reproduce the compact layout, resource IDs, menu tree, five settings pages, tray actions, and keyboard/mouse state transitions; do not redesign or enlarge the product UI. Preserve the official main-dialog (224×214 DLU) and settings-page template (260×168 DLU each) geometry after measured font/DPI conversion; the 336×348 CI probe is test-only.
- Keep English and Simplified Chinese as independent locales; do not concatenate both languages in a label. Complete Simplified Chinese remains a product goal, but each upstream untranslated string must retain its source text and proposed Linux translation for an owner decision. The 12 P2C source-language differences remain `DECISION_REQUIRED`; preserve the current translations until that decision is made.
- Win32 dialog dimensions are Dialog Units, not pixels. Pixel targets require a measured Windows 3.5.2 font/DPI conversion and a real client-area capture; GTK defaults and an estimated test-probe size are not goldens.
- Use Zamak `std/ui` for the native shell/lifecycle, Canvas for deterministic client-area geometry, and only narrow C/GTK bridges for measured gaps such as font selection or desktop integration. Preserve focus, keyboard navigation, accessibility, and explicit hit regions.
- This visual contract does not assert Linux equivalence for Windows memory APIs. Keep the per-operation Linux mapping and privilege boundary in `docs/UI_LINUX_COMPATIBILITY.md`; never map all Windows cleanup choices to one `drop_caches` action.
- Without authentic paired Windows/Linux screenshots and measured overlays, pixel acceptance remains blocked/not run.

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
