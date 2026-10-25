# Project status / releases truth

At 2026-10-25 the checked-in source represents the v0.2.0 prototype with governance added, not a stable app release.

| Activity | Status | Evidence |
|---|---|---|
| C helper unit/integration tests | PASS (3/3) | `python3 -m unittest discover -s tests -v` in authoring environment |
| Shell syntax and governance verification | PASS (current package) | `bash -n`, `python3 scripts/verify_project.py` |
| Zamak check/build/UI | NOT_RUN | `zam` not installed; npm registry lookup failed EAI_AGAIN |
| Omarchy/Hyprland real GUI | NOT_RUN | no authorized target machine in current environment |
| GitHub new public repo | NOT_DONE | create_repo unsupported by connector; browser use credits insufficient |
| Git commit/push to main | NOT_DONE | no remote repository created |
| Release binary | NOT_DONE | cannot release an unverified GUI |

Open risks are detailed in `memory/SHORT_TERM.md` and `docs/SECURITY.md`.
