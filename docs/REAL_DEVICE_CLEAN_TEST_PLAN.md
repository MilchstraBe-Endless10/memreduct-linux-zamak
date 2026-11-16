# Real device authorization and page-cache test plan

- **Status:** PREPARED ONLY. Nothing in this document has been executed on any device.
- **Requires:** separate explicit approval naming the target device and the exact
  action. Approval of P3B, of Issue #10, or of this document is not that approval.
- **Scope:** Verify, once, on a dedicated machine, the three things the isolated
  tests cannot prove: real Polkit authorization, the installed root-owned
  helper, and a real write to `/proc/sys/vm/drop_caches`.

## Why this needs its own approval

The kernel documents `drop_caches` as a debugging aid, not an optimization:
caches are reclaimed automatically when memory is needed, and dropping them
forcibly costs I/O and CPU afterwards. On a machine in use, the visible effect
is that applications re-read files they already had cached. That is a
performance change to the running system, which is why it is not part of
ordinary CI and not something this round performs.

## Device requirements

| Requirement | Why |
|---|---|
| Dedicated or disposable machine | The run degrades cache behaviour for a while afterwards |
| Not the daily-driver workstation | It is a benchmark-hostile action on a machine in use |
| Physical or VM access to undo state | The helper is installed and removed in this plan |
| Known memory workload available | Needed to observe an effect at all; an idle machine shows nothing |

## What gets installed, and where

| Path | Owner | Mode | Content |
|---|---|---|---|
| `/usr/libexec/memreduct-zamak-helper` | `root:root` | `0755` | the compiled helper |
| `/usr/share/polkit-1/actions/io.github.milchstrasse_endless10.memreduct.zamak.policy` | `root:root` | `0644` | the action, pinned to the helper path |

`scripts/install-arch.sh` performs this with a staging directory, forces
ownership and modes, and verifies owner/mode/symlink before claiming success.
No other file is written by the installer.

## Commands, in order

Run as the ordinary desktop user unless stated. Every step states what it
proves and what it must print.

### 0. Record the baseline

```
free -k
cat /proc/sys/vm/drop_caches          # must be absent or unreadable as user
sha256sum /usr/libexec/memreduct-zamak-helper   # must not exist yet
```

Expected: `drop_caches` cannot be written by the normal user; the helper is
absent. Record `MemAvailable`, `Cached`, `SReclaimable`, and every ZRAM/Swap
value verbatim. **Do not change Swap or ZRAM at any point in this plan.**

### 1. Install

```
sudo bash scripts/install-arch.sh
stat -c '%n %U:%G %a' /usr/libexec/memreduct-zamak-helper \
  /usr/share/polkit-1/actions/io.github.milchstrasse_endless10.memreduct.zamak.policy
```

Expected: `root:root 755` and `root:root 644`, and the installer's own
verification step reports success. If it reports anything else, stop; do not
adjust permissions by hand.

### 2. Prove the helper refuses everything except the fixed action

```
/usr/libexec/memreduct-zamak-helper                 # exit 2, usage
/usr/libexec/memreduct-zamak-helper metadata        # exit 2, rejected
/usr/libexec/memreduct-zamak-helper all                # exit 2, rejected
/usr/libexec/memreduct-zamak-helper --dry-run       # exit 0, "no kernel state modified"
```

The dry run proves the argument path without writing. Nothing so far changes
kernel state.

### 3. Prove authorization is required

```
pkexec /usr/libexec/memreduct-zamak-helper --dry-run
```

Expected on a desktop with an agent: an authentication dialog appears. Cancel
it. `pkexec` returns non-zero and the helper does not run. This is the only
step that touches Polkit, and it performs no write.

If **no agent appears**, stop. A missing agent on a headless session means the
authorization path is untested, not that it is unnecessary.

### 4. The one real write

```
pkexec /usr/libexec/memreduct-zamak-helper pagecache
```

Before and after, in the same shell:

```
free -k
```

- Expected helper output: `kernel accepted the cache reclaim request; reclaimed
  bytes are not measured`, exit 0.
- Expected observation: `Cached` and `SReclaimable` fall; `MemAvailable` may
  rise by an unrelated amount because other processes are also allocating.
- **What must not be claimed:** that a specific number of bytes was freed, that
  the rise equals what the operation returned, or that any application got
  faster. The kernel accepts a request; it does not report an amount.

Run once. Do not loop it.

### 5. Prove the uncertain path is not silently retried

Only if a fault injection build is available. Otherwise record NOT RUN.

### 6. Remove the installation

```
sudo bash scripts/uninstall-arch.sh
sha256sum /usr/libexec/memreduct-zamak-helper    # must fail: file removed
ls /usr/share/polkit-1/actions/ | grep memreduct # must be empty
```

## Rollback

| Situation | Action |
|---|---|
| Wrong permissions after install | `sudo bash scripts/uninstall-arch.sh`, then reinstall with the same script |
| Authorization dialog absent | Stop; do not bypass with `sudo`. Report the session type |
| Helper returns 3 (uncertain) | Treat as unknown. Reboot if the system misbehaves; do not re-run |
| Unexpected memory pressure | Stop the run; the caches rebuild on their own |

No step of this plan requires modifying Swap, ZRAM, or any boot configuration,
and none of those is permitted as part of it.

## Cost the user accepts by approving this

- Files that were cached must be read from disk again, so the first access to
  each is slower.
- The kernel's own advice is that doing this outside testing or debugging is not
  recommended; this run is the diagnostic case, not routine use.
- The effect is not measurable in a stable way, so the run cannot produce a
  "freed X MB" figure even if it is asked for.

## What this plan does not authorize

- Any second run, any periodic or automatic reclaim
- Any change to `swapoff`, Swap, or ZRAM
- Any test on a machine not named in the approval
- Any use of this helper for anything other than the `pagecache` action
