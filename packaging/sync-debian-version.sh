#!/usr/bin/env bash
# Sync debian/changelog version from the root VERSION file.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=read-version.sh
source "$SCRIPT_DIR/read-version.sh"

CHANGELOG="$ROOT/debian/changelog"
UPSTREAM="$VERSION"

if [[ ! -f "$CHANGELOG" ]]; then
    echo "Missing debian/changelog: $CHANGELOG" >&2
    exit 1
fi

if [[ -z "$UPSTREAM" ]]; then
    echo "VERSION file is empty: $VERSION_FILE" >&2
    exit 1
fi

CURRENT="$(sed -n 's/^git-view (\([^)]*\)).*/\1/p' "$CHANGELOG" | head -1)"
if [[ -z "$CURRENT" ]]; then
    echo "Could not parse version from debian/changelog" >&2
    exit 1
fi

CURRENT_UPSTREAM="${CURRENT%-*}"
CURRENT_REV="${CURRENT##*-}"

if [[ "$CURRENT_UPSTREAM" == "$UPSTREAM" ]]; then
    if [[ "$CURRENT_REV" =~ ^[0-9]+$ ]]; then
        NEW_REV=$((CURRENT_REV + 1))
    else
        NEW_REV=1
    fi
    NEW_VERSION="${UPSTREAM}-${NEW_REV}"
    MESSAGE="Rebuild package."
else
    NEW_VERSION="${UPSTREAM}-1"
    MESSAGE="Release ${UPSTREAM}."
fi

export DEBFULLNAME="${DEBFULLNAME:-git_view}"
export DEBEMAIL="${DEBEMAIL:-git_view@localhost}"

if command -v dch >/dev/null 2>&1; then
    dch --newversion "$NEW_VERSION" -D unstable "$MESSAGE"
else
  # devscripts not installed — update only the top changelog line.
    sed -i "1s/^git-view (.*/git-view (${NEW_VERSION}) unstable; urgency=medium/" "$CHANGELOG"
    if ! grep -q "^  \* ${MESSAGE}$" "$CHANGELOG"; then
        sed -i "1a\\
\\
  * ${MESSAGE}
" "$CHANGELOG"
    fi
fi

echo "Debian package version: ${NEW_VERSION} (upstream ${UPSTREAM} from VERSION)"
