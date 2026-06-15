#!/usr/bin/env bash
set -euo pipefail

ACTION="${1:?action required}"
shift || true

GIT_VIEW_LIB="/usr/share/git_view"
if [[ ! -x "$GIT_VIEW_LIB/run-git_view.sh" ]]; then
    GIT_VIEW_LIB="${HOME}/.local/share/git_view"
fi

if [[ ! -x "$GIT_VIEW_LIB/run-git_view.sh" ]]; then
    echo "git_view launcher not found (expected $GIT_VIEW_LIB/run-git_view.sh)" >&2
    exit 1
fi

exec "$GIT_VIEW_LIB/run-git_view.sh" "$ACTION" "$@"
