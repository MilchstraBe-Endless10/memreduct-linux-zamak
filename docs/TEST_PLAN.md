# Test and release qualification matrix

| Gate | Environment | Commands / exercise | Pass condition |
|---|---|---|---|
| G0 file integrity | any | `python3 scripts/verify_project.py` | all required docs + files + licenses exist |
| G1 C safety tests | non-root Linux | `python3 -m unittest discover -s tests -v` | 100% pass, no real `drop_caches` write |
| G2 shell lint | bash | `bash -n scripts/*.sh` (loop per file) | no syntax error |
| G3 Zamak static | Arch or CI with `zam` | `zam check .`; `zam test .` | both pass, exact version recorded |
| G4 native build | Arch with GTK3/pkgconf | `zam build .`; `bash scripts/build-arch.sh` | produces executable, no missing symbols |
| G5 GUI basic | Omarchy + Hyprland | launch non-root; click actions in dry-run test mode | window and events work on Wayland, no hang |
| G6 privileged flow | expendable test environment | authorized / denied / canceled Polkit | correct labels, exit codes, no elevation leak |
| G7 real cache test | isolated test host, approved | controlled file-cache benchmark | no file loss, log reclaim/cache refill cost |
| G8 auto policy | unprivileged simulation | threshold, cooldown, time warp, repeated denied auth | no hidden repeat or excessive prompts |
| G9 ZRAM | isolated machine | conflict detection + failure injection + rollback | no swap loss / OOM / false success |
| G10 packaging | clean Arch VM or disposable host | install → use → uninstall | all files tracked, helper perms correct |
| G11 release | GitHub PR + CI | signed/traceable commit, checksum, binary audit | reproducible provenance, known risks in notes |

## P1 — Zamak 0.2.0 qualification (2026-10-25)

Environment: Omarchy 4.0.6 (Arch-based), Hyprland 0.56.2, Wayland, GTK 3.24.52, Zamak 0.2.0. Source branch `fix/issue-1-zamak-build`, based on `080417e153d91bf2adac756e5154311fef49333f`.

| Command / exercise | Status | Evidence |
|---|---|---|
| `python3 scripts/verify_project.py` | PASS | 29 required project/governance checks passed. |
| `python3 -m unittest discover -s tests -v` | PASS | 3 C safety tests passed, including dry-run and unprivileged denial coverage. |
| `bash -n` on each `scripts/*.sh` | PASS | `build-arch.sh`, `install-arch.sh`, `uninstall-arch.sh`, and `publish-github.sh` parsed successfully. |
| `zam --version` | PASS | `0.2.0`. |
| `zam check .` | PASS | `CHECK OK memreduct_zamak`. |
| `zam test .` | PASS | 5 passed, 0 failed. |
| `zam build .` | PASS | Produced `build/memreduct_zamak-00ee9d44b77125b6721133332f35be3f1f9d3d518e7ab20b40d8265940928d57`. |
| `bash scripts/build-arch.sh` | PASS | Re-ran Zamak check/tests/build and compiled `build/memreduct-zamak-helper`. |
| `build/memreduct-zamak-helper --dry-run` | PASS | Reported no kernel state modified. |
| GUI launch and refresh | PASS | Launched as the normal user on Wayland (`xwayland=false`); Chinese UI, RAM/cache/Swap readings, controls, periodic refresh, and normal close were observed; process exited 0. |

GitHub Actions `build-and-test` passed on code SHA `1d4654867f59154981010007288cff1fda42f457` for both events: [push run 40170617141](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40170617141) and [pull request run 40170634633](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40170634633). The documentation follow-up at `9b5177e992eb1c86b72643a0ae2c0cda9a570555` also passed both events: [push run 40170749833](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40170749833) and [pull request run 40170752581](https://github.com/MilchstraBe-Endless10/memreduct-linux-zamak/actions/runs/40170752581).

The installer candidate pattern matched the Zamak output name and excluded the `-test-` binary. `ldd` reported no missing libraries. The install/uninstall scripts were not executed because they modify `/usr`. Polkit authorization end-to-end was not exercised. No cleanup action or automatic mode was activated, and `/proc/sys/vm/drop_caches`, Swap, and ZRAM were not modified. GitHub Actions status is recorded in `memory/SHORT_TERM.md` after the PR run.

## Test reporting contract
`command / platform / SHA / status(PASS|FAIL|NOT_RUN) / evidence / risk`. **NOT_RUN != PASS**.
Track p95 UI response, cold-cache re-read latency, memory available before/after, and exact branch build. Avoid marketing “up to X% performance gain” unless independently benchmarked.

## Real workload selection
1. Controlled file IO to create file-page cache, with isolated test path.
2. Browser tabs + development tool memory pressure, observation only unless user approves write.
3. Creative software open and save operation (performance regression, never deliberately interrupt real work).
4. Baseline, same test with action, warm-cache penalty and 10-minute follow-up; repeat several trials.
