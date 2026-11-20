# Security policy, threat model, and privilege requirements

## Assets and threat boundaries
- User processes/data, running desktop session and availability.
- Kernel memory control interfaces; host root privilege; helper executable path and polkit session.
- Public source/release pipeline; no real passwords or local identifying paths in any commit.

## Risks / mitigations
| Threat | Mitigation | Evidence gate |
|---|---|---|
| GUI elevates arbitrary action | page-cache helper accepts only fixed `pagecache`; Crossproc accepts one bounded target identity and a fixed mode (`pageout` or exact `app_scope`), derives the target/cgroup itself, and accepts no shell text or path | invalid/legacy args tests + manual review |
| Helper executable replaced | root-owned absolute path; verify mode/owner after install | packaging E2E test |
| Authorized action unexpectedly repeats | no background default; per-action state + cooldown | P4 tests + UX review |
| Cache eviction harms latency | product warning; opt-in only, never as best-performance claim | perf comparison (cold re-read) |
| ZRAM conflict or swapoff OOM | read-only first; no forced swapoff; rollback plan | separate ADR + user authorization |
| Race between worker and GUI | one operation, detect child exit, no busy-loop | stress tests + cancel/deny |
| Misleading freed-memory number | report observed `MemAvailable` delta only | snapshot fixture, concurrent load caveat |
| Leaked secrets in public repo | secret scan and review each staged diff | pre-push scan |

## Policy for privileged writes
- No automatic writes to `/proc/sys/vm/drop_caches` on unattended CI or developer machine.
- No unattended manipulation of ZRAM/swap; no arbitrary PID, address, mapping, executable path or cgroup path. M1.3 passes one manually selected PID/start-time candidate to the fixed helper, which independently revalidates UID, executable, cgroup, start time, pidfd and protected-process rules. `pageout` derives its bounded mappings from procfs. `app_scope` derives the exact user application cgroup from that PID, rejects child cgroups, verifies each current member has the same requester UID and scope, and writes the requested byte ceiling to that scope's `memory.reclaim` exactly once. Neither mode accepts a batch target list. The hard ceiling is 1 GiB; the UI further limits it to sampled RSS or `anon + file` cgroup charge. These values bound a request and do not guarantee reclaimed RAM.
- Document command, rollback, permissions, output, and expected performance impact before approved new capability.
- No blanket claims “safe / stable / no impact”; user intent defines design target, not proved result.

## R5 large-request checkpoint (2026-11-06)

- The local candidate supports 256 MiB (default), 512 MiB, and 1 GiB per request for PAGEOUT and app-scope cgroup reclaim. The helper retains the 1 GiB hard ceiling and independently clamps against live target RSS or the selected scope's `anon + file` charge.
- App-scope requests are fixed to the exact cgroup v2 path re-derived from the selected process. The helper uses descriptor-relative traversal with `O_NOFOLLOW`, refuses child cgroups, verifies all members belong to the requester and that exact scope, checks identity again immediately before one `memory.reclaim` write, and never retries.
- Capability checks are operation-specific: PAGEOUT requires effective `CAP_SYS_NICE` for `process_madvise`; app-scope reclaim does not require that unrelated capability and instead relies on the fixed Polkit action plus an actual write-open of the re-derived `memory.reclaim` control. A denied or unavailable control fails before any write.
- Fixture tests use a stub for PAGEOUT and a temporary regular file for `memory.reclaim`; they do not call the live kernel reclaim interfaces. Real Hyprland operation and large daily-app reclaim remain NOT RUN for this checkpoint.

## R5 PAGEOUT private-resident guard (2026-11-07)

- Candidate discovery and the helper share one strict `smaps` parser. For eligible VMAs, `Private_Clean + Private_Dirty` is shown as a resident estimate and included in the selection snapshot. It is not a guarantee that those bytes will be advised or reclaimed.
- The protocol-v4 helper clamps PAGEOUT to the selected budget, live RSS, eligible VMA lengths, and the helper's fresh private-resident estimate. It reads the target's fixed `/proc/<pid>/pagemap` and only forms vectors from pages marked present (bit 63), not swapped (bit 62), and exclusively mapped (bit 56). Shared, absent, swapped, and inaccessible pages are skipped or cause a fail-closed refusal. The scan is capped at 8 GiB of virtual address space and 1024 iovecs.
- The GUI shows the private-resident estimate and limiting measure before authorization. PAGEOUT confirmation says it selects only present, exclusive private pages; app-scope mode instead says `memory.reclaim` is bounded to that exact scope. The helper's requested/advised bytes and before/after observations remain separate and are not labelled as RAM freed.
- Regression fixtures cover present/exclusive, present/nonexclusive, swapped, absent, missing pagemap, and private-resident changes. They use a stubbed syscall and do not invoke Polkit or a live kernel reclaim interface.
- At the time of this initial candidate checkpoint, protocol v4 was local-only and no live operation had occurred. It was later installed under separate authorization; the live observation below confirms the installed helper reported protocol v4. The initial checkpoint itself did not verify any real reclaim effect.

## R5 v4 controlled live PAGEOUT observation (2026-11-07)

- One Polkit-authorized call targeted only a project-owned temporary process. The installed protocol-v4 helper revalidated the exact PID, start time, UID, user app scope, identity token, and mapping digest successfully.
- At syscall time only 65,536 bytes of present exclusive private pages remained. The 256 MiB user budget was therefore capped to 65,536 bytes; `process_madvise(MADV_PAGEOUT)` was called once and returned 0. RSS was observed at 3640→3576 KiB and Swap stayed 267356 KiB. These observations do not prove 65,536 bytes of net RAM release.
- All 65,536 test pages passed integrity checking; the temporary process exited normally and its scope disappeared. This confirms one real helper/syscall path on a small target, **not** a 256 MiB reclaim result. No retry, daily-app operation, M0 action, or Swap/ZRAM change occurred.
- During the Polkit wait the idle test mapping was largely swapped out. This is a timing/host-residency limitation, not evidence that the helper should accept swapped pages; the helper correctly capped the request.

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
