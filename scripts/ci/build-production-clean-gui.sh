#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Produce the PRODUCTION-CLEAN build: the one build where the GUI can reach
# the pkexec bridge.
#
# zam's [native] table has no defines field, so the capability switch is a
# translation-unit choice instead of a -D flag: the default zam.toml lists
# native/production_clean_gate_default.c (mr_production_clean_enabled() -> 0),
# and this script swaps in production_clean_gate_enabled.c (-> 1) inside a
# private copy of the tree. Nothing in the repository state changes; the swap
# is visible in the build log and verifiable in the resulting ELF with nm.
#
# The helper and the polkit policy are built/staged here too so an installer
# run has every privileged artifact from one auditable origin.
set -Eeuo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
artifact_dir="${ARTIFACT_DIR:-$root/artifacts/p0a}"
mkdir -p "$artifact_dir"
work="$(mktemp -d "$artifact_dir/build-tree.XXXXXX")"
cleanup() { rm -rf -- "$work"; }
trap cleanup EXIT
cd "$root"

python3 scripts/verify_project.py 2>&1 | tee "$artifact_dir/project-verification.log"
for file in scripts/*.sh scripts/ci/*.sh; do bash -n "$file"; done
python3 -m unittest discover -s tests 2>&1 | tee "$artifact_dir/c-safety-tests.log" | tail -1
zam check . 2>&1 | tee "$artifact_dir/zam-check.log"

# The swap, done in the open.
cp -a zam.toml src native "$work/"
python3 - "$work/zam.toml" <<'PY'
import pathlib, sys
p = pathlib.Path(sys.argv[1])
s = p.read_text()
old = "native/production_clean_gate_default.c"
new = "native/production_clean_gate_enabled.c"
assert old in s, "default gate not listed; zam.toml changed upstream?"
for source_old, source_new in (
    ('"native/gui_test.c"', '"native/gui_runtime_prod.c"'),
    ('"native/async_privileged.c"', '"native/async_privileged_production.c"'),
):
    assert s.count(source_old) == 1, f"expected exactly one {source_old} entry"
    s = s.replace(source_old, source_new)
p.write_text(s.replace(old, new))
PY
grep -q 'gui_runtime_prod.c' "$work/zam.toml"
grep -q 'async_privileged_production.c' "$work/zam.toml"
! grep -q 'gui_test.c' "$work/zam.toml"
echo "gate swapped: production_clean_gate_enabled.c" | tee "$artifact_dir/gate-swap.log"

cd "$work"
zam test . 2>&1 | tee "$artifact_dir/zam-test.log" | tail -2
zam build . 2>&1 | tee "$artifact_dir/zam-build.log"
binary_rel="$(sed -n 's/^OUTPUT //p' "$artifact_dir/zam-build.log" | tail -n 1)"
[[ -n "$binary_rel" && -x "$binary_rel" ]] || { echo 'no OUTPUT produced' >&2; exit 1; }
install -m 0755 "$binary_rel" "$artifact_dir/memreduct-zamak-production-clean"

# Binary-level proof of which build this is: the enabled gate must be present
# and the default gate must be absent.
enabled="$(nm "$artifact_dir/memreduct-zamak-production-clean" | grep -c ' T mr_production_clean_enabled' || true)"
[[ "$enabled" == 1 ]] || { echo 'gate symbol missing' >&2; exit 1; }
if nm "$artifact_dir/memreduct-zamak-production-clean" | grep -q 'production_clean_gate_default'; then
  echo 'default gate leaked into production build' >&2; exit 1
fi
nm "$artifact_dir/memreduct-zamak-production-clean" >"$artifact_dir/production-symbols.txt"
if grep -Eq 'mr_gui_test_|mr_ci_emit|mr_start_reclaim_mock|mr_(xproc|cand)_test_' \
  "$artifact_dir/production-symbols.txt"; then
  echo 'test or fake-authorization symbol leaked into production-clean GUI' >&2
  exit 1
fi
if strings "$artifact_dir/memreduct-zamak-production-clean" | grep -Eq 'MEMREDUCT_CI|MEMREDUCT_TEST'; then
  echo 'test-only environment hook leaked into production-clean GUI' >&2
  exit 1
fi
cc -std=c11 -O2 -Wall -Wextra -Werror -pedantic \
   -fstack-protector-strong -D_FORTIFY_SOURCE=2 \
   -o "$artifact_dir/memreduct-zamak-helper" native/memreduct_helper.c

{
  printf 'gate=production_clean_gate_enabled.c\n'
  sha256sum "$artifact_dir/memreduct-zamak-production-clean" "$artifact_dir/memreduct-zamak-helper"
} | tee "$artifact_dir/production-clean-sha256.txt"
printf 'Production-clean GUI + helper built successfully.\n' | tee "$artifact_dir/build-result.txt"
