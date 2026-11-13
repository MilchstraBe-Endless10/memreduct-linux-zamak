# Issue #12 — same-host control and treated measurements

Host: Arch/Omarchy, kernel `7.2.15-1-omarchy`, cgroup v2 with `memory`
delegated to the user session. Both arms run unprivileged, inside throwaway
cgroups (`q12-control.scope`, `q12-treated.scope`), and touch nothing global:
no `drop_caches`, no sysctl, no swap/ZRAM change, no signal to any process
outside the test cgroup.

Scripts: `artifacts/q12-line-d/host_control.py`, `host_treated.py`.

## Why a control arm at all

A single before/after number cannot distinguish "our action helped" from "the
machine drifted". This host drifts a lot: during the 22-second control run,
with nothing done to it, `MemAvailable` fell from 1746 MiB to 1338 MiB
(-408 MiB) purely from the rest of the desktop working.

Any claim of benefit must therefore survive comparison against that drift, and
must be expressed on the metric that actually corresponds to the harm the
product targets — stalls, not a free-memory percentage.

## Control arm (no action)

| Field | Delta over 22 s |
| --- | --- |
| MemAvailable | −408 MiB (host drift, no action taken) |
| Test cgroup `memory.current` | 0 (workload resident, untouched) |

## Treated arm (file-first, `swappiness=0`)

Workload: 350 MiB of a 400 MiB file read into page cache inside its own cgroup,
so the reclaim has genuinely eligible cold file pages. 8 batches of
32 MiB, the exact production write form; every batch was accepted (0 errors).

Baseline in the cgroup: `file` 165.9 MiB, `current` 170.6 MiB.

| Field | immediate | +30 s | +120 s | +240 s |
| --- | --- | --- | --- | --- |
| cgroup `file` (MiB) | −165.9 | −165.9 | −165.9 | −165.9 |
| cgroup `current` (MiB) | −165.9 | −165.9 | −170.6 | −170.6 |
| cgroup anon (MiB) | −0.004 | −0.004 | −4.7 | −4.7 |
| cgroup swap (MiB) | 0 | 0 | 0 | 0 |
| MemAvailable (MiB) | −214 | +5.9 | −331 | −809 |
| PSI some (ms) | 9 | 155 | 1420 | 2137 |
| pswpin | 20120 | 109540 | 378347 | 632228 |
| pswpout | 0 | 36238 | 370756 | 560558 |

## What this actually shows

1. The file-first mechanism works and is bounded: 165.9 MiB of file pages left
   the cgroup and the anonymous side did not move (−4 KiB immediate). That is
   the behaviour the `swappiness=0` write was chosen for, confirmed end to end
   on the production write path.
2. It is **not** evidence of a balanced-clean success. `MemAvailable` did not
   improve at any observation point, and PSI stall time rose monotonically to
   +2.1 s over four minutes. The cgroup's file pages were not the thing making
   this host stall; the anonymous working set and swap traffic were.
3. The honest reading of the treated arm is "the mechanism is correct and did
   exactly what it was told, on a workload that was not the problem". Claiming
   a benefit from it would be exactly the fake success the product forbids.

## Consequence for the shipped policy

The planner already refuses this shape of machine: with `inactive_anon`
(1988 MiB) exceeding `inactive_file` (1555 MiB), it classifies
`anon_pressure` and proposes **no** file-first run — see
`scripts/balance-sample.sh` output on this host:

```
state   anon_pressure
action  none
reason  anon_disabled
request 0 bytes
```

That is the intended outcome, and it matches what the treated arm would have
cost if it had been allowed to run on the live desktop.

## What is still missing

- A several-hundred-MiB **beneficial** demonstration needs a machine or scope
  where cold file pages dominate the stall. This host does not have that shape
  at the moment of measurement, and manufacturing pressure was explicitly out
  of scope for this round.
- Anonymous pageout remains off by default and was not exercised here.
