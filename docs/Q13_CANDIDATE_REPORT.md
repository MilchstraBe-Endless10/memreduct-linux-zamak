# Q13 candidate report — memory footprint, clean-path correctness, release gaps

Baseline branch `q12/integration`, starting commit `2e8e1f1`.
All measurements below were taken on this machine (Arch/Omarchy/Hyprland,
Wayland, HDMI-A-1 1920x1080 scale=1) with the production build
(`scripts/ci/build-production-gui.sh`, i.e. `gui_runtime_prod.c` — no CI hooks;
`mr_runtime_emit` is `{(void)line;}` in that build).

## A. Footprint — measured, not estimated

### Result: the 10 MB PSS target is not reachable on this GUI stack

| Stage | PSS | RSS | Private (clean+dirty) | SwapPss | Threads |
|---|---|---|---|---|---|
| cold start, 3 s | 28,249 kB | 43,320 kB | 23,744 kB | 0 | 11 |
| main window, 10 s | 28,124 kB | 43,324 kB | 23,496 kB | 0 | 11 |
| main window, 20 s | 28,128 kB | 43,328 kB | 23,500 kB | 0 | 10 |
| settled, 40 s | 30,101 kB | 43,328 kB | 26,720 kB | 0 | 9 |

Peak observed: 30,101 kB PSS. The target was 10,000,000 bytes ≈ 9,537 kB PSS.
**This build runs at roughly 2.9× the target.** No helper process was resident
during any sample (`pgrep -P` empty at every stage), so none of this is a
leaked helper.

### Where it goes: an empty GTK3 window already costs 18.5 MB

Measured with a 20-line C program that calls `gtk_init`, creates one
`GtkWindow`, shows it and runs `gtk_main`:

| | PSS | RSS | Threads |
|---|---|---|---|
| empty GTK3 window | 18,503 kB | 30,176 kB | 7 |

Its breakdown is already the same shape as ours: `libgtk-3.so` 3.45 MB,
`[heap]` 2.68 MB, anon 1.91 MB, fontconfig cache 1.10 MB, `memfd:gdk-wayland`
0.74 MB. **The floor is the toolkit, not our code.** A 9.5 MB PSS budget
cannot be met by any GTK3 application on this host, so the target as written
is unreachable by construction, not by our inefficiency.

### Our increment over that floor, and what we did about it

Integrated build, top mappings (PSS):

| | PSS | vs. empty-window floor |
|---|---|---|
| total | 29.33 MB | |
| `libgtk-3.so` | 4.22 MB | +0.77 MB (more widget classes touched) |
| `[heap]` | 3.91 MB | +1.23 MB (our state + GTK widget tree) |
| `libglycin-2.so` | 3.60 MB | +3.60 MB (image-loader sandbox, GTK 3.24.44 feature) |
| `[anon]` | 3.00 MB | |
| fontconfig cache | 1.10 MB | same |
| our binary | 0.95 MB | text+data |
| Adwaita Sans | 0.71 MB | |
| libharfbuzz | 0.68 MB | |

Two candidate savings were tested rather than assumed:

| attempted | PSS result | verdict |
|---|---|---|
| baseline | 27.89 MB | |
| `GTK_ICON_THEME_NAME=Adwaita XDG_DATA_DIRS=/usr/share` | 27.77 MB | −0.12 MB: not worth a behavioural change |
| `GDK_PIXBUF_MODULE_FILE=<empty>` | 27.36 MB | −0.53 MB, and `libglycin` still maps 4 segments |
| both | 27.14 MB | −0.75 MB total, still maps glycin |

So the glycin mapping cannot be dropped from outside the process; it is pulled
in by GTK 3.24.44's new image-loading path at first pixbuf use, not by our
cursor or icon code (we set no custom icon). The remaining delta over the
toolkit floor is our widget tree and parsing state, which is what the window
is for.

**What was NOT done:** no feature removed, no process killed to fake a number,
no self-pageout to shrink PSS. Those are the three ways this number could be
made to look better and all three are excluded by the brief.

**Honest conclusion for A:** target not met; measured numbers and the largest
sources are above; the binding constraint is the GTK3 baseline, demonstrated
with a minimal program rather than asserted.

## B. Clean-path correctness — seven defects fixed

`5dfaff5` (then `ab2a9a5` after integration). See the commit message for the
full per-item reasoning; summary:

- **B1** the whole-session fallback that bypassed the planner is gone. Both the
  `bal_action == 0` branch and the no-acknowledgement branch now refuse and
  report, instead of routing around the planner's scope decision.
- **B2** whole-session reclaim requires `bal_session_ack`; the default balanced
  path is scoped to the selected application or the configured list.
- **B4** stop reasons are named exactly
  (`guard_swap_rate` / `telemetry_unavailable` / `no_batches` /
  `completed_all_batches` / `batch_refused_partial`) instead of one word.
- **B5** whole-machine `pswpout` is no longer called a disk-paging rate
  anywhere in the tree; it is a system swap-out rate that includes ZRAM.
- **B6** whole-run, S4-stage and kernel-reported numbers are three distinct
  values in both the status text and the CI line.
- **B7** a failed pressure start clears the active flag and says "did not run"
  in the same three-state vocabulary as a normal finish.
- **B8** the whole-run baseline is captured at confirmation time, not after
  S1-S3, so the reported net no longer describes only the tail.

**One thing to be clear about:** the native rewrite that made S2-S4 one
asynchronous run (with a real cancel flag and per-stage accounting) arrived in
this worktree from a parallel attempt and did not link — the GUI still called
the removed `pressure_*` symbols. `ab2a9a5` restores that contract as a thin
projection over `deep_*` so the build works and the new capability is kept.
That projection is not the final shape: S1's ran/ok flags are still the GUI's
to record, and `partial_flags` now reports a stage-level fact rather than
per-batch EAGAIN. Both limitations are written into the code comments.

## C. Release gaps

`docs/CLEAN_CHAIN_CAPABILITY_MATRIX.md` (`eb876bf`) holds the itemised table:
18 implemented with evidence locations, 1 spec-only, 6 known defects, 4 copy
defects. It covers the clean chain (S1-S4) only — it is not a full
Windows→Linux capability comparison, so "platform has no equivalent" and "not
implemented" are 0 by scope, not by finding.

The five findings that matter most, all left as known defects rather than
papered over:

- **G-01** `CI_DEEP_S4` / `CI_DEEP_CLEAN` have no automated assertions anywhere
  in the repository. S4 is the only stage that can touch anonymous pages and it
  has no CI gate. Deferred by decision this round; it is the single most
  valuable follow-up.
- **G-02** guard early-stop is evidenced by one real-machine observation
  (`7cf3c4f`), not by a test.
- **G-03** no record ties the installed binary to a commit.
- **G-04** the S4 cgroup path has no injectable seam, so no unit test can reach it.
- **G-06** the fix is not deployed: `/usr/bin/memreduct-zamak`
  (`1c3ef7e0…`) still contains the old over-broad scope copy (`grep -aF` finds
  the old string once, the new strings zero times). The repaired binary
  (`54e1f22a…` / `9a4ec1e4…`) is in the worktree only.

## Release package hygiene

Must ship: the matrix document, `src/main.zm`, `native/balance_bridge.{c,h}`,
the copy fixes.

Must not ship: `artifacts/` (build output, 1.2 MB of binaries and symbol dumps —
untracked), `progress/*.md` (worker checkpoints), `deploy-backup/`, and any
`acceptance-*.log` under it. `production_clean_gate_enabled.c` is swapped in
only by the production script; the default `zam.toml` keeps `_default.c`, which
answers 0, so a default build cannot reach the pkexec bridge.

## Not done, deliberately

- `/usr/bin` was not overwritten (deployment needs the owner's confirmation).
- Nothing was pushed, merged or released.
- No new CI assertions were written (G-01 deferred).
