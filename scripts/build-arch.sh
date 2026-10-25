#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"
for cmd in zam cc pkg-config; do
    command -v "$cmd" >/dev/null || { echo "Missing command: $cmd" >&2; exit 1; }
done
pkg-config --exists gtk+-3.0 || { echo 'Missing GTK 3 development files (Arch: sudo pacman -S gtk3 pkgconf)' >&2; exit 1; }
echo 'Checking Zamak modules'
zam check .
echo 'Running Zamak tests'
zam test .
echo 'Building native GUI'
zam build .
mkdir -p build
cc -std=c11 -O2 -Wall -Wextra -Werror -pedantic \
   -fstack-protector-strong -D_FORTIFY_SOURCE=2 \
   -o build/memreduct-zamak-helper native/memreduct_helper.c
echo 'Build successful. Run: sudo bash scripts/install-arch.sh'
