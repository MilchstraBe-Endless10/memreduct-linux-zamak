#!/usr/bin/env bash
# Install Mem Reduct Zamak: the GUI stays unprivileged, one root-owned helper
# and one Polkit action are the only privileged surface.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

helper_source='build/memreduct-zamak-helper'
policy_source='packaging/polkit-1/actions/io.github.milchstrasse_endless10.memreduct.zamak.policy'
[[ -f "$helper_source" ]] || { echo 'First run: bash scripts/build-arch.sh' >&2; exit 1; }
[[ -f "$policy_source" ]] || { echo "Missing Polkit action definition: $policy_source" >&2; exit 1; }

mapfile -t candidates < <(find build -maxdepth 1 -type f -name 'memreduct_zamak-[0-9a-f]*' -perm /111 | sort)
((${#candidates[@]})) || { echo 'Zamak GUI binary not found in build/' >&2; exit 1; }
if ((${#candidates[@]} > 1)); then
    echo 'Multiple GUI binaries found. Remove stale build binaries and run build-arch.sh again.' >&2
    exit 1
fi
bin="${candidates[0]}"

# Install into a private staging directory first, then move into place, so a
# failure part-way cannot leave a half-written privileged helper behind.
stage="$(mktemp -d)"
trap 'rm -rf -- "$stage"' EXIT

echo "Installing native GUI: $bin"
install -m 0755 -D "$bin" "$stage/memreduct-zamak"
install -m 0755 -D "$helper_source" "$stage/memreduct-zamak-helper"
install -m 0644 -D packaging/memreduct-zamak.desktop "$stage/memreduct-zamak.desktop"
install -m 0644 -D LICENSE "$stage/LICENSE"
install -m 0644 -D THIRD-PARTY-NOTICES.md "$stage/THIRD-PARTY-NOTICES.md"
install -m 0644 -D "$policy_source" "$stage/memreduct-zamak.policy"

# The helper is the only root-executed file: force root:root 0755 explicitly
# rather than inheriting whatever the build directory holds.
chown 0:0 "$stage/memreduct-zamak-helper"
chmod 0755 "$stage/memreduct-zamak-helper"

install -m 0755 -D "$stage/memreduct-zamak" /usr/bin/memreduct-zamak
install -m 0755 -D "$stage/memreduct-zamak-helper" /usr/libexec/memreduct-zamak-helper
install -m 0644 -D "$stage/memreduct-zamak.desktop" /usr/share/applications/memreduct-zamak.desktop
install -m 0644 -D "$stage/LICENSE" /usr/share/licenses/memreduct-zamak/LICENSE
install -m 0644 -D "$stage/THIRD-PARTY-NOTICES.md" /usr/share/doc/memreduct-zamak/THIRD-PARTY-NOTICES.md
install -m 0644 -D "$stage/memreduct-zamak.policy" \
    /usr/share/polkit-1/actions/io.github.milchstrasse_endless10.memreduct.zamak.policy

# Re-assert ownership/permissions on the installed helper: install(1) is not a
# security boundary, and a wrong owner would let a normal user swap the binary.
chown 0:0 /usr/libexec/memreduct-zamak-helper
chmod 0755 /usr/libexec/memreduct-zamak-helper
chown 0:0 /usr/share/polkit-1/actions/io.github.milchstrasse_endless10.memreduct.zamak.policy
chmod 0644 /usr/share/polkit-1/actions/io.github.milchstrasse_endless10.memreduct.zamak.policy

# Verify the privileged surface before declaring success.
fail=0
owner="$(stat -c '%u:%g' /usr/libexec/memreduct-zamak-helper)"
[[ "$owner" == '0:0' ]] || { echo "FAIL helper owner is $owner, expected 0:0" >&2; fail=1; }
mode="$(stat -c '%a' /usr/libexec/memreduct-zamak-helper)"
[[ "$mode" == '755' ]] || { echo "FAIL helper mode is $mode, expected 755" >&2; fail=1; }
if find /usr/libexec /usr/bin -maxdepth 1 -name 'memreduct-zamak-helper' -type l | grep -q .; then
    echo 'FAIL helper path is a symlink' >&2; fail=1
fi
if command -v pkexec >/dev/null 2>&1; then
    pkexec --version >/dev/null 2>&1 || { echo 'FAIL pkexec is present but not runnable' >&2; fail=1; }
else
    echo 'WARN pkexec not found; install polkit before using Clean memory' >&2
fi
((fail == 0)) || { echo 'Installation verification failed.' >&2; exit 1; }

printf '%s\n' 'Installed. Run memreduct-zamak as a normal user (never sudo).'
printf '%s\n' 'Clean memory asks for authorization through Polkit for each request.'
