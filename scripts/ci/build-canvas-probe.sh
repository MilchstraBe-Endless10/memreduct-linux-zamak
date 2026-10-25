#!/usr/bin/env bash
set -Eeuo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
probe_dir="$root/tests/gui/canvas_probe"
artifact_dir="${ARTIFACT_DIR:-$root/artifacts}"
mkdir -p "$artifact_dir/probe"

cd "$probe_dir"
version="$(zam --version 2>&1 | tee "$artifact_dir/zamak-version.log")"
[[ "$version" == "0.2.0" ]] || {
  printf 'Expected Zamak 0.2.0, got: %s\n' "$version" >&2
  exit 1
}

sha256sum --check source-sha256.txt | tee "$artifact_dir/source-hash-check.log"
zam check . 2>&1 | tee "$artifact_dir/zam-check.log"
zam test . 2>&1 | tee "$artifact_dir/zam-test.log"
pass_count="$(grep -Ec '^test test_.* \.\.\. ok$' "$artifact_dir/zam-test.log")"
[[ "$pass_count" == "9" ]] || {
  printf 'Expected all 9 preserved event logic tests; saw %s passing lines.\n' "$pass_count" >&2
  exit 1
}
zam build . 2>&1 | tee "$artifact_dir/zam-build.log"

binary_rel="$(sed -n 's/^OUTPUT //p' "$artifact_dir/zam-build.log" | tail -n 1)"
[[ -n "$binary_rel" && -x "$binary_rel" ]] || {
  printf 'Zamak did not produce an executable from OUTPUT line: %s\n' "$binary_rel" >&2
  exit 1
}
install -m 0755 "$binary_rel" "$artifact_dir/probe/p2a_r4_canvas_probe"
{
  printf 'Zamak: %s\n' "$version"
  printf 'GTK3: %s\n' "$(pkg-config --modversion gtk+-3.0)"
  printf 'Host kernel/architecture: %s\n' "$(uname -srmo)"
  printf 'Preserved Zamak logic tests: %s/9\n' "$pass_count"
} | tee "$artifact_dir/environment.txt"
{
  sha256sum src/main.zm | awk '{print $1 "  src/main.zm"}'
  sha256sum "$artifact_dir/probe/p2a_r4_canvas_probe" \
    | awk '{print $1 "  p2a_r4_canvas_probe"}'
} | tee "$artifact_dir/sha256sums.txt"
