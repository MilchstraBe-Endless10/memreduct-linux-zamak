#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -Eeuo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
artifact_dir="${ARTIFACT_DIR:-$root/artifacts/p2b}"
mkdir -p "$artifact_dir"
cd "$root"

version="$(zam --version 2>&1 | tee "$artifact_dir/zamak-version.log")"
[[ "$version" == "0.2.0" ]] || {
  printf 'Expected Zamak 0.2.0, received %s\n' "$version" >&2
  exit 1
}

python3 scripts/verify_project.py 2>&1 | tee "$artifact_dir/project-verification.log"
for file in scripts/*.sh; do bash -n "$file"; done
printf 'Shell syntax: PASS (%s scripts)\n' "$(find scripts -maxdepth 1 -name '*.sh' -type f | wc -l)" \
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
install -m 0755 "$binary_rel" "$artifact_dir/memreduct-zamak-production"
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
  sha256sum src/main.zm src/settings_ui.zm src/settings_resources.zm src/ui_geometry.zm native/gui_test.c native/gui_window.c zam.toml \
    | sed 's#  #  #'
  sha256sum "$artifact_dir/memreduct-zamak-production" \
    | awk '{print $1 "  memreduct-zamak-production"}'
  sha256sum "$artifact_dir/memreduct-zamak-helper" \
    | awk '{print $1 "  memreduct-zamak-helper"}'
} | tee "$artifact_dir/source-and-binary-sha256.txt"

if [[ -n "${GITHUB_OUTPUT:-}" ]]; then
  printf 'binary=%s\n' "$artifact_dir/memreduct-zamak-production" >>"$GITHUB_OUTPUT"
fi
printf 'Production Zamak GUI and existing C helper built successfully.\n' \
  | tee "$artifact_dir/build-result.txt"
