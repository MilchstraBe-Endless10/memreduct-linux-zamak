#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"
[[ -f build/memreduct-zamak-helper ]] || { echo 'First run: bash scripts/build-arch.sh' >&2; exit 1; }
mapfile -t candidates < <(find build -maxdepth 1 -type f -name 'memreduct_zamak-[0-9a-f]*' -perm /111 | sort)
((${#candidates[@]})) || { echo 'Zamak GUI binary not found in build/' >&2; exit 1; }
if ((${#candidates[@]} > 1)); then
    echo 'Multiple GUI binaries found. Remove stale build binaries and run build-arch.sh again.' >&2
    exit 1
fi
bin="${candidates[0]}"
echo "Installing native GUI: $bin"
install -m 0755 -D "$bin" /usr/bin/memreduct-zamak
install -m 0755 -D build/memreduct-zamak-helper /usr/libexec/memreduct-zamak-helper
install -m 0644 -D packaging/memreduct-zamak.desktop /usr/share/applications/memreduct-zamak.desktop
install -m 0644 -D LICENSE /usr/share/licenses/memreduct-zamak/LICENSE
install -m 0644 -D THIRD-PARTY-NOTICES.md /usr/share/doc/memreduct-zamak/THIRD-PARTY-NOTICES.md
printf '%s\n' 'Installed. Run memreduct-zamak as a normal user (never sudo).'
