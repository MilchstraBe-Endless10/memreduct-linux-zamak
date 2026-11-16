# ADR-0008: What each Mem Reduct action actually becomes on Linux

- Status: PROPOSED (implementation exists behind the default-off gate)
- Issue: #12, balanced clean
- Upstream reference: Henry++ Mem Reduct `v.3.5.2`, tag `2f5c55d15f69a2c88859e036b16c6f4c971e1968`

## Context

Mem Reduct for Windows exposes four cleaning actions. The product boundary for
this project says each Windows action must be mapped to a Linux mechanism with
its own capability and safety argument, or declared unsupported. Mapping all of
them onto `drop_caches` is explicitly forbidden.

This ADR records what was measured on the target host (Arch/Omarchy, kernel
`7.2.15-1-omarchy`, cgroup v2 with the `memory` controller delegated to the
user session) and what the implementation therefore does.

## Decision

| Windows v3.5.2 action | Linux mechanism | Status |
| --- | --- | --- |
| Empty working set | `process_madvise(MADV_PAGEOUT)` on a pidfd-pinned, identity-verified single target | Implemented, `crossproc_helper.c`, protocol 4 |
| Clean system file cache | cgroup v2 `memory.reclaim` with `swappiness=0` on one exact app scope | Implemented, `balance_reclaim.c` |
| Clean standby list | none | **Unsupported.** The standby list is a Windows cache-manager concept; Linux has no user-addressable equivalent. Not emulated with `drop_caches`. |
| Clean modified list (flush) | none addressable per-list | **Unsupported.** Writeback is driven per page and per backing device; there is no "flush this list" knob that does not amount to global sync. |

The global `/proc/sys/vm/drop_caches` is not used by any balanced path. It
remains available only as the separate, explicitly-requested page-cache helper
that existed before this ADR, and it is still gated behind its own Polkit
action and capability gate.

## The measurement that decides the write format

On the target host, inside a throwaway child cgroup holding ~600 MiB of
anonymous memory and ~0 MiB of file cache:

```
write "128M swappiness=0"   -> EAGAIN, 0 bytes reclaimed, anon unchanged
write "128M"                -> accepted, 128 MiB reclaimed, ALL of it anonymous
write "128M swappiness=200" -> accepted, 128 MiB reclaimed, ALL of it anonymous
```

Two conclusions follow, and both are load-bearing:

1. A bare byte count is not "clean the cache". With no file pages present the
   kernel takes the anonymous working set and pushes it to swap. On a machine
   whose ZRAM is already nearly full, that is exactly the harm this product
   must not cause silently.
2. `swappiness=0` is the only form that restricts the request to file pages,
   and when no file page qualifies the kernel answers `EAGAIN` instead of
   taking anonymous memory.

### EAGAIN is not "nothing happened"

Measured afterwards on the target host, inside a scope holding file pages:

```
write "0 swappiness=0"     -> accepted, 0 bytes reclaimed  (probe is a no-op)
write "32M swappiness=0"   -> EAGAIN, and the scope still lost 4 MiB of file pages
```

`EAGAIN` reports that the request was **not fully satisfied**, not that the
call was rejected outright. A caller that counts `accepted_bytes += size` only
on a zero return understates every partial reclaim to zero; a caller that
counts `+= size` on EAGAIN would overstate it. Neither is acceptable, so both
backends now measure the scope counters before and after each batch and report:

- `accepted_bytes` — bytes the kernel took during a call it returned success for;
- `observed_on_reject_bytes` — bytes that left the scope during a call it did not.

Both are still observations of the scope, not promises about the machine.

The parameter order matters: `swappiness=0 128M` is `EINVAL`, because the
kernel parses `<bytes> <knobs...>`.

## Consequences

- `balance_reclaim.c` has no code path that writes a bare byte count, not even
  as a fallback after a rejection. A kernel that rejects `swappiness=0` gets
  the answer "unsupported", not a degraded retry.
- `EAGAIN` means "no eligible file pages" and is reported as a stop, never as
  a partial success and never retried.
- Requests are issued in small batches so the loop can stop between them. A
  stop condition latches: once set, no further batch is issued.
- Cumulative counters reported to the user are *requests issued* and *requests
  the kernel accepted*. Neither is an amount of memory freed, and the GUI must
  not present them as one.
- Swap capacity is not authorization. A full ZRAM with free disk swap is
  reported as two separate facts; it is never read as "swap has room, so push
  the working set out".

## Open items

- `MADV_PAGEOUT` remains a separate, per-target decision with its own budget.
  It is not part of the file-first path and this ADR does not enable it.
- The standby and modified list rows are descriptive, not implementable. Any
  future attempt to emulate them must come back through a new ADR.
