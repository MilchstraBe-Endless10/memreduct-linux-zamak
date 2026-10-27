#!/usr/bin/env bash
set -uo pipefail

usage() {
    printf 'Usage: %s update | install PACKAGE...\n' "$0" >&2
    exit 64
}

[[ $# -ge 1 ]] || usage
mode=$1
shift
[[ $mode == update || $mode == install ]] || usage
[[ $mode != install || $# -gt 0 ]] || usage

diagnostics_dir=${APT_DIAGNOSTICS_DIR:?APT_DIAGNOSTICS_DIR must point to an artifact directory}
mkdir -p "$diagnostics_dir" || exit 73
umask 022

if [[ $mode == update ]]; then
    limit=${APT_UPDATE_TIMEOUT:-8m}
else
    limit=${APT_INSTALL_TIMEOUT:-38m}
fi
[[ $limit =~ ^[0-9]+[smh]$ ]] || {
    printf 'Invalid APT timeout value: %s\n' "$limit" >&2
    exit 64
}

log_file="$diagnostics_dir/apt-${mode}.log"
metadata_file="$diagnostics_dir/runner-and-apt.txt"
summary_file="$diagnostics_dir/apt-${mode}-result.txt"
packages=()
[[ $mode != install ]] || packages=("$@")

{
    printf 'operation=%s\n' "$mode"
    printf 'utc=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    printf 'runner_image=%s %s\n' "${ImageOS:-unknown}" "${ImageVersion:-unknown}"
    printf 'runner_arch=%s\n' "$(dpkg --print-architecture 2>&1)"
    printf '\n--- /etc/os-release ---\n'
    cat /etc/os-release 2>&1
    printf '\n--- kernel ---\n'
    uname -a 2>&1
    printf '\n--- apt version ---\n'
    apt-get --version 2>&1 | sed -n '1p'
    printf '\n--- apt source definitions (credentials redacted) ---\n'
    for source in /etc/apt/sources.list /etc/apt/sources.list.d/*.list /etc/apt/sources.list.d/*.sources; do
        [[ -f $source ]] || continue
        printf '[%s]\n' "${source##*/}"
        sed -E 's#(https?://)[^/@[:space:]]+:[^/@[:space:]]+@#\1[redacted]@#g' "$source"
    done
    printf '\n--- requested packages ---\n'
    printf '%s\n' "${packages[@]:-(none)}"
} >>"$metadata_file" 2>&1

if [[ $mode == install ]]; then
    {
        for package in "${packages[@]}"; do
            printf '\n--- apt-cache policy %s ---\n' "$package"
            apt-cache policy "$package" 2>&1
        done
    } >>"$diagnostics_dir/package-policy.txt"
fi

apt_options=(
    -o Acquire::Retries=3
    -o Acquire::http::Timeout=30
    -o Acquire::https::Timeout=30
    -o DPkg::Lock::Timeout=180
)
if [[ $mode == update ]]; then
    command=(apt-get "${apt_options[@]}" update)
else
    command=(apt-get "${apt_options[@]}" install --yes --no-install-recommends "${packages[@]}")
fi
printf 'Running with hard timeout %s: ' "$limit" | tee "$log_file"
printf '%q ' "${command[@]}" | tee -a "$log_file"
printf '\n' | tee -a "$log_file"

export DEBIAN_FRONTEND=noninteractive
export APT_LISTCHANGES_FRONTEND=none
set +e
timeout --foreground --signal=TERM --kill-after=30s "$limit" "${command[@]}" 2>&1 | tee -a "$log_file"
pipeline_status=("${PIPESTATUS[@]}")
apt_status=${pipeline_status[0]:-255}
tee_status=${pipeline_status[1]:-255}

if [[ $mode == install ]]; then
    {
        for package in "${packages[@]}"; do
            printf '\n--- installed version: %s ---\n' "$package"
            if dpkg-query -W -f='${binary:Package} ${Version} ${Status}\n' "$package" 2>&1; then
                :
            else
                query_status=$?
                printf 'dpkg-query exit=%s\n' "$query_status"
            fi
        done
    } >>"$diagnostics_dir/package-versions.txt" 2>&1
fi

final_status=$apt_status
if [[ $apt_status -eq 0 && $tee_status -ne 0 ]]; then
    final_status=$tee_status
    classification='failed to preserve complete command log'
elif [[ $apt_status -eq 0 ]]; then
    classification=PASS
else
    classification='unclassified failure'
    if [[ $apt_status -eq 124 || $apt_status -eq 137 ]]; then
        classification='command timeout'
    elif grep -Eqi 'Could not get lock|Unable to acquire.*lock|lock .*held by process|Resource temporarily unavailable' "$log_file"; then
        classification='APT/dpkg package-manager lock'
    elif grep -Eqi 'Temporary failure resolving|Could not resolve|Name or service not known|Temporary failure in name resolution' "$log_file"; then
        classification='DNS resolution failure'
    elif grep -Eqi 'Unable to locate package|has no installation candidate|E: Package .* not found' "$log_file"; then
        classification='package unavailable in configured repositories'
    elif grep -Eqi 'Failed to fetch|Connection timed out|Could not connect|Network is unreachable|TLS connection|Connection failed' "$log_file"; then
        classification='repository/network connection failure'
    elif grep -Eqi 'Unmet dependencies|held broken packages|dependency problems' "$log_file"; then
        classification='package dependency resolution failure'
    fi
fi

{
    printf 'operation=%s\n' "$mode"
    printf 'apt_exit_code=%s\n' "$apt_status"
    printf 'log_writer_exit_code=%s\n' "$tee_status"
    printf 'exit_code=%s\n' "$final_status"
    printf 'classification=%s\n' "$classification"
    printf 'log=%s\n' "${log_file##*/}"
} | tee "$summary_file"

exit "$final_status"
