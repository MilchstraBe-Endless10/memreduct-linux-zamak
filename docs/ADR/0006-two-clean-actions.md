# ADR-0006: Two Clean Actions — Page Cache and Process Workingset

- **Status:** PROPOSED — implemented behind build-time switches, no production action enabled.
- **Date:** 2026-10-31.
- **Supersedes:** the single-action assumption in ADR-0005. ADR-0005's page-cache boundary is unchanged and still governs that action.
- **Scope:** Define what the Linux build can honestly offer as "clean memory", split into two separate user actions with separate guarantees.

## Context

Mem Reduct 3.5.2 on Windows calls eight `NtSetSystemInformation` operations. The heaviest is `MemoryEmptyWorkingSets`, which forces every process's working set out to the page file. That is what makes Task Manager's number drop by gigabytes, and it is what a user comparing the Linux build to the original actually expects.

The Linux build currently ships one action: a request to drop clean page cache through `/proc/sys/vm/drop_caches`. It is implemented end to end and disabled by a compile-time switch. It does not touch process memory at all, so it cannot reproduce the effect the original is known for.

Measured on this machine (kernel 7.2.8, `CapEff=0000000000000000`):

| Probe | Result |
|---|---|
| `madvise(self, MADV_PAGEOUT)` | success |
| `process_madvise(pidfd_open(self), MADV_PAGEOUT)` | success; `RssAnon` 65640 kB → 108 kB |
| `process_madvise(pidfd_open(other same-UID process), …)` | `EPERM` |

The decisive fact is the third row combined with the second: reclaiming **your own** pages needs no privilege, and reclaiming **another process's** pages is refused even when that process runs as the same user. There is no middle setting short of `CAP_SYS_NICE` (or root).

## Decision

Expose two distinct actions, never one merged "clean memory" button that silently does whichever is available.

### Action 1 — Page cache (basic)

- Existing behaviour, unchanged: write `1` to `/proc/sys/vm/drop_caches` through the fixed helper.
- Effect: clean page cache and reclaimable slab are dropped. Anonymous memory, working sets and swap are untouched.
- Cost: subsequent file reads hit disk again. The kernel documents this as a debugging aid, not an optimisation.
- Guarantee: none about how much memory is freed. The kernel accepts a request; it does not report an amount.

### Action 2 — Process workingset (advanced)

Two sub-modes, because the permission boundary splits them:

- **Self-reclaim, unprivileged.** The application may call `process_madvise(pidfd_open(getpid()), …, MADV_PAGEOUT)` on its own pages. Measured to work with zero capabilities. This frees only the pages of this process, which is a small and honest win.
- **System-wide reclaim, privileged.** Iterating other processes requires `CAP_SYS_NICE`. This goes through the same root helper as action 1, with the same fixed-argument discipline.

The privileged mode is not implemented in this ADR's first pass. What it requires is decided here, and the second section below records the open questions.

### Naming in the interface

The two actions must never share a label. Draft wording, to be reviewed with the UI:

| Action | English | 简体中文 |
|---|---|---|
| Page cache | Clean page cache | 清理页缓存 |
| Workingset (self) | Trim this app's memory | 释放本程序内存 |
| Workingset (system) | Trim all processes' memory | 释放所有进程内存 |

No label may say "free memory", "release RAM", or carry a number. The interface reports observed `MemAvailable` before and after with a timestamp and an explicit non-attribution note, exactly as ADR-0005 already requires.

## Permission model for the privileged mode

Open questions that must be answered before it is built:

1. **How is `CAP_SYS_NICE` granted?** Options: a file capability on the helper (`setcap cap_sys_nice+ep`), a systemd unit with `CapabilityBoundingSet`, or keeping the helper root-only and letting Polkit's existing authorization cover it. The last is closest to what already ships and adds no new sudoers or capability surface.
2. **What is the enumeration?** The helper must never accept a PID or a path from the caller. It walks `/proc` itself, skips PID 1 and kernel threads, and applies a size threshold. The fixed enum discipline from ADR-0005 applies unchanged.
3. **Is pushing a user's pages into swap acceptable?** On this machine zram is 10.6 GB of 15 GB used; pushing more anonymous pages there converts a memory problem into a swap-subsystem problem. This is a product decision, not an engineering one.
4. **What happens to a process that is mid-write?** `MADV_PAGEOUT` on dirty pages forces writeback. The helper must skip processes whose pages cannot be safely reclaimed, and must report how many it skipped.

## Consequences

- The interface gains a third and possibly fourth action, which the original does not have. That is a deliberate divergence: the original's `MemoryEmptyWorkingSets` has no single Linux equivalent, so the Linux build states what each action actually does instead of pretending one button covers it.
- Action 1 alone will keep disappointing a user who expects the original's effect. Stating this in the interface is the honest response.
- Action 2's self-reclaim path is the only memory-releasing capability that works with no privilege at all, so it should ship first.
- Every measurement stays observational. No action may report "freed N MB".

## Alternatives considered

| Option | Reason not selected |
|---|---|
| One button that picks whichever action is available | Two actions with different costs and guarantees would share one label, which is exactly the ambiguity ADR-0005 was written to prevent. |
| Only implement the privileged system-wide workingset trim | Requires `CAP_SYS_NICE` before anything is usable, and pushes anonymous pages into an already-full zram. |
| Use cgroup `memory.reclaim` | Targets one subtree, depends on delegation, and is not a whole-system action. Already rejected in ADR-0005. |
| Map `DONTNEED` onto working sets | Drops clean file-backed pages only; anonymous pages stay resident. It would look like a trim and not be one. |

## Evidence

- `process_madvise(2)`, `madvise(2)` — kernel man pages; `MADV_PAGEOUT` requires the target's `PTRACE_MODE_READ_FSCREDS` and `CAP_SYS_NICE` for processes other than the caller.
- Local probes recorded in this repository's test output: self vs same-UID-other authorisation, and the `RssAnon` drop above.
- [Linux kernel drop_caches](https://docs.kernel.org/admin-guide/sysctl/vm.html#drop-caches)
