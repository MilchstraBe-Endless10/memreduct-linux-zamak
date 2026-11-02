#!/usr/bin/env bash
# Remove every file install-arch.sh places, including the Polkit action.
set -euo pipefail
rm -f -- /usr/bin/memreduct-zamak \
    /usr/libexec/memreduct-zamak-helper \
    /usr/libexec/memreduct-zamak-crossproc-helper \
    /usr/share/applications/memreduct-zamak.desktop \
    /usr/share/polkit-1/actions/io.github.milchstrasse_endless10.memreduct.zamak.policy \
    /usr/share/polkit-1/actions/io.github.milchstrasse_endless10.memreduct.zamak.crossproc.policy
rm -f -- /usr/share/licenses/memreduct-zamak/LICENSE /usr/share/doc/memreduct-zamak/THIRD-PARTY-NOTICES.md
rmdir --ignore-fail-on-non-empty /usr/share/licenses/memreduct-zamak \
    /usr/share/doc/memreduct-zamak 2>/dev/null || true
printf '%s\n' 'Removed Mem Reduct Zamak files, including its Polkit action.'
