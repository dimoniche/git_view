#!/usr/bin/env bash
# Resolve git_view executable for Nautilus integration.
# Prints the binary path on stdout; exits 1 if not found.
#
# Priority:
#   1. git_view on PATH in a system location (/usr/bin, /usr/local/bin, /opt/…)
#      — matches the .desktop entry and survives deb package upgrades
#   2. GIT_VIEW_BIN from ~/.config/git_view/integration.conf (dev / custom builds)
#   3. GIT_VIEW_BIN from /usr/share/git_view/integration.conf (packaged default)
#   4. any other git_view on PATH
set -euo pipefail

read_config_bin() {
    local file="$1"
    [[ -f "$file" ]] || return 1

    local line key value
    while IFS= read -r line || [[ -n "$line" ]]; do
        line="${line%%#*}"
        line="${line#"${line%%[![:space:]]*}"}"
        line="${line%"${line##*[![:space:]]}"}"
        [[ -z "$line" ]] || [[ "$line" != GIT_VIEW_BIN=* ]] && continue

        value="${line#GIT_VIEW_BIN=}"
        value="${value%\"}"
        value="${value#\"}"
        value="${value%\'}"
        value="${value#\'}"
        if [[ -n "$value" && -x "$value" ]]; then
            echo "$value"
            return 0
        fi
    done <"$file"
    return 1
}

is_system_binary() {
    case "$1" in
        /usr/bin/* | /usr/local/bin/* | /opt/git_view/* | /opt/*/bin/git_view)
            return 0
            ;;
    esac
    return 1
}

USER_CONFIG="${XDG_CONFIG_HOME:-"$HOME/.config"}/git_view/integration.conf"
SYSTEM_CONFIG="/usr/share/git_view/integration.conf"

PATH_BIN=""
if command -v git_view >/dev/null 2>&1; then
    PATH_BIN="$(command -v git_view)"
fi

USER_BIN="$(read_config_bin "$USER_CONFIG" || true)"
SYSTEM_BIN="$(read_config_bin "$SYSTEM_CONFIG" || true)"

if [[ -n "$PATH_BIN" && -x "$PATH_BIN" ]]; then
    if is_system_binary "$PATH_BIN"; then
        echo "$PATH_BIN"
        exit 0
    fi
    # Stale manual install config (build tree under $HOME) after a deb upgrade.
    if [[ -n "$USER_BIN" && "$USER_BIN" == "$HOME"/* ]]; then
        echo "$PATH_BIN"
        exit 0
    fi
fi

if [[ -n "$USER_BIN" ]]; then
    echo "$USER_BIN"
    exit 0
fi

if [[ -n "$SYSTEM_BIN" ]]; then
    echo "$SYSTEM_BIN"
    exit 0
fi

if [[ -n "$PATH_BIN" && -x "$PATH_BIN" ]]; then
    echo "$PATH_BIN"
    exit 0
fi

exit 1
