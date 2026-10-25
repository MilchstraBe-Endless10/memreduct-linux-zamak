#!/usr/bin/env bash
set -euo pipefail
rm -f -- /usr/bin/memreduct-zamak /usr/libexec/memreduct-zamak-helper /usr/share/applications/memreduct-zamak.desktop
rm -f -- /usr/share/licenses/memreduct-zamak/LICENSE /usr/share/doc/memreduct-zamak/THIRD-PARTY-NOTICES.md
printf '%s\n' 'Removed Mem Reduct Zamak files.'
