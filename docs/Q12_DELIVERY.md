# Issue #12 — balanced clean, round delivery

Integration head: `999e474ee6d5d2d3596915588328b5348ab7c00e`
Base: local R5 `6e1e550` (contains `161ceb5` handoff and `9323a6e` identity fix).

## What is in this round

Four lines were developed in independent worktrees from the same base and then
merged with no conflicts:

| Line | Head | What it adds |
| --- | --- | --- |
| A | `66bed36` | Read-only sampler + planner: meminfo, PSI, vmstat, swaps, zram; classifies healthy / healthy_cache / cold_file / anon_pressure / thrashing; water marks, hysteresis, cooldown, sticky early stop |
| B | `c32d1f3` | File-first backend: cgroup v2 `memory.reclaim` with `swappiness=0`, small batches, sticky stop, no bare-byte-count path anywhere |
| C | `ae843a9` | Tenth clean-menu row: plan before action, availability answered before it draws, honest skips, no fake success |
| D | `9d21b65` | Same-host control and treated arms, plus the measurement report |

## What is code-complete vs installed vs verified

| Claim | Status |
| --- | --- |
| Planner and sampler implemented and unit-tested | **Yes** — 22 tests |
| File-first backend implemented and behaviour-tested | **Yes** — 14 tests plus 2 opt-in real-cgroup tests |
| `swappiness=0` restricts reclaim to file pages on this kernel | **Verified on this host** with a controlled A/B |
| GUI row exists, draws inside the popup, and answers a real pointer click | **Verified under a private Xvfb** |
| End-to-end: click -> plan -> privileged reclaim -> result panel | **Not wired.** The backend and the GUI live in separate translation units and no call path joins them in this branch. The row reports `CI_BALANCED_DISABLED entry=0` on a build without the gate open, which is correct fail-closed behaviour, not a success. |
| Installed on the system | **No.** `/usr/bin/memreduct-zamak` is untouched. |
| Several-hundred-MiB beneficial demonstration | **No.** See `docs/Q12_SAME_HOST_CONTROL.md` for why this host cannot show one honestly right now. |

## Candidate artifacts (not installed)

```
/home/arlune_innersea/q12-evidence/q12-candidate/memreduct-zamak
  56c0dc988512b936838168d5d9d2f9c9ea3e5df8b9eca06ee1933e73bb4fcc2b
/home/arlune_innersea/q12-evidence/q12-candidate/memreduct-zamak-crossproc-helper
  bfc8fbd997d0020e75a4fe22b57d2f545d09f1f49d21954270b4ca4623bfc905  (unchanged)
/home/arlune_innersea/q12-evidence/q12-candidate/memreduct-zamak-helper
  b0652c8ac3ae70576e39b71fbdcd047a7c57f23873fec2e45b1b691b34c45a5e  (unchanged)
```

The two helpers are byte-identical to what is already installed; only the GUI
changed. Protocol marker `MR_XPROC_PROTOCOL_ABI=4;` is present, matching the
installed helper.

## Install and rollback

Nothing here has been installed. To install, the owner runs:

```
sudo bash scripts/install-arch.sh
```

Rollback is the existing uninstall path plus the pre-existing backup:

```
sudo bash scripts/uninstall-arch.sh
sudo install -m0755 /usr/bin/memreduct-zamak.before-p0a /usr/bin/memreduct-zamak
```

No Polkit policy, PAM rule, swap, zram or sysctl file is created, modified or
removed by this round.

## Verification actually run on the integration head

| Command | Result |
| --- | --- |
| `python3 -m unittest discover -s tests -p "test_*.py"` | 306 tests OK (3 skipped) |
| `zam check .` | OK |
| `zam test .` | 85 passed |
| `bash -n scripts/*.sh scripts/ci/*.sh` | clean |
| `bash scripts/build-arch.sh` | built `build/memreduct_zamak-bf9608e6...` |
| `scripts/ci/q12-balanced-menu-xvfb.sh <binary>` | PASS, row reachable by pointer |
| `scripts/balance-sample.sh` on the host | `state=anon_pressure action=none reason=anon_disabled` |

## What is still not done

1. **The execute path is not joined.** The GUI row produces a plan; the backend
   can run it; nothing in this branch calls the backend from the controller.
   That is the one remaining integration step, and it needs the privileged
   helper wiring (Polkit) that this round deliberately did not touch.
2. **No beneficial same-host demonstration.** The host is anonymous-heavy
   (`inactive_anon` 1988 MiB vs `inactive_file` 1555 MiB) and its zram is
   nearly full; the planner refuses file-first there and the measurement
   confirms refusing was right.
3. **Anonymous pageout** is implemented as a separate decision but is off by
   default and was never exercised against a real target this round.
4. **Windows pixel 1:1** remains tracked separately and does not block this.

## Safety boundary held

No process was killed or paused. No global `drop_caches`. No swap, zram, sysctl,
PAM or Polkit change. No background reclaim service. Every measurement ran in a
throwaway cgroup that held only the test's own child.
