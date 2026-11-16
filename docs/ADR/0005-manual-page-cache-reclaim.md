# ADR-0005: Manual Linux Page-Cache Reclaim

- **Status:** PROPOSED — no owner approval and no production action enabled. The UI binding and the isolated GUI → authorization → helper loop now exist behind a build-time switch; this ADR does not treat that as approval.
- **Date:** 2026-10-28 (implementation note added 2026-10-31).
- **Scope:** Define the safety boundary for any future user-triggered Linux cache action. This ADR does not approve real kernel writes.

## Context

Mem Reduct 3.5.2 exposes eight Windows memory operations. None has a one-to-one Linux equivalent. The current Zamak main window keeps its clean button and Windows region commands disabled; the GUI does not call the existing `mr_start_reclaim()` bridge. The helper accepts only the fixed `pagecache` action and writes `1` to `/proc/sys/vm/drop_caches`; legacy `metadata` and `all` actions are rejected before opening the target.

The Linux kernel documents those values as requests to drop clean page cache, reclaimable slab objects, or both. Dirty objects are not freed. The kernel also says these caches are reclaimed automatically when memory is needed and warns that dropping them can cause significant I/O and CPU costs; use outside testing or debugging is not recommended. Running `sync` first can increase the objects eligible for dropping, but writes dirty data and is not an acceptable silent pre-step.

The cgroup v2 `memory.reclaim` file targets a particular cgroup subtree; it is not a whole-system memory-cleaning API. PSI is a pressure signal, not a reclaim command. `systemd-oomd` may kill cgroups, which is outside this product's allowed behavior.

## Proposed boundary

1. Do not map any Windows resource ID or region checkbox to `drop_caches`.
2. If an owner later approves an experimental manual action, the only candidate for a first implementation is an explicitly named Linux operation that requests clean page-cache drop (`1`) after an in-app warning and a separate Polkit authorization.
3. Do not issue `sync` automatically. Do not expose metadata/all modes from the UI. Do not add background, interval, startup, or threshold-triggered operation.
4. The confirmation must state that cached files may require slower re-reads, that the kernel may ignore dirty objects, and that no amount of freed RAM is guaranteed.
5. Report helper status and before/after `MemAvailable` observations with timestamps and a concurrency caveat. Never label the observed delta as bytes released by the operation.
6. Keep the GUI unprivileged; pass only fixed enum values to a root-owned helper with an absolute target path. Authorization cancellation and helper failure must remain distinguishable. No automatic retry after an uncertain write result.

## Current implementation change

This proposal's current code work only hardens the existing helper's result contract:

- production builds accept only `pagecache` and use the fixed `/proc/sys/vm/drop_caches` path;
- `MEMREDUCT_TESTING` builds alone may redirect to an absolute temporary path;
- symlinks and non-regular targets are rejected;
- `EINTR` before data transfer is retried once;
- short writes or close errors return exit code `3` and report an uncertain outcome;
- tests use a temporary file and injected write results; no kernel state is written.

This does not approve connecting the helper to the GUI for production use.

### P3B addition (2026-10-31): the loop is built and gated, not enabled

The GUI binding and the full state machine now exist, reachable only in a build configured with `MEMREDUCT_ENABLE_PRODUCTION_CLEAN`. A default build compiles the bridge but never calls it:

- `mr_production_clean_enabled()` answers the switch; a missing build flag cannot be turned on by an environment variable on a user machine.
- The main window asks that function before enabling the clean action, so the default build refuses the click at the button.
- Both backends share one result-state model and one wording function; `simulation` only selects the phrasing, never the safety claim.
- Simulation runs print `CI_CLEAN_SIM … no_system_write=true` and never measure bytes; the production wording omits `no_system_write` and reports `bytes_measured=false`, because the kernel accepts a request and does not report an amount.

Two isolated scripts exercise this without privilege:

- `scripts/ci/p3b-clean-simulation-xvfb.sh` drives real XTEST clicks through confirmation, mock authorization and a same-UID launcher to the result shown in the window, then asserts a second click does not silently retry.
- `scripts/ci/p3b-production-gate-xvfb.sh` sets `MEMREDUCT_CI`, leaves the simulation backend unselected, and asserts the same click is refused: no confirmation, no helper launch.

Neither script invokes `pkexec`, installs the helper, or writes kernel state. What remains unproven is exactly what the status line still says: real Polkit authorization, the installed root helper, and any real page-cache write.

## Alternatives considered

| Option | Reason not selected for a general cleaner |
|---|---|
| Map the eight Windows cleanup regions or expose metadata/all modes | Linux objects and Windows lists are not equivalent; the kernel warns about repeated cache drops. |
| Run `sync` before every reclaim | Can trigger system-wide writeback and additional I/O; it still cannot reclaim dirty objects synchronously with a guaranteed amount. |
| Write cgroup `memory.reclaim` | Only targets a chosen cgroup subtree and depends on cgroup delegation; it is not a system-wide “free RAM” command. |
| Watch PSI and clean automatically | PSI reports pressure; safe response should first release caches owned by this application. Automatic privileged operations remain out of scope. |
| Use systemd-oomd to free memory | Its configured actions can terminate cgroups, which this application must not do. |

## Evidence and references

- [Linux kernel `drop_caches` documentation](https://docs.kernel.org/admin-guide/sysctl/vm.html#drop-caches)
- [Linux kernel cgroup v2 memory interface](https://docs.kernel.org/admin-guide/cgroup-v2.html#memory-interface-files)
- [systemd resource pressure handling](https://systemd.io/PRESSURE/)
- [systemd-oomd behavior](https://www.freedesktop.org/software/systemd/man/latest/systemd-oomd.html)

## Required review before acceptance

- Product owner decides whether a diagnostic-only page-cache drop is worth exposing as a manual action.
- Security review checks Polkit policy, root-owned packaging, action cancellation, repeated invocation and result language.
- An isolated test must validate the complete GUI → Polkit → helper result state machine without writing the host kernel. Any eventual real-device test requires separate explicit authorization and is not part of ordinary CI.
