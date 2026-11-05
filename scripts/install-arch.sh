#!/usr/bin/env bash
# Install Mem Reduct Zamak: the GUI stays unprivileged, one root-owned helper
# and one Polkit action are the only privileged surface.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

candidate_gui=''
candidate_mode=0
preflight_only=0
usage() {
    printf 'Usage: %s [--m0-m1-candidate ABSOLUTE_GUI_PATH | --check-m0-m1-candidate ABSOLUTE_GUI_PATH]\n' "$0" >&2
    exit 2
}
if (($#)); then
    (($# == 2)) || usage
    case "$1" in
        --m0-m1-candidate) candidate_mode=1 ;;
        --check-m0-m1-candidate) candidate_mode=1; preflight_only=1 ;;
        *) usage ;;
    esac
    candidate_gui="$2"
    [[ "$candidate_gui" == /* ]] || {
        echo 'Candidate GUI path must be absolute.' >&2
        exit 2
    }
fi

helper_source='build/memreduct-zamak-helper'
policy_source='packaging/polkit-1/actions/io.github.milchstrasse_endless10.memreduct.zamak.policy'
# Cross-process reclaim is a separate privileged surface: its own binary, its
# own Polkit action, its own install path. Nothing here may merge the two.
xproc_helper_source='build/memreduct-zamak-crossproc-helper'
xproc_policy_source='packaging/polkit-1/actions/io.github.milchstrasse_endless10.memreduct.zamak.crossproc.policy'
xproc_helper_target='/usr/libexec/memreduct-zamak-crossproc-helper'
xproc_policy_target='/usr/share/polkit-1/actions/io.github.milchstrasse_endless10.memreduct.zamak.crossproc.policy'
xproc_action_id='io.github.milchstrasse_endless10.memreduct.zamak.crossproc'
[[ -f "$helper_source" ]] || { echo 'First run: bash scripts/build-arch.sh' >&2; exit 1; }
[[ -f "$policy_source" ]] || { echo "Missing Polkit action definition: $policy_source" >&2; exit 1; }
# The cross-process helper is optional at install time: a user who only wants
# page-cache cleaning must still be able to install. When it is absent the GUI
# reports "not installed" instead of failing to authorize.
have_xproc=0
if [[ -e "$xproc_helper_source" || -e "$xproc_policy_source" ]]; then
    [[ -f "$xproc_helper_source" && ! -L "$xproc_helper_source" &&
       -f "$xproc_policy_source" && ! -L "$xproc_policy_source" ]] || {
        echo 'Cross-process helper and policy must both be regular, non-symlink files.' >&2
        exit 1
    }
    grep -Eq "^[[:space:]]*<action id=\"$xproc_action_id\">[[:space:]]*$" "$xproc_policy_source" || {
        echo 'Cross-process policy action ID does not match the fixed action.' >&2
        exit 1
    }
    grep -Fq "<annotate key=\"org.freedesktop.policykit.exec.path\">$xproc_helper_target</annotate>" \
        "$xproc_policy_source" || {
        echo 'Cross-process policy does not pin the fixed helper path.' >&2
        exit 1
    }
    if command -v pkexec >/dev/null 2>&1 && command -v pkaction >/dev/null 2>&1; then
        have_xproc=1
    else
        echo 'WARN Polkit pkexec/pkaction is unavailable; cross-process helper will not be installed.' >&2
    fi
fi
if ((candidate_mode)) && ((have_xproc == 0)); then
    echo 'M0+M1 candidate installation requires the cross-process helper, policy, pkexec, and pkaction.' >&2
    exit 1
fi

if ((candidate_mode)); then
    [[ -f "$candidate_gui" && ! -L "$candidate_gui" && -x "$candidate_gui" ]] || {
        echo 'Candidate GUI must be an executable regular non-symlink file.' >&2
        exit 1
    }
    command -v nm >/dev/null 2>&1 && command -v strings >/dev/null 2>&1 || {
        echo 'Candidate validation requires nm and strings.' >&2
        exit 1
    }
    candidate_symbols="$(nm "$candidate_gui")"
    [[ "$(grep -c ' T mr_production_clean_enabled$' <<<"$candidate_symbols")" == 1 &&
       "$(grep -c ' T mr_xproc_capability_enabled$' <<<"$candidate_symbols")" == 1 ]] || {
        echo 'Candidate GUI does not contain both enabled M0 and M1 gates.' >&2
        exit 1
    }
    if grep -Eq 'production_(clean|crossproc)_gate_default|mr_gui_test_|mr_ci_emit|mr_start_reclaim_mock|mr_(xproc|cand)_test_' <<<"$candidate_symbols" ||
       strings "$candidate_gui" | grep -Eq 'MEMREDUCT_CI|MEMREDUCT_TEST'; then
        echo 'Candidate GUI contains a disabled gate or test hook.' >&2
        exit 1
    fi
    bin="$candidate_gui"
else
    bin=''
fi

verify_system_directory() {
    local directory="$1" owner mode permissions
    [[ -d "$directory" && ! -L "$directory" ]] || {
        echo "FAIL system directory is missing or a symlink: $directory" >&2
        return 1
    }
    owner="$(stat -c '%u:%g' -- "$directory")"
    [[ "$owner" == '0:0' ]] || {
        echo "FAIL system directory owner is $owner, expected 0:0: $directory" >&2
        return 1
    }
    mode="$(stat -c '%a' -- "$directory")"
    permissions=$((8#$mode))
    (( (permissions & 0022) == 0 )) || {
        echo "FAIL system directory is group/world writable: $directory" >&2
        return 1
    }
}

verify_existing_target() {
    local target="$1" owner mode expected_mode="$2"
    if [[ -L "$target" ]]; then
        echo "FAIL refusing symlink at privileged target: $target" >&2
        return 1
    fi
    if [[ -e "$target" ]]; then
        [[ -f "$target" ]] || {
            echo "FAIL existing privileged target is not a regular file: $target" >&2
            return 1
        }
        owner="$(stat -c '%u:%g' -- "$target")"
        mode="$(stat -c '%a' -- "$target")"
        [[ "$owner" == '0:0' && "$mode" == "$expected_mode" ]] || {
            echo "FAIL existing privileged target has unexpected owner/mode: $target ($owner $mode)" >&2
            return 1
        }
    fi
}

if ((have_xproc)) && ((preflight_only == 0)); then
    for directory in /usr /usr/libexec /usr/share /usr/share/polkit-1 /usr/share/polkit-1/actions; do
        verify_system_directory "$directory"
    done
    verify_existing_target "$xproc_helper_target" '755'
    verify_existing_target "$xproc_policy_target" '644'
fi

if ((candidate_mode == 0)); then
    mapfile -t candidates < <(find build -maxdepth 1 -type f -name 'memreduct_zamak-[0-9a-f]*' -perm /111 | sort)
    ((${#candidates[@]})) || { echo 'Zamak GUI binary not found in build/' >&2; exit 1; }
    if ((${#candidates[@]} > 1)); then
        echo 'Multiple Zamak GUI binaries found. Select a profile or rebuild in a clean tree.' >&2
        exit 1
    fi
    bin="${candidates[0]}"
fi
if ((preflight_only)); then
    printf 'Candidate preflight passed: profile=m0+m1-candidate pagecache_gate=enabled crossproc_gate=enabled no_test_hooks=true install=not_run\n'
    exit 0
fi

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

if ((have_xproc)); then
    install -m 0755 -D "$xproc_helper_source" "$stage/memreduct-zamak-crossproc-helper"
    install -m 0644 -D "$xproc_policy_source" "$stage/memreduct-zamak-crossproc.policy"
    chown 0:0 "$stage/memreduct-zamak-crossproc-helper"
    chmod 0755 "$stage/memreduct-zamak-crossproc-helper"
    install -m 0755 -D "$stage/memreduct-zamak-crossproc-helper" "$xproc_helper_target"
    install -m 0644 -D "$stage/memreduct-zamak-crossproc.policy" \
        "$xproc_policy_target"
fi

# Re-assert ownership/permissions on the installed helpers: install(1) is not a
# security boundary, and a wrong owner would let a normal user swap the binary.
chown 0:0 /usr/libexec/memreduct-zamak-helper
chmod 0755 /usr/libexec/memreduct-zamak-helper
chown 0:0 /usr/share/polkit-1/actions/io.github.milchstrasse_endless10.memreduct.zamak.policy
chmod 0644 /usr/share/polkit-1/actions/io.github.milchstrasse_endless10.memreduct.zamak.policy
if ((have_xproc)); then
    chown 0:0 "$xproc_helper_target"
    chmod 0755 "$xproc_helper_target"
    chown 0:0 "$xproc_policy_target"
    chmod 0644 "$xproc_policy_target"
fi

# Verify the privileged surface before declaring success.
fail=0
owner="$(stat -c '%u:%g' /usr/libexec/memreduct-zamak-helper)"
[[ "$owner" == '0:0' ]] || { echo "FAIL helper owner is $owner, expected 0:0" >&2; fail=1; }
mode="$(stat -c '%a' /usr/libexec/memreduct-zamak-helper)"
[[ "$mode" == '755' ]] || { echo "FAIL helper mode is $mode, expected 755" >&2; fail=1; }
if find /usr/libexec /usr/bin -maxdepth 1 -name 'memreduct-zamak-helper' -type l | grep -q .; then
    echo 'FAIL helper path is a symlink' >&2; fail=1
fi
if ((have_xproc)); then
    xowner="$(stat -c '%u:%g' "$xproc_helper_target")"
    [[ "$xowner" == '0:0' ]] || { echo "FAIL crossproc helper owner is $xowner, expected 0:0" >&2; fail=1; }
    xmode="$(stat -c '%a' "$xproc_helper_target")"
    [[ "$xmode" == '755' ]] || { echo "FAIL crossproc helper mode is $xmode, expected 755" >&2; fail=1; }
    xpolicy_owner="$(stat -c '%u:%g' "$xproc_policy_target")"
    [[ "$xpolicy_owner" == '0:0' ]] || { echo "FAIL crossproc policy owner is $xpolicy_owner, expected 0:0" >&2; fail=1; }
    xpolicy_mode="$(stat -c '%a' "$xproc_policy_target")"
    [[ "$xpolicy_mode" == '644' ]] || { echo "FAIL crossproc policy mode is $xpolicy_mode, expected 644" >&2; fail=1; }
    if [[ -L "$xproc_helper_target" || -L "$xproc_policy_target" ]]; then
        echo 'FAIL crossproc helper path is a symlink' >&2; fail=1
    fi
    if ! cmp -s "$xproc_policy_source" "$xproc_policy_target"; then
        echo 'FAIL installed cross-process policy differs from the verified source.' >&2
        fail=1
    fi
    if ! grep -Eq "^[[:space:]]*<action id=\"$xproc_action_id\">[[:space:]]*$" \
        "$xproc_policy_target" ||
       ! grep -Fq "<annotate key=\"org.freedesktop.policykit.exec.path\">$xproc_helper_target</annotate>" \
        "$xproc_policy_target"; then
        echo 'FAIL installed cross-process policy does not bind the dedicated action to the fixed helper.' >&2
        fail=1
    fi
    if ! pkaction --action-id "$xproc_action_id" >/dev/null 2>&1; then
        echo "FAIL Polkit does not report the dedicated action $xproc_action_id" >&2
        fail=1
    fi
else
    echo 'WARN cross-process helper not built; cross-process reclaim will report "not installed"' >&2
fi
if command -v pkexec >/dev/null 2>&1; then
    pkexec --version >/dev/null 2>&1 || { echo 'FAIL pkexec is present but not runnable' >&2; fail=1; }
else
    echo 'WARN pkexec not found; install polkit before using Clean memory' >&2
fi
((fail == 0)) || { echo 'Installation verification failed.' >&2; exit 1; }

printf '%s\n' 'Installed. Run memreduct-zamak as a normal user (never sudo).'
printf '%s\n' 'Clean memory asks for authorization through Polkit for each request.'
