#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -Eeuo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
artifact_dir="${ARTIFACT_DIR:-$root/artifacts/p2b}"
mkdir -p "$artifact_dir"
work="$(mktemp -d "$artifact_dir/production-tree.XXXXXX")"
cleanup() { rm -rf -- "$work"; }
trap cleanup EXIT
cd "$root"

version="$(zam --version 2>&1 | tee "$artifact_dir/zamak-version.log")"
[[ "$version" == "0.2.0" ]] || {
  printf 'Expected Zamak 0.2.0, received %s\n' "$version" >&2
  exit 1
}

python3 scripts/verify_project.py 2>&1 | tee "$artifact_dir/project-verification.log"
shell_files=(scripts/*.sh scripts/ci/*.sh)
for file in "${shell_files[@]}"; do bash -n "$file"; done
printf 'Shell syntax: PASS (%s scripts)\n' "${#shell_files[@]}" \
  | tee "$artifact_dir/shell-syntax.log"
python3 -m unittest discover -s tests -v 2>&1 | tee "$artifact_dir/c-safety-tests.log"
zam check . 2>&1 | tee "$artifact_dir/zam-check.log"
zam test . 2>&1 | tee "$artifact_dir/zam-test.log"
zam build . 2>&1 | tee "$artifact_dir/zam-build.log"

binary_rel="$(sed -n 's/^OUTPUT //p' "$artifact_dir/zam-build.log" | tail -n 1)"
[[ -n "$binary_rel" && -x "$binary_rel" ]] || {
  printf 'Zamak did not produce an executable (OUTPUT=%s).\n' "$binary_rel" >&2
  exit 1
}
install -m 0755 "$binary_rel" "$artifact_dir/memreduct-zamak-ci-test"

# Build the user-facing binary from a private source copy with inert production
# runtime hooks. The normal repository manifest intentionally retains GUI test
# controls for Xvfb/CI, so it is not itself the production binary manifest.
cp -a zam.toml src native "$work/"
python3 - "$work/zam.toml" <<'PY'
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
text = path.read_text()
replacements = {
    '"native/gui_test.c"': '"native/gui_runtime_prod.c"',
    '"native/async_privileged.c"': '"native/async_privileged_production.c"',
}
for old, new in replacements.items():
    assert text.count(old) == 1, f"expected exactly one {old} entry"
    text = text.replace(old, new)
path.write_text(text)
PY
cp "$work/zam.toml" "$artifact_dir/production-zam.toml"
grep -q 'gui_runtime_prod.c' "$artifact_dir/production-zam.toml"
grep -q 'async_privileged_production.c' "$artifact_dir/production-zam.toml"
! grep -q 'gui_test.c' "$artifact_dir/production-zam.toml"
cd "$work"
zam build . 2>&1 | tee "$artifact_dir/production-zam-build.log"
production_binary_rel="$(sed -n 's/^OUTPUT //p' "$artifact_dir/production-zam-build.log" | tail -n 1)"
[[ -n "$production_binary_rel" && -x "$production_binary_rel" ]] || {
  printf 'Zamak did not produce a production executable (OUTPUT=%s).\n' "$production_binary_rel" >&2
  exit 1
}
install -m 0755 "$production_binary_rel" "$artifact_dir/memreduct-zamak-production"
nm "$artifact_dir/memreduct-zamak-production" >"$artifact_dir/production-symbols.txt"
if grep -Eq 'mr_gui_test_|mr_ci_emit|mr_start_reclaim_mock|mr_(xproc|cand)_test_' \
  "$artifact_dir/production-symbols.txt"; then
  printf 'Test or fake-authorization symbol leaked into production GUI.\n' >&2
  exit 1
fi
if strings "$artifact_dir/memreduct-zamak-production" | grep -Eq 'MEMREDUCT_CI|MEMREDUCT_TEST'; then
  printf 'Test-only environment hook leaked into production GUI.\n' >&2
  exit 1
fi
cd "$root"
cc -std=c11 -O2 -Wall -Wextra -Werror -pedantic \
  -fstack-protector-strong -D_FORTIFY_SOURCE=2 \
  -o "$artifact_dir/memreduct-zamak-helper" native/memreduct_helper.c

{
  printf 'Zamak=%s\n' "$version"
  printf 'GTK3=%s\n' "$(pkg-config --modversion gtk+-3.0)"
  printf 'Kernel=%s\n' "$(uname -srmo)"
  printf 'Layout source=Mem Reduct v3.5.2 IDD_MAIN 224x214 DLU\n'
  printf 'Settings source=Mem Reduct v3.5.2 five child templates 260x168 DLU; shell 374x214 DLU is PROVISIONAL\n'
  printf 'Provisional conversion=X 1.4 logical px/DLU, Y 1.5 logical px/DLU at nominal 96 DPI; GTK top-levels are fixed-size to preserve the compact canvas\n'
  printf 'Windows font/DPI calibration=NOT RUN; Linux X11 and Wayland surface geometries are recorded separately\n'
  printf 'The 336x348 CI canvas probe is not used as the production window default\n'
} | tee "$artifact_dir/environment.txt"

{
  sha256sum zam.toml src/*.zm native/*.c native/*.h scripts/ci/build-production-gui.sh \
    | sed 's#  #  #'
  sha256sum "$artifact_dir/memreduct-zamak-production" \
    | awk '{print $1 "  memreduct-zamak-production"}'
  sha256sum "$artifact_dir/memreduct-zamak-ci-test" \
    | awk '{print $1 "  memreduct-zamak-ci-test"}'
  sha256sum "$artifact_dir/memreduct-zamak-helper" \
    | awk '{print $1 "  memreduct-zamak-helper"}'
} | tee "$artifact_dir/source-and-binary-sha256.txt"

if [[ -n "${GITHUB_OUTPUT:-}" ]]; then
  printf 'binary=%s\n' "$artifact_dir/memreduct-zamak-production" >>"$GITHUB_OUTPUT"
fi
printf 'Production GUI (no test hooks), CI GUI, and existing C helper built successfully.\n' \
  | tee "$artifact_dir/build-result.txt"
