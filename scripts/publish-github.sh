#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"
for cmd in gh git python3; do
    command -v "$cmd" >/dev/null || { echo "Missing: $cmd" >&2; exit 1; }
done
python3 scripts/verify_project.py
EXPECTED_OWNER='MilchstraBe-Endless10'
REPO_NAME='memreduct-linux-zamak'
REPO="$EXPECTED_OWNER/$REPO_NAME"
account="$(gh api user --jq '.login')" || { echo 'GitHub CLI not authenticated: gh auth login' >&2; exit 1; }
printf 'Authenticated user: %s\nTarget repository: %s (PUBLIC)\n' "$account" "$REPO"
[[ "$account" == "$EXPECTED_OWNER" ]] || { echo "Wrong account: expected $EXPECTED_OWNER" >&2; exit 1; }
if gh repo view "$REPO" --json name >/dev/null 2>&1; then
    echo "Repo already exists: $REPO. Refusing to overwrite or reinitialize it." >&2
    exit 1
fi
if [[ -d .git ]]; then
    echo 'A local .git directory already exists. Examine branch/remote and publish manually; script will not change it.' >&2
    exit 1
fi
if ! git config user.name >/dev/null || ! git config user.email >/dev/null; then
    echo 'Set git user.name and git user.email before publishing.' >&2
    exit 1
fi
# Prevent untracked secrets from being silently staged.
if find . -type f \( -name '*.pem' -o -name '*.key' -o -name '.env' -o -name '.env.*' -o -name '*id_rsa*' \) | grep -q .; then
    echo 'Potential private configuration or key file found; inspect and remove before public push.' >&2
    exit 1
fi
echo 'First-level public file review:'
find . -maxdepth 2 -type f ! -path './.git/*' | sort
printf '\nPublic repository creation requires confirmation. Type exactly: PUBLISH %s\n' "$REPO"
read -r -p '> ' reply
[[ "$reply" == "PUBLISH $REPO" ]] || { echo 'Canceled'; exit 1; }
git init -b main
git add -A
printf '\nStaged summary:\n'
git diff --cached --stat
git commit -m 'chore: bootstrap Zamak architecture, memory and collaboration workflow'
gh repo create "$REPO" --public --source . --remote origin --push \
    --description 'Unofficial Zamak-native Linux memory utility (GTK3/Polkit); early prototype'
LOCAL="$(git rev-parse HEAD)"
REMOTE="$(git ls-remote origin refs/heads/main | awk '{print $1}')"
[[ "$LOCAL" == "$REMOTE" && -n "$REMOTE" ]] || { echo "SHA MISMATCH local=$LOCAL remote=$REMOTE" >&2; exit 1; }
VISIBILITY="$(gh repo view "$REPO" --json visibility --jq '.visibility')"
[[ "$VISIBILITY" == 'PUBLIC' ]] || { echo "Expected PUBLIC, got $VISIBILITY" >&2; exit 1; }
printf '\nSUCCESS https://github.com/%s\nverified remote main SHA: %s\n' "$REPO" "$REMOTE"
