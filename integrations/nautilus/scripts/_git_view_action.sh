#!/usr/bin/env bash
# Shared helper for Nautilus scripts (deb install + user install).
set -euo pipefail

ACTION="${1:?action required}"
shift || true

if [[ -x /usr/share/git_view/nautilus-action.sh ]]; then
    exec /usr/share/git_view/nautilus-action.sh "$ACTION" "$@"
fi

GIT_VIEW_LIB="${HOME}/.local/share/git_view"
if [[ ! -x "$GIT_VIEW_LIB/run-git_view.sh" ]]; then
    echo "git_view is not installed. Install the git-view package or run integrations/install-nautilus-integration.sh" >&2
    exit 1
fi

exec "$GIT_VIEW_LIB/run-git_view.sh" "$ACTION" "$@"
