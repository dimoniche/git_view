#!/usr/bin/env bash
set -euo pipefail

CONFIG="${XDG_CONFIG_HOME:-"$HOME/.config"}/git_view/integration.conf"
GIT_VIEW_BIN=""
URI_HELPER="${HOME}/.local/share/git_view/uri_to_path.py"

if [[ -f "$CONFIG" ]]; then
    # shellcheck disable=SC1090
    source "$CONFIG"
fi

if [[ -z "${GIT_VIEW_BIN:-}" ]]; then
    if command -v git_view >/dev/null 2>&1; then
        GIT_VIEW_BIN="$(command -v git_view)"
    fi
fi

if [[ -z "${GIT_VIEW_BIN:-}" || ! -x "$GIT_VIEW_BIN" ]]; then
    if command -v zenity >/dev/null 2>&1; then
        zenity --error --text="git_view not found.\n\nRun integrations/install-nautilus-integration.sh and set GIT_VIEW_BIN in:\n$CONFIG"
    else
        echo "git_view binary not found. Set GIT_VIEW_BIN in $CONFIG" >&2
    fi
    exit 1
fi

normalize_path() {
    python3 "$URI_HELPER" "$1"
}

ACTION="${1:?action required}"
shift

declare -a PATHS=()
if [[ $# -gt 0 ]]; then
    for path in "$@"; do
        PATHS+=("$(normalize_path "$path")")
    done
elif [[ -n "${NAUTILUS_SCRIPT_SELECTED_URIS:-}" ]]; then
    while IFS= read -r uri; do
        [[ -n "$uri" ]] && PATHS+=("$(normalize_path "$uri")")
    done <<< "$NAUTILUS_SCRIPT_SELECTED_URIS"
elif [[ -n "${NAUTILUS_SCRIPT_SELECTED_FILE_PATHS:-}" ]]; then
    while IFS= read -r line; do
        [[ -n "$line" ]] && PATHS+=("$(normalize_path "$line")")
    done <<< "$NAUTILUS_SCRIPT_SELECTED_FILE_PATHS"
elif [[ -n "${NAUTILUS_SCRIPT_CURRENT_URI:-}" ]]; then
    PATHS+=("$(normalize_path "$NAUTILUS_SCRIPT_CURRENT_URI")")
fi

REPO=""
if [[ ${#PATHS[@]} -gt 0 ]]; then
    GIT_CWD="${PATHS[0]}"
    if [[ -f "$GIT_CWD" ]]; then
        GIT_CWD="$(dirname "$GIT_CWD")"
    fi
    REPO="$(git -C "$GIT_CWD" rev-parse --show-toplevel 2>/dev/null || true)"
elif [[ -n "${NAUTILUS_SCRIPT_CURRENT_URI:-}" ]]; then
    CURRENT_PATH="$(normalize_path "$NAUTILUS_SCRIPT_CURRENT_URI")"
    REPO="$(git -C "$CURRENT_PATH" rev-parse --show-toplevel 2>/dev/null || true)"
fi

declare -a CMD=("$GIT_VIEW_BIN" "--action" "$ACTION")
if [[ -n "$REPO" ]]; then
    CMD+=("--repo" "$REPO")
fi

if [[ "$ACTION" == "file-history" || "$ACTION" == "file-diff" ]]; then
    for path in "${PATHS[@]}"; do
        CMD+=("--file" "$path")
    done
else
    CMD+=("${PATHS[@]}")
fi

nohup "${CMD[@]}" >/dev/null 2>&1 &
