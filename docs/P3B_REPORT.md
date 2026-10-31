# P3B — Production GUI and isolated clean loop

Issue: [#10](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/issues/10)
Branch: `feat/issue-10-p3b-clean-closure`, started from `d8ca307090782182355892d2777a44c1ddac68fa`.

## 1. What actually works in normal mode

The application runs as an ordinary unprivileged GTK process and shows the
main window at its fixed size. Verified this round by launching the default
binary under a private Xvfb with `MEMREDUCT_CI` unset:

| Observation | Result |
|---|---|
| window appears and stays up | yes, 337x369 client |
| bytes written to stdout | 0 |
| bytes written to stderr | 0 |
| `CI_` markers | 0 |
| mentions of `pkexec`/`polkit`/`drop_caches`/`swapoff` | 0 |
| GTK warnings or criticals | 0 |

Preferences (English/简体中文, light/dark) persist and survive restart; that
work landed in P3A and was not changed here.

The clean button is present and draws its label, but in a default build it
refuses the action. That is not an accident of this round — see §4.

## 2. The isolated GUI → authorization → helper loop

`scripts/ci/p3b-clean-simulation-xvfb.sh` runs the production binary under a
private Xvfb and drives it with real XTEST input:

```
real click on Clean        -> CI_MOUSE_ACTION target=19
confirmation dialog opens  -> CI_CLEAN_CONFIRM open=true mode=simulation
Tab toggles both buttons   -> CI_CLEAN_CONFIRM_FOCUS choice=cancel / confirm
real click on Confirm      -> CI_MOUSE_ACTION target=710
authorization accepted     -> CI_CLEAN_SIM stage=authorization-accepted backend=mock
helper stage               -> CI_CLEAN_SIM stage=helper-running helper=mock
result shown in the window -> CI_CLEAN_SIM result=accepted … no_system_write=true
```

The run also asserts:

- the fake authorizer received the production contract,
  `/usr/libexec/memreduct-zamak-helper pagecache` — so the argument shape the
  privileged path would use is the shape that was exercised;
- the result changed the rendered window (`changed_pixels ≈ 2900`), not only
  the log;
- a second click does not silently retry;
- no privileged operation appears anywhere in the run.

Nothing privileged executes. There is no `pkexec`, no root helper, no write to
`/proc/sys/vm/drop_caches`, and the mock authorizer is a same-UID script in a
private directory that the bridge accepts only when it is a single-link 0600
executable owned by the caller.

## 3. Why production is still gated

Two barriers, both deliberate:

1. **Build time.** `mr_production_clean_enabled()` returns 1 only when the
   binary was compiled with `MEMREDUCT_ENABLE_PRODUCTION_CLEAN`. Nothing in
   `zam.toml` or `scripts/build-arch.sh` defines it, so no shipped build has
   the switch. An environment variable on a user's machine cannot create it.
2. **Runtime.** The main window asks that function before enabling the action.

`scripts/ci/p3b-production-gate-xvfb.sh` proves the barrier behaviourally rather
than by grepping symbols (statically linked C hides both):

```
click on Clean from the same geometry -> CI_MOUSE_ACTION target=19
guarded branch taken                  -> CI_CLEAN_DISABLED action=none
confirmation opened                   -> false
helper invoked                        -> false
```

The simulation backend being off is asserted in the same run
(`CI_CLEAN_SIM enabled=false`), so this is the gate and not a missing test
fixture.

## 4. Safety code and packaging results

| Area | State |
|---|---|
| helper accepts only `pagecache`, fixed target `/proc/sys/vm/drop_caches` | enforced; `metadata`/`all` rejected before opening |
| symlink and non-regular target rejection | covered by tests |
| single `EINTR` retry, then no further retry | covered by tests |
| short write or close failure → exit 3, "outcome uncertain", no auto-retry | covered by tests |
| test-only path override absent from production builds | `MEMREDUCT_TESTING` compile gate |
| Polkit action pins `org.freedesktop.polkit.exec.path` to the exact helper | `packaging/polkit-1/actions/…pagecache.policy` |
| installer stages, forces `root:root` + `0755`/`0644`, then verifies owner/mode/symlink before claiming success | `scripts/install-arch.sh` |

Tests: `python3 -m unittest discover -s tests` → **61 passed**;
`zam test .` → **76 passed, 0 failed**;
`bash -n scripts/*.sh scripts/ci/*.sh` → clean;
`zam check .` → `CHECK OK memreduct_zamak`.

Not run, and why: real `pkexec`, real installation of the helper, and any real
page-cache write. Issue #10 is not authorization to touch the running machine's
kernel state.

## 5. Bugs found and fixed this round

| Defect | Root cause | Fix |
|---|---|---|
| Every startup marker silently missing | `src/main.zm` printed through the Zamak runtime's buffered `println`; the process is normally killed by its bounded timer, so the buffer died with it | native `mr_ci_emit` writes to stderr and flushes; all 74 markers moved onto it. Regression asserts no marker uses `println` and that the emitter is silent without `MEMREDUCT_CI`; verified by mutation |
| Clean label drawn as disabled in a build that enables production | colour test only looked at `clean_simulation` | condition widened to `clean_simulation or clean_production` |
| Cancellation printed simulation wording on the production backend | marker was unconditional | backend-specific marker, note and value |
| Test-harness clicks did nothing | `xdotool click --window` sends synthetic events GTK ignores | real XTEST clicks |
| Staged progress never observed | mock authorizer exited at once, skipping the stages the bridge polls for | mock now publishes `authorized` / `helper-running` with dwell time |
| Stage file always rejected | `>` under the default umask produced 0644, but the reader requires 0600 | mock sets `umask 077` |
| Privileged-reference check could never fail | `grep -q` emitted nothing, so the downstream filter saw empty input | check rewritten to capture and test the actual hits |
| Gate check would pass without touching the button | fixed-pixel click landed in empty space at the GTK scale factor (window 674x738, not 337x369) | click coordinates derived as a fraction of the client area, and the run asserts the button was hit |
| Confirmation dialog clipped its own text mid-word (`…no system memory chan`) | second line measured 290px against 229px of usable inner width in a fixed-width panel | explanation split over two short lines and the panel made tall enough for three; the window now reports measured widths and the script requires `fits=true` |

## 6. Progress against P3A

P3A ended with the clean path disabled and no evidence that a GUI event could
reach it. Now:

- the click → confirmation → authorization → helper → result chain is driven
  by real input and asserted end to end;
- the refusal in a default build is asserted at the button, not inferred;
- both outcomes share one result-state model, so the wording cannot drift
  between a tested path and an untested one;
- the production bridge remains compiled in and auditable.

## 7. Binaries

- default/runnable: `build/memreduct_zamak-7582f543dc00123fb8100b4df5092a9532fd518594e23bf19b01f23f95cbd3f1`
- helper: `build/memreduct-zamak-helper`

## 8. Remaining blockers

**Functionality**
- Nothing blocks the read-only monitoring and preference surface.
- The clean action is unusable for end users until it is deliberately enabled.

**Safety authorization**
- ADR-0005 is still PROPOSED. A product owner has to accept that a diagnostic
  page-cache drop is worth exposing, given the kernel's own warning that doing
  so can cost significant I/O and CPU.
- Three things are unverified by construction: real Polkit authorization, the
  installed root-owned helper, and any real page-cache write. All three need
  separate explicit authorization and a device test.

**Hyprland**
- Everything here ran under Xvfb/XWayland-compatible GTK. No input was
  injected into the live Hyprland session, so compositor-level behaviour of
  the modal confirmation and the Polkit agent prompt is unverified.

**Pixel 1:1**
- The main window is 337x369 and settings 558x335; these are measured
  baselines, not proof of 1:1 fidelity against the Windows original. Font
  resolution differs by system, and the golden-image comparison is a CI
  concern that this round did not re-run.
