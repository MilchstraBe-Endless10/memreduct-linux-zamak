# ADR-0007: Bounded Cross-Process Workingset Advisory (Prototype)

- **Status:** PROPOSED — planning/discovery only. No production activation. The writable path exists in code but is disabled at compile time *and* by a runtime approval gate; nothing in this ADR authorizes running it.
- **Date:** 2026-10-31.
- **Relates to:** ADR-0005 (page cache, unchanged), ADR-0006 (two clean actions; self-reclaim and the open questions about a privileged mode).
- **Scope:** Define whether this project can honestly offer anything resembling Windows `MemoryEmptyWorkingSets`, and if so under which boundary. ADR-0006 leaves the privileged mode unimplemented and lists four open questions; this ADR answers them for a prototype and refuses production activation.

## Context

Issue #12 asks for a cross-process memory reclaim prototype with strict boundaries. Three facts from ADR-0006 and the measurements recorded there govern the whole design:

1. `native/self_reclaim.c` pages out **its own** mappings with `process_madvise(pidfd_open(getpid()), …, MADV_PAGEOUT)`. It was measured to work with `CapEff=0`. That number ("359 MB paged out") is a self-reclaim result and is **not** evidence of any system-wide process-memory reclaim.
2. `process_madvise` against another same-UID process returns `EPERM` on this machine. Linux checks `PTRACE_MODE_READ_FSCREDS` on the target and requires `CAP_SYS_NICE` (added in 5.12) for any target other than the caller.
3. There is no Linux "System working set" counter. That is a Windows metric. Summing RSS across processes would produce a number with that name and none of its meaning.

ADR-0005 approved a page-cache design only. ADR-0006 kept it valid and split "clean memory" into two named actions. A new capability therefore needs its own ADR and threat model — this one — and still must not be activated in production this round.

## What the kernel actually offers

`process_madvise(2)`, since Linux 5.10:

```
ssize_t process_madvise(int pidfd, const struct iovec *iov, size_t vlen,
                        int advice, unsigned int flags);
```

- `pidfd` comes from `pidfd_open(2)`, which pins an identity rather than a number: a reused PID cannot be advised through an old pidfd.
- `vlen` is capped by `UIO_MAXIOV` (1024) and by `/proc/sys/vm/process_madvise_max_iovec` where present. Each call is one permission check.
- **The return value is the number of bytes the call covered, not bytes reclaimed.** A short return means a partial application: the kernel stopped inside the vector. Treating that number as "freed" is the exact dishonesty this ADR forbids.
- Failure modes must stay distinguishable: `EPERM` (no permission for this target), `ESRCH` (process gone), `EINVAL` (bad range/advice/flags), `ENOSYS` (kernel or seccomp lacks the syscall), `EBADF`/`ENOTDIR` (not a pidfd for a thread-group leader).

Advice values in scope:

| Advice | Meaning | Cost |
|---|---|---|
| `MADV_COLD` (20) | Mark pages inactive so reclaim finds them first. Pages stay resident. | Small CPU; may slow the target on next access. No guaranteed release. |
| `MADV_PAGEOUT` (21) | Try to reclaim now: clean file pages dropped, dirty pages written back, anonymous pages swapped. | I/O and CPU in the target's context; swap consumption; latency spikes. |

Nothing here can force every process's working set to zero. The closest real mechanism, cgroup v2 `memory.reclaim`, targets one subtree and needs delegation — already rejected in ADR-0005. A global sweep of all processes requires privilege this product refuses to take.

## Decision

### 1. No "System working set" number, ever

The Linux build will not publish a metric named after a Windows counter. It may report, separately and with timestamps: `MemAvailable`, `SwapUsed`, and per-target advisory byte counts labelled "bytes advised" — never "bytes freed". No summed-RSS field is introduced.

### 2. Ship the planner, not the action

The deliverable of this round is an unprivileged, read-only planner (`native/crossproc_plan.c`, `native/crossproc_plan.h`):

- Enumerates candidates from a procfs root. Production root is the fixed `/proc`; a mock root is compile-time test-only (`MEMREDUCT_TESTING`).
- Fixed enums for advice and result state. The caller cannot pass a PID, a path, a raw range, or a raw advice number.
- Deterministic target/region selection under hard ceilings: at most 8 targets, 16 regions per target, 32 iovecs per batch, 64 MiB per region, 256 MiB total, 250 ms budget, and a bounded readdir of at most 256 `/proc` entries. There is no unbounded global sweep.
- Skips PID 1, the caller itself, kernel threads (no `maps`), other UIDs when unprivileged, non-private mappings, writable file-backed mappings (dirty pages), and special mappings (`[vdso]`, `[vvar]`, `[stack]`).
- Pins each target with `pidfd_open`, and re-reads `/proc/<pid>/stat` field 22 before and after to detect PID reuse. A exited target is reported as `SKIP_EXITED`, a reused one as `SKIP_PID_REUSED`; neither becomes a plan entry.
- Returns a stable status enum including `MR_CROSSPROC_PERMISSION_DENIED` — a lack of authority is a failure, not something to route around.

### 3. Writable path: double disabled

`native/crossproc_advise.c` contains the complete `process_madvise` call, including errno classification and partial-result handling, but:

- it is compiled out unless `MEMREDUCT_ENABLE_PRODUCTION_ADVISE` is defined at build time;
- even in that build it refuses unless the GUI has armed a one-shot user approval (`mr_crossproc_arm_approval`), which no current caller does;
- it never retries an uncertain result, never signals or kills a target, and is re-entrancy protected.

Tests exercise the full syscall shape through an injectable backend (`mr_crossproc_set_backend`) available only in `MEMREDUCT_TESTING` builds, and through a private throwaway child process the test itself creates and kills. No other process on the host is ever advised, no `setcap` is used, and the GUI never runs as root.

### 4. Honest interface wording

The GUI contract is header-only (`native/crossproc_plan.h` plus the wording constants in `native/crossproc_gui.h`); `src/main.zm` is untouched because the Integrator owns it. Required user-visible statements:

- the action is experimental and separate from page-cache cleaning;
- it may cause disk I/O, swap use, CPU work, and visible stalls in the target;
- it cannot promise an amount released, because the kernel only accepts a request;
- targets it cannot reach (other users, PID 1, kernel threads, protected processes) are skipped, not forced.

## Threat model

| Threat | Control |
|---|---|
| GUI asks the kernel to page out a process the user did not intend | No caller-supplied PID/path/range; fixed scan policy; explicit per-target listing before any action |
| PID reuse sends advice to the wrong process | pidfd pinning + `start_time` recheck; mismatch aborts the target |
| Privilege creep to make "all processes" work | No `setcap`, no root GUI, no `CAP_SYS_NICE` deployment in this round; unprivileged path only |
| Host memory/IO damage during tests | Real syscall reachable only through the test backend or a private child; production path not built by default |
| Partial or failed advice reported as success | Return value is bytes advised; short returns and each errno become distinct states; no automatic retry |
| Re-entrancy or concurrent runs doubling the cost | Single static guard returns `MR_CROSSPROC_REENTRANT`; approval is one-shot |
| Budget evasion by a hostile caller | Limits are compile-time constants, clamped again at build time |

Residual risk: `MADV_PAGEOUT` on another process with `CAP_SYS_NICE` can force writeback and swap for that process, causing latency spikes. This cannot be eliminated by the prototype; it is why production activation remains `OWNER_APPROVAL_REQUIRED`.

## Comparison of the three Linux candidates

| Property | `drop_caches` (pagecache helper) | Self-reclaim (`self_reclaim.c`) | `process_madvise` on another process |
|---|---|---|---|
| Privilege | root / Polkit helper | none | `PTRACE_MODE_READ_FSCREDS` + `CAP_SYS_NICE` (5.12+) |
| Scope | clean page cache + reclaimable slab | caller's own private mappings | explicitly listed mappings of explicitly pinned targets |
| Affects other processes | yes, indirectly (their cached files are dropped too) | no | yes, directly |
| Reports an amount | no | bytes advised | bytes advised |
| Typical cost | re-read from disk | small | writeback, swap, target stalls |
| Production status here | behind `MEMREDUCT_ENABLE_PRODUCTION_CLEAN`, ADR-0005 | available, ADR-0006 action 2 | not built by default; prototype only |

## Consequences

- The project can demonstrate and test the complete advisory path without touching the host, which is the honest maximum for this round.
- The user still cannot get the original's "everything drops to zero" effect, and the interface must keep saying so.
- Activating the writable path requires: an explicit product-owner decision, a security review of the capability grant, and a real-device test with separate written authorization.

## Alternatives considered

| Option | Rejected because |
|---|---|
| Call `MADV_PAGEOUT` on every process as root | Privilege escalation with system-wide latency risk; no honest amount to report. |
| Publish summed RSS as "system working set" | Fabricates a Windows metric on Linux; violates AGENTS.md §1. |
| Iterate `/proc` with an unbounded loop | A global sweep with no ceiling; target enumeration cost is attacker-controllable. |
| Accept a PID from the GUI | Loses the fixed-enum discipline of ADR-0005; enables advising an arbitrary process. |
| Use `MADV_DONTNEED` on other processes | Only drops clean file-backed pages, looks like a trim without being one; ADR-0006 already rejects it. |

## Evidence and references

- `process_madvise(2)`, `pidfd_open(2)`, `madvise(2)` — Linux man pages; `MADV_COLD` since 5.4, `MADV_PAGEOUT` since 5.4, `process_madvise` since 5.10, `CAP_SYS_NICE` requirement for non-self targets since 5.12.
- Measurements recorded in ADR-0006 (self success, same-UID other `EPERM`).
- [Linux kernel `drop_caches`](https://docs.kernel.org/admin-guide/sysctl/vm.html#drop-caches)
- [Linux kernel cgroup v2 memory interface](https://docs.kernel.org/admin-guide/cgroup-v2.html#memory-interface-files)

## Required review before any activation

- Product owner: is advising other processes at all acceptable, given swap/IO/latency costs and no guaranteed release?
- Security review: no capability is granted in this round; if one is ever proposed, it needs its own ADR amendment and threat-model update.
- Real-device test plan with explicit written authorization, on a throwaway same-UID child process only.
