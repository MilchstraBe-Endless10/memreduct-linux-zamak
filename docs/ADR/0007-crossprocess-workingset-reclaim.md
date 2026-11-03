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

---

## Amendment: M1.1 — the separate privileged helper (2026-11-02)

- **Status:** M1.1 code complete and locally verified. **Not installed. No real cross-process syscall issued.**
- **Scope:** implements the option this ADR previously left as "not built by default", without granting the GUI any privilege.

### What was built

| Unit | Role | Privilege |
|---|---|---|
| `native/crossproc_helper.c` | The only privileged action. One verb, three decimal numbers, one syscall. | root + `CAP_SYS_NICE`, via its own Polkit action |
| `native/crossproc_candidates.c` | Enumerates the user's own processes. Read-only. | none |
| `native/async_crossproc.c` | Spawns the helper through `pkexec` with a literal argv. | none |
| `production_crossproc_gate_{default,enabled}.c` | Build-time capability gate, separate from the page-cache one. | none |
| `...memreduct.zamak.crossproc.policy` | Distinct action id and `exec.path` from the page-cache action. | authorization |

### The helper re-derives every decision

The PID the GUI sends is an untrusted candidate. For a root-target action, `pkexec` calls `setreuid(0, 0)` before executing the helper, so the expected real and effective UIDs are both 0; requiring them to differ would reject a valid `pkexec` launch. The helper requires `ruid=0`, `euid=0`, effective `CAP_SYS_NICE`, and a strictly parsed, nonzero `PKEXEC_UID`. That environment value identifies the claimed requester but is not treated by itself as proof that a target is safe. The helper independently reads the target's kernel `Uid:` tuple and requires real/effective/saved/filesystem UIDs to agree, be non-root, and match the requester claim. It also verifies `start_time`, pins the process with a pidfd and rechecks its identity, rejects PID 1, itself, and its own ancestors, reads `/proc/<pid>/exe` without accepting a truncated or unknown path, checks the shared protected executable/name rules, and accepts only a matching user's supported application cgroup. Missing, malformed, protected, or unsupported process identity data fails closed. Only then does it read the target's maps itself, choose ranges itself, and issue **one** `process_madvise(MADV_PAGEOUT)` call. No retry, no signal, no shell, no path argument, no address range from outside.

### Two defects this round found by running it

Both were found by exercising the code against this machine, and both are now regression-tested:

1. **Truncated `comm` bypassed the protected list.** The kernel caps `comm` at 15 bytes, so `/usr/bin/dbus-broker-launch` reports as `dbus-broker-lau` and an exact comparison let a desktop service through as a reclaim target. Matching is now truncation-aware, the long name is listed explicitly, and `/proc/<pid>/exe`'s basename is checked as a second, untruncated source. The helper and the enumeration apply the identical rule, and a test fails if the two lists drift.
2. **A refusal could report "no refusal".** `mr_xproc_start()` cleared the local reason on entry, and a refusal path could return before setting it, so the caller saw `0` — the value that means "none". Every path now sets a distinct reason with a stable token.

A third issue is a design correction rather than a bug: `mr_xproc_test_classify()` had been exported from every build. It is now compiled only under `MEMREDUCT_TESTING`, and a symbol scan asserts its absence, because a GUI-reachable classify entry point would let a caller decide what the privileged side "said".

### Owner security review correction (2026-11-02)

- The first credential predicate incorrectly required real and effective UIDs to differ as evidence of `pkexec`. For a root target, `pkexec` calls `setreuid(0, 0)`, so the helper now accepts only the expected root/root model, requires effective `CAP_SYS_NICE`, and strictly parses a nonzero `PKEXEC_UID` requester claim. The claim alone is not authorization evidence: the helper independently requires the target's real/effective/saved/filesystem UIDs to agree, be non-root, and match that claim.
- A 15-byte `comm` match is not a sufficient target boundary. Both enumeration and helper now require a complete `/proc/<pid>/exe` path, apply the shared protected-name rules, validate the target UID tuple and accept only a supported application scope for the same user. Missing, truncated, malformed, protected, or unsupported identity data is skipped/refused. The helper repeats these checks after opening the pidfd and verifies the PID start time.
- The Arch installer checks the source policy's dedicated action ID and fixed helper path before staging, then verifies the installed policy is byte-for-byte the checked source, repeats the action/path checks against the installed file, and requires `pkaction --action-id` to recognize the dedicated action. The installer was not run in this review because installation is not approved yet.
- Regression coverage models `ruid=0/euid=0`, rejects mismatched requester/target UIDs and missing capability, and exercises protected, missing, and unsupported process identity/cgroup cases. All tests use fixtures or a stub syscall backend; no real cross-process syscall is issued.
- **Post-correction local verification:** governance 34/34; Python suite 186 passed, 1 existing filesystem-length case skipped; Zamak 0.2.0 `check` passed, `test` 76/76, `build` passed; Arch native build and all shell syntax checks passed. The installed cross-process action is not present in this host's `pkaction` registry because installation was not run; no installed Polkit behavior is claimed.

### Honesty invariants kept

- The result block prints `bytes_advised_is_not_bytes_freed=true`, and no status token contains the word "freed".
- `advised == 0` cannot be reported as accepted; more than one call, a truncated block, a duplicate key, an unknown token, or an exit code disagreeing with the report all resolve to `OUTCOME_UNKNOWN`.
- A build without the gate enabled refuses locally with `not_enabled` rather than spawning anything.

### Initial M1.1 verification before owner review correction (historical snapshot)

167 Python tests (1 skipped: a >PATH_MAX path the filesystem rejects), 76 Zamak tests, `zam check`, GUI build, all shell syntax. Live enumeration against this host: 512 procfs entries scanned, 32 candidates, 7 protected skipped. The production binary contains no `MEMREDUCT_TEST_*` switch and no `mr_xproc_test_*` symbol.

### What is still required before any real reclaim

1. The product owner authorizes installing the root-owned helper and its Polkit action.
2. The product owner authorizes one interactive, authenticated `MADV_PAGEOUT`.
3. Only then can M1.2 (controlled real call) and M1.3 (everyday applications) be claimed. Neither is claimed now.

---

## Amendment: M1.2 owner-reported success and M1.3 local candidate (2026-11-03)

- **M1.2 status:** The owner reported one authenticated real `process_madvise(..., MADV_PAGEOUT, 0)` call against a project-created child process. The helper reported `advised_bytes=1048576`, `error_code=0`, and one call; target residency was observed as 256/256 pages before and 0/256 after. The child remained alive and its data passed a later integrity check; temporary resources were removed. Source record: [Issue #12 M1.2 acceptance](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/issues/12#issuecomment-6599614790). This is owner-provided execution evidence, not a new run or independent binary audit in this worktree. The accepted 1 MiB advice and page residency change do **not** prove 1 MiB net system RAM was freed.
- **M1.2 authorization:** The one-call authorization is consumed. No M1.2 syscall was repeated in this worktree, and it does not authorize a daily-application target.
- **M1.3 implementation checkpoint:** Base code commit `3bd2a7f1b3bcb975bac3d31302fc146e61475bee` adds a read-only current-user application picker, identity/state display without full executable paths, initially empty target selection, explicit single-target confirmation, bounded request size, risk wording, the existing fixed-helper bridge, and separate result reporting for authorization/target/helper outcomes. Follow-up `e32c2ba3b802a6b90c696a2af127d04a5fcf033e` distinguishes mock results from kernel outcomes; `74574a93c4059946ddc446e7787efa0a8a0e9adc` fixes the GTK titlebar attach lifecycle. The ordinary build keeps the cross-process gate disabled; a separate review build enables only this action while leaving the page-cache gate disabled.
- **Nonprivileged verification:** governance 34/34; Python 190 passed with one existing long-path filesystem-limit skip; Zamak 0.2.0 `check`, 80/80 tests, and build passed; 20 Shell scripts parse; default GUI, opt-in GUI, and helper builds passed. The Xvfb M1.3 simulation passed with two fixed fixtures: mouse/keyboard selection, confirmation cancellation, mock authorization/helper, partial-result display, Escape/titlebar close, exit 0. The log reports `call_count=0`, `no_syscall=true`, and `no_system_write=true`; it does not enumerate live processes or start Polkit. The test now rejects GTK/GDK/GLib warnings and criticals; the post-fix run emitted none. Evidence (including PNG SHA-256) is in the AI output directory recorded in `memory/SHORT_TERM.md`.
- **Remaining M1.3 acceptance:** The simulation does not verify real same-user process enumeration, installed-helper invocation, Polkit behavior, or a daily-application kernel result. Obtain separate target-specific authorization before any real daily-application advice. Do not equate advised bytes to bytes freed. No additional real syscall was run in this worktree.
- **Harness issue:** Issue #12 records an outer orchestration script that misreported normal `SystemExit(0)` as exit code 2. No such orchestration source is present in this worktree or the inspected `p3c-integrate` worktree; its available untracked M1.2 runner tests use `subprocess.run(...).returncode` and contain no such exception mapping. The source could not be repaired or tested here, and the real syscall was not repeated.

ADR status remains **PROPOSED**; this amendment records observed results and a local implementation checkpoint, not production approval or release readiness.
