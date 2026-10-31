# P3B round 2 — authorization semantics and the review-build limit

Follows Issue #10 comment (2026-10-31). Previous round ended at
`676d73fbe42e379e082c31dd1c57c4f99ddc4284`.

## 1. The defect this round fixed

`pkexec(1)` forwards the child's exit status. Codes 126 and 127 are documented
as pkexec's own "dismissed" and "authorization or launch failure" values, but a
child that exits with the same number produces a byte-identical status. The
bridge mapped them straight to `AUTH_CANCELLED` and `AUTH_OR_PKEXEC_FAILED`, and
the GUI rendered that as **"Authorization cancelled"** — a claim about the user's
action that the available evidence cannot support.

That was wrong in both directions: a helper that legitimately exits 126 was
reported as the user cancelling, and a genuine pkexec refusal was reported with
the same confidence as a proven cause.

### What changed

| Before | After |
|---|---|
| `126` → `MR_RECLAIM_AUTH_CANCELLED` | `126/127` without child evidence → `MR_RECLAIM_OUTCOME_UNCERTAIN` |
| `127` → `MR_RECLAIM_AUTH_OR_PKEXEC_FAILED` | `126/127` with child-reported failure → `MR_RECLAIM_HELPER_FAILED` |
| GUI state 6: "Authorization cancelled" | GUI state 6: "Authorization or execution not confirmed" |
| state name `authorization-cancelled` | `authorization-unconfirmed` |

`mr_reclaim_child_reported_failure()` is the channel for the child-side
evidence; it is the parent recording what the child said about itself, not a
guess from stderr. pkexec's stderr is deliberately not parsed — that text is not
trustworthy input, and the issue says so explicitly.

`OUTCOME_UNCERTAIN` already forbids automatic retry, so the safety behaviour did
not change; only the honesty of what the interface claims did.

### Evidence

- `test_exit_codes_126_and_127_need_child_evidence_to_be_attributed` covers both
  codes with and without child evidence, and that reporting a stage for a
  non-running child does nothing.
- Mutation check: reverting to "trust the number" fails four assertions.
- `TestConfirmationWording` pins the wording: state 6 carries no cause claim,
  the retracted strings are gone, and the two backends keep separate sentences.

## 2. Production warning wording

The production dialog no longer borrows the simulation's harmless sentence. It
now reads:

- title: `Clean page cache` / `清理页缓存`
- line 2: `Drops clean page cache only.` / `仅丢弃干净页缓存。`
- line 3: `Files re-read slower; no free amount promised.` / `文件将变慢重读；不保证释放量。`

The simulation keeps its own three lines. `TestConfirmationWording` asserts each
simulation sentence appears exactly once, so a future edit cannot collapse the
two into one string.

## 3. The review-build limit — reported, not worked around

Issue #10 asks for the production warning to be rendered and captured. It cannot
be done from this checkout, and the reason is a tool limitation rather than a
missing step:

- `zam` takes no `-D` flags and `zam.toml` has no defines section.
- Its build cache is keyed on source content. A compiler wrapper on `PATH` is
  invoked once for the cache probe, but the cached object is then reused, so the
  define never reaches the native object.
- Proof: the wrapper build produced a binary byte-identical to the build without
  it (`sha256 16ffda1f…` in both cases).

The macro itself does work when the object is compiled directly:

| Object compiled | `mr_production_clean_enabled()` |
|---|---|
| no define | `0` |
| `-DMEMREDUCT_ENABLE_PRODUCTION_CLEAN=1` | `1` |

So the gate is verified at source level, and the **rendered** production strings
are NOT RUN. A script that wrapped `cc` was written, found to produce a
misleading Green build, and deleted rather than shipped.

## 4. What is verified, by category

| Area | State | Evidence |
|---|---|---|
| Normal mode, no `MEMREDUCT_CI` | PASS | 0 bytes stdout/stderr, 0 markers, 0 privilege mentions, 0 GTK warnings, 337x369 |
| Isolated simulation loop, real clicks | PASS | `scripts/ci/p3b-clean-simulation-xvfb.sh`, 13 markers incl. helper contract |
| Default build refuses the action | PASS | `scripts/ci/p3b-production-gate-xvfb.sh`, `fits=true` on dialog text |
| Confirmation dialog text fits | PASS | `inner_width=229 widest_line=183` |
| Exit-code attribution | PASS | native tests + mutation |
| Production warning strings in source | PASS | `TestConfirmationWording` |
| Production warning **rendered** | NOT RUN | see §3 |
| Real Polkit authorization | NOT RUN | needs separate device approval |
| Installed root helper | NOT RUN | needs separate device approval |
| Real page-cache write | NOT RUN | needs separate device approval |
| Hyprland compositor input | NOT RUN | all input went to a private Xvfb |
| Pixel 1:1 vs Windows 3.5.2 | NOT PASS | measured baselines only |
| GitHub CI on this SHA | NOT RUN | nothing pushed; the SHA is local-only |

Totals: `python3 -m unittest discover -s tests` → 66 passed;
`zam test .` → 76 passed, 0 failed; `zam check .` → CHECK OK;
`bash -n scripts/*.sh scripts/ci/*.sh` → clean.

## 5. Prepared, not executed

`docs/REAL_DEVICE_CLEAN_TEST_PLAN.md` is the plan Issue #10 asks for: commands,
expected output per step, the cost the approver accepts, rollback, and what the
plan does not authorize. It has not been run on any device.

## 6. Items needing user authorization

1. **Real device test** — naming the machine and the action, per the plan above.
   Nothing else in this list blocks it.
2. **A build path that can define `MEMREDUCT_ENABLE_PRODUCTION_CLEAN`.** Either
   Zamak gains a defines mechanism, or the diagnostic variant gets a supported
   build route. Until then the production strings cannot be reviewed as pixels.
3. **Push / PR Ready / Merge** — none performed; the branch is local.
