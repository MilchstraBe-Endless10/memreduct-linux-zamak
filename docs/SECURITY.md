# Security policy, threat model, and privilege requirements

## Assets and threat boundaries
- User processes/data, running desktop session and availability.
- Kernel memory control interfaces; host root privilege; helper executable path and polkit session.
- Public source/release pipeline; no real passwords or local identifying paths in any commit.

## Risks / mitigations
| Threat | Mitigation | Evidence gate |
|---|---|---|
| GUI elevates arbitrary action | helper accepts strictly enumerated modes; no shell | invalid args tests + manual review |
| Helper executable replaced | root-owned absolute path; verify mode/owner after install | packaging E2E test |
| Authorized action unexpectedly repeats | no background default; per-action state + cooldown | P4 tests + UX review |
| Cache eviction harms latency | product warning; opt-in only, never as best-performance claim | perf comparison (cold re-read) |
| ZRAM conflict or swapoff OOM | read-only first; no forced swapoff; rollback plan | separate ADR + user authorization |
| Race between worker and GUI | one operation, detect child exit, no busy-loop | stress tests + cancel/deny |
| Misleading freed-memory number | report observed `MemAvailable` delta only | snapshot fixture, concurrent load caveat |
| Leaked secrets in public repo | secret scan and review each staged diff | pre-push scan |

## Policy for privileged writes
- No automatic writes to `/proc/sys/vm/drop_caches` on unattended CI or developer machine.
- No unattended manipulation of ZRAM/swap; no PID targeting or arbitrary cgroup paths in v0.x.
- Document command, rollback, permissions, output, and expected performance impact before approved new capability.
- No blanket claims “safe / stable / no impact”; user intent defines design target, not proved result.

## Current production blockers
- `zam check`, `zam test`, `zam build` not yet verified.
- Cannot guarantee Polkit authorization works under Hyprland without a running authentication agent.
- No real-world permission-denied/canceled-session/desktop-tray tests.
- Need deliberate test of helper process EINTR/abnormal termination handling.
