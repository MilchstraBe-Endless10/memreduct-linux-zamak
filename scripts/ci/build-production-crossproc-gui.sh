#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Build the normal opt-in M1.3 GUI candidate without CI/test hooks. This does
# not install anything or run a reclaim request. The default zam.toml keeps both
# privileged capabilities disabled.
set -Eeuo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
artifact_dir="${ARTIFACT_DIR:-$root/artifacts/p3c-crossproc}"
mkdir -p "$artifact_dir"
work="$(mktemp -d "$artifact_dir/build-tree.XXXXXX")"
cleanup() { rm -rf -- "$work"; }
trap cleanup EXIT
cd "$root"

version="$(zam --version 2>&1 | tee "$artifact_dir/zamak-version.log")"
[[ "$version" == "0.2.0" ]] || {
  printf 'Expected Zamak 0.2.0, received %s\n' "$version" >&2
  exit 1
}
python3 scripts/verify_project.py 2>&1 | tee "$artifact_dir/project-verification.log"
for file in scripts/*.sh scripts/ci/*.sh; do bash -n "$file"; done
python3 -m unittest discover -s tests -v 2>&1 | tee "$artifact_dir/c-safety-tests.log"
zam check . 2>&1 | tee "$artifact_dir/zam-check.log"

cp -a zam.toml src native "$work/"
python3 - "$work/zam.toml" <<'PY'
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
text = path.read_text()
replacements = {
    '"native/gui_test.c"': '"native/gui_runtime_prod.c"',
    '"native/async_privileged.c"': '"native/async_privileged_production.c"',
    '"native/production_crossproc_gate_default.c"': '"native/production_crossproc_gate_enabled.c"',
}
for old, new in replacements.items():
    assert text.count(old) == 1, f"expected exactly one {old} entry"
    text = text.replace(old, new)
assert '"native/production_clean_gate_default.c"' in text
assert '"native/production_clean_gate_enabled.c"' not in text
path.write_text(text)
PY
cp "$work/zam.toml" "$artifact_dir/zam.toml"
grep -q 'production_crossproc_gate_enabled.c' "$artifact_dir/zam.toml"
grep -q 'production_clean_gate_default.c' "$artifact_dir/zam.toml"
! grep -q 'production_clean_gate_enabled.c' "$artifact_dir/zam.toml"
grep -q 'gui_runtime_prod.c' "$artifact_dir/zam.toml"
grep -q 'async_privileged_production.c' "$artifact_dir/zam.toml"
! grep -q 'gui_test.c' "$artifact_dir/zam.toml"
printf 'crossproc_gate=enabled\npagecache_gate=default-disabled\nruntime=production-no-test-hooks\n' \
  | tee "$artifact_dir/gate-selection.txt"

cd "$work"
zam test . 2>&1 | tee "$artifact_dir/zam-test.log"
zam build . 2>&1 | tee "$artifact_dir/zam-build.log"
binary_rel="$(sed -n 's/^OUTPUT //p' "$artifact_dir/zam-build.log" | tail -n 1)"
[[ -n "$binary_rel" && -x "$binary_rel" ]] || {
  printf 'Zamak did not produce an executable (OUTPUT=%s).\n' "$binary_rel" >&2
  exit 1
}
binary="$artifact_dir/memreduct-zamak-production-crossproc-candidate"
install -m 0755 "$binary_rel" "$binary"

nm "$binary" >"$artifact_dir/symbols.txt"
grep -q ' T mr_xproc_capability_enabled$' "$artifact_dir/symbols.txt"
! grep -q 'production_crossproc_gate_default' "$artifact_dir/symbols.txt"
grep -q ' T mr_production_clean_enabled$' "$artifact_dir/symbols.txt"
if grep -Eq 'mr_gui_test_|mr_ci_emit|mr_start_reclaim_mock|mr_(xproc|cand)_test_' "$artifact_dir/symbols.txt"; then
  printf 'Test or fake-authorization symbol leaked into production candidate.\n' >&2
  exit 1
fi
if strings "$binary" | grep -Eq 'MEMREDUCT_CI|MEMREDUCT_TEST'; then
  printf 'Test-only environment hook leaked into production candidate.\n' >&2
  exit 1
fi
sha256sum "$binary" | tee "$artifact_dir/binary-sha256.txt"
printf 'Built an opt-in production candidate without test hooks; no install or reclaim request was run.\n' \
  | tee "$artifact_dir/build-result.txt"
