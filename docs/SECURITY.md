# Security policy, threat model, and privilege requirements

## Assets and threat boundaries
- User processes/data, running desktop session and availability.
- Kernel memory control interfaces; host root privilege; helper executable path and polkit session.
- Public source/release pipeline; no real passwords or local identifying paths in any commit.

## Risks / mitigations
| Threat | Mitigation | Evidence gate |
|---|---|---|
| GUI elevates arbitrary action | page-cache helper accepts only fixed `pagecache`; cross-process helper accepts one bounded target identity and derives mappings itself; neither accepts shell text | invalid/legacy args tests + manual review |
| Helper executable replaced | root-owned absolute path; verify mode/owner after install | packaging E2E test |
| Authorized action unexpectedly repeats | no background default; per-action state + cooldown | P4 tests + UX review |
| Cache eviction harms latency | product warning; opt-in only, never as best-performance claim | perf comparison (cold re-read) |
| ZRAM conflict or swapoff OOM | read-only first; no forced swapoff; rollback plan | separate ADR + user authorization |
| Race between worker and GUI | one operation, detect child exit, no busy-loop | stress tests + cancel/deny |
| Misleading freed-memory number | report observed `MemAvailable` delta only | snapshot fixture, concurrent load caveat |
| Leaked secrets in public repo | secret scan and review each staged diff | pre-push scan |

## Policy for privileged writes
- No automatic writes to `/proc/sys/vm/drop_caches` on unattended CI or developer machine.
- No unattended manipulation of ZRAM/swap; no arbitrary PID, address, mapping, executable path or cgroup path. M1.3 may pass one manually selected PID/start-time candidate to the fixed cross-process helper, which must independently revalidate UID, executable, cgroup, start time, pidfd, protected-process rules and its own bounded mappings. No batch target list is accepted.
- Document command, rollback, permissions, output, and expected performance impact before approved new capability.
- No blanket claims “safe / stable / no impact”; user intent defines design target, not proved result.

## Helper write result contract

### Page-cache helper

- The only accepted production operation is `pagecache` (`drop_caches=1`). `metadata` and `all` are rejected before any path is opened.
- The production helper writes only the fixed `/proc/sys/vm/drop_caches` target. The test-only `MEMREDUCT_TESTING` build can redirect to a regular temporary file; production builds ignore that environment variable.
- A full write with exit code `0` means the kernel accepted the request. It does not establish how many bytes were reclaimed.
- An interrupted write that reports `EINTR` before transferring data is retried once. A short write or close failure returns exit code `3` (`outcome uncertain`); callers must not retry automatically because the operation may already have taken effect.
- Regression tests exercise the operation against temporary files and injected `EINTR`/short-write results. No test writes to `/proc/sys/vm/drop_caches`.

## Historical production blockers snapshot (2026-10-29)
- Local code HEAD `640fb5df044475df827308388c3da5be25d4ffad` passes Zamak 0.2.0 check/test (44 tests), C/Python safety (12 tests), governance (34 checks), and production GUI/helper build. The isolated Canvas probe passes 12/12. These are local results, not new GitHub checks.
- PR #7's published GitHub CI is attached to its remote HEAD, not to local-only changes.
- Fake-launcher tests now verify asynchronous bridge exit-code propagation and no retry for statuses 1, 3, and 126. Polkit authorization/denial/cancellation in a real Hyprland session and the actual privileged cache action remain unverified.
- No real `/proc/sys/vm/drop_caches` write, Swap/ZRAM change, root GUI, or live desktop input was performed.

## Cross-process advisory security checkpoint (2026-11-03)

- Owner-reported M1.2 evidence records one authenticated `process_madvise(MADV_PAGEOUT)` call against a project-owned 1 MiB test child. The kernel accepted 1 MiB of advice and the target mapping was observed nonresident afterward; the child survived and data integrity passed. This is not a measurement of net RAM freed and is not evidence for a daily application. The original record is [Issue #12](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/issues/12#issuecomment-6599614790).
- The one-call authorization was consumed. This checkpoint did not invoke Polkit, the helper, `process_madvise`, page-cache writes, Swap/ZRAM changes, or any root GUI.
- Local M1.3 code commit `3bd2a7f1b3bcb975bac3d31302fc146e61475bee` adds a read-only picker for eligible current-user app scopes, empty initial selection, explicit one-target confirmation, risk wording, and result display. The helper continues to re-derive identity and eligible ranges. The normal GUI build has the cross-process gate disabled; a separate opt-in review build keeps page-cache cleaning disabled.
- Nonprivileged tests/build passed locally: 190 Python tests (one existing long-path skip), Zamak 79/79 plus check/build, Shell syntax and diff checks. Isolated Xvfb interaction was NOT RUN because Xvfb and its input tools are unavailable; neither binary was launched and no live process list was read.
- Daily-app use is not yet accepted. Require isolated GUI interaction evidence and a separate explicit authorization naming the target before any daily-app syscall. Keep the approved `auth_admin` policy, fixed helper path, per-request Polkit flow, no automatic retry, and the distinction `bytes advised != bytes freed`.
