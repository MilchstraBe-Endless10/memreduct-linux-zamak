# ADR-0005: Manual Linux Page-Cache Reclaim

- **Status:** PROPOSED — no owner approval, no UI binding, and no production action enabled.
- **Date:** 2026-10-28.
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

This does not approve connecting the helper to the GUI. The main-window action remains disabled until this ADR is reviewed and the operation's product value is accepted against the kernel's warning.

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
