# Issue #12 — balanced clean, round delivery

Integration head: `5c0abb0456e18e74706f7e77efaed6fbe2744545`
Base: local R5 `6e1e550` (contains `161ceb5` handoff and `9323a6e` identity fix).
Not pushed. Not installed.

## The finding that changed the design

The balanced clean **does not need Polkit**. Measured on this host: with the
`memory` controller delegated to the user session, every `app-*.scope` in the
user's slice has `memory.reclaim` owned by that user at mode 0200. A uid-1000
process opens and writes it — for its own scopes and for other applications'
scopes alike (verified by a zero-byte write to six real scopes owned by other
apps).

So this round did not route the balanced action through `pkexec`, did not add a
Polkit action, and did not touch the cross-process helper. It opens one exact
scope directory by fd, re-checks the owner *from the fd* rather than the path,
requires the bare scope name with `O_NOFOLLOW`, and writes the measured-safe
form.

## Measured facts this round rests on

| # | Finding | Evidence |
| --- | --- | --- |
| 1 | A bare byte count reclaims **anonymous** pages on this kernel | `128M` took 128 MiB, all from anon, in a cgroup with no file pages |
| 2 | `swappiness=0` is the only form restricted to file pages | `128M swappiness=0` → EAGAIN, 0 reclaimed, anon unchanged |
| 3 | Parameter order is load-bearing | `swappiness=0 128M` → EINVAL |
| 4 | **EAGAIN does not mean nothing happened** | `32M swappiness=0` → EAGAIN, and the scope still lost 4 MiB of file pages |
| 5 | Scope write needs no privilege | six foreign app scopes accepted a zero-byte write as uid 1000 |
| 6 | A `mkdir` scope has **no** memory accounting here | `memory.stat` all zero while the workload sat in it; a `systemd-run --scope` scope works |

## What each line changed

| Line | Head | Change |
| --- | --- | --- |
| A | `66bed36` | Read-only sampler (meminfo, PSI, vmstat, swaps, zram, missing-source flags) and planner (5 states, water marks, hysteresis, cooldown, dirty-backlog and PSI stop) |
| B | `c32d1f3` | Privileged-form file-first backend, later corrected for finding #4 |
| C | `ae843a9` | Tenth clean-menu row, plan before action, pure presentation helpers, honest skips |
| D | `9d21b65` | Same-host control and treated arms |
| — | `d9ecb7d`..`5c0abb0` | Unprivileged executor, scope discovery, GUI wiring, scope-bounded budget |

## The real end-to-end run

Under a private Xvfb, with a real pointer click on the tenth row, on a machine
state where file pages dominate:

```
CI_BALANCED_RUN scope=org.chromium.Chromium status=1 stop=4 \
                 requested=49184768 accepted=47689728
CI_BALANCED_PLAN state=3 action=1 request_bytes=442378240 executed=true
```

49.2 MB requested, 47.7 MB accepted by the kernel, unprivileged, in a real
Chromium scope. `status=1` is `partial` and `stop=4` is `budget_met`: the
requested budget was reached, which is the honest label for "asked for N, the
kernel took N".

On the current normal machine state the same row correctly refuses:

```
CI_BALANCED_DISABLED entry=1 state=4 action=0
```

available, but the planner sees `anon_pressure` and proposes nothing, because
`inactive_anon` (1988 MiB) exceeds `inactive_file` (1555 MiB) here.

## Bugs found by running it, not by reading it

1. The row's availability was tied to `clean_production`, hiding it in every
   build without the page-cache gate.
2. The scope root from `/proc/self/cgroup` is mount-relative; listing with it
   returned zero scopes while looking correct.
3. The calling process sits in a leaf scope, so matching only a trailing
   `/app.slice` never matched.
4. A zero budget leaked through the bridge because the 32 MiB batch default was
   clamped to 0 and the executor refused for the wrong reason.
5. The pre-probe snapshot was taken after the probe, so reclaimed bytes could
   vanish from the deltas.
6. EAGAIN was counted as zero reclaimed, understating every partial answer.
7. A machine-wide 442 MiB plan was aimed at a scope holding 73 KiB; the budget
   is now capped by the chosen scope's own file counter.
8. Scope listing offered `dsh-subprocess-*` (harness plumbing) and showed
   `deepseek\x2dai\x2ddsh\x2ddesktop` instead of decoding the systemd escape.

## Candidate and rollback

```
/home/arlune_innersea/q12-evidence/q12-candidate/memreduct-zamak
  807c2524a31f01d3f1fc07288f820b23b16c4e658774daacc08cf739d9d747d6
```

Only the GUI changes. The two helpers on this machine are left as they are.

Install: `sudo bash scripts/install-arch.sh`
Rollback: `sudo bash scripts/uninstall-arch.sh` then restore
`/usr/bin/memreduct-zamak.before-p0a`.

Nothing was installed, pushed, merged or tagged. No Polkit policy, PAM rule,
swap, zram or sysctl file was created or modified.

## Verification run on this head

| Command | Result |
| --- | --- |
| `python3 -m unittest discover -s tests -p "test_*.py"` | 325 OK (4 skipped) |
| `zam check .` / `zam test .` | OK / 85 passed |
| `bash scripts/build-arch.sh` | built |
| `scripts/ci/q12-balanced-menu-xvfb.sh` | PASS + real run (above) |
| `scripts/balance-sample.sh` | `state=anon_pressure action=none` |

## Still not done

1. **No confirmed beneficial same-host demo.** The machine did not present a
   state where file pages were the cause of its stalls; the 47.7 MB reclaim ran
   against a deliberately constructed scope. Claiming machine-wide benefit from
   that would be false.
2. **Anonymous pageout stays off** and was not exercised against a live target.
3. **Scope picker: done since this line was written.** The tenth row now opens
   a panel listing every app scope with its file pages and anonymous pages side
   by side, eligible rows selectable, the rest greyed with the reason inline,
   and the planner's refusal (when it refuses) printed above the buttons.
   Verified under Xvfb with real clicks: 6 scopes listed, 49 MiB budget
   matching the selected scope, click changes the panel by 488 px.
4. **Windows pixel 1:1** is tracked separately and does not block this.

## Safety boundary held

No process killed or paused. No global `drop_caches`. No swap, zram, sysctl,
PAM or Polkit change. No background reclaim service. Every kernel write this
round made was to a cgroup the test owned.
