## Issue / scope
Closes #

## Architecture and security impacts
- [ ] Changed privileged code? ADR / review link:
- [ ] Default auto-clean behavior unchanged or approved
- [ ] No secret/log/local path leakage
- [ ] UI non-root / no process termination

## Reproducible verification
| command | PASS/FAIL/NOT_RUN | evidence |
|---|---|---|
| python3 scripts/verify_project.py | | |
| python3 -m unittest discover -s tests -v | | |
| zam check . | | |
| zam test . | | |
| zam build . | | |
| Arch/Hyprland GUI smoke | | |

## Baseline/HEAD/remote SHA

## Screenshots / benchmark / limitations

## Memory / docs changes
- [ ] `memory/SHORT_TERM.md` updated
- [ ] `memory/MEDIUM_TERM.md` updated if milestone changed
- [ ] Relevant ADR updated if architectural decision changed

## Reviewer focus and rollback
