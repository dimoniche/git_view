#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEFAULT_BINARY="$ROOT/build/src/git_view"

resolve_binary() {
    local candidate="$1"

    if [[ -f "$candidate" && -x "$candidate" ]]; then
        echo "$candidate"
        return 0
    fi

    if [[ -d "$candidate" ]]; then
        for name in git_view bin/git_view src/git_view; do
            local nested="$candidate/$name"
            if [[ -f "$nested" && -x "$nested" ]]; then
                echo "$nested"
                return 0
            fi
        done
    fi

    return 1
}

BINARY_INPUT="${1:-$DEFAULT_BINARY}"
BINARY="$(resolve_binary "$BINARY_INPUT" || true)"

if [[ -z "$BINARY" ]]; then
    echo "git_view binary not found: $BINARY_INPUT" >&2
    echo "Pass the executable or the directory that contains it." >&2
    echo "Examples:" >&2
    echo "  $0" >&2
    echo "  $0 $ROOT/build/src/git_view" >&2
    echo "  $0 /opt/git_view/git_view" >&2
    exit 1
fi

CONFIG_DIR="${XDG_CONFIG_HOME:-"$HOME/.config"}/git_view"
CONFIG_FILE="$CONFIG_DIR/integration.conf"
LAUNCHER_DIR="$HOME/.local/share/git_view"
SCRIPTS_DIR="$HOME/.local/share/nautilus/scripts/GitView"
PYTHON_EXT_DIR="$HOME/.local/share/nautilus-python/extensions"

mkdir -p "$CONFIG_DIR" "$LAUNCHER_DIR" "$SCRIPTS_DIR"
cp "$ROOT/integrations/nautilus/run-git_view.sh" "$LAUNCHER_DIR/run-git_view.sh"
cp "$ROOT/integrations/nautilus/uri_to_path.py" "$LAUNCHER_DIR/uri_to_path.py"
chmod +x "$LAUNCHER_DIR/run-git_view.sh" "$LAUNCHER_DIR/uri_to_path.py"

for script in "$ROOT/integrations/nautilus/scripts/"*; do
    cp "$script" "$SCRIPTS_DIR/"
    chmod +x "$SCRIPTS_DIR/$(basename "$script")"
done

cat >"$CONFIG_FILE" <<EOF
# git_view file manager integration
GIT_VIEW_BIN=$BINARY
EOF

echo "Configured: $CONFIG_FILE"
echo "  GIT_VIEW_BIN=$BINARY"
echo "Installed launcher: $LAUNCHER_DIR/run-git_view.sh"
echo "Installed Nautilus scripts: $SCRIPTS_DIR"

PYTHON_NAUTILUS_READY=false
PYTHON_NAUTILUS_HINT=""

if python3 - <<'PY' >/dev/null 2>&1
import gi
try:
    gi.require_version("Nautilus", "4.0")
except ValueError:
    gi.require_version("Nautilus", "3.0")
from gi.repository import Nautilus
PY
then
    PYTHON_NAUTILUS_READY=true
else
    if command -v apt-cache >/dev/null 2>&1 && apt-cache show python3-nautilus >/dev/null 2>&1; then
        PYTHON_NAUTILUS_HINT="sudo apt install python3-nautilus"
    else
        PYTHON_NAUTILUS_HINT="install the python3-nautilus package for your distribution"
    fi
fi

if [[ "$PYTHON_NAUTILUS_READY" == true ]]; then
    mkdir -p "$PYTHON_EXT_DIR"
    cp "$ROOT/integrations/nautilus/git_view_nautilus.py" "$PYTHON_EXT_DIR/git_view_nautilus.py"
    echo "Installed Nautilus Python extension: $PYTHON_EXT_DIR/git_view_nautilus.py"
    echo "Items appear directly in the context menu (restart Nautilus)."
else
    echo "Nautilus Python bindings are not available — using Scripts/GitView submenu only."
    echo "For TortoiseGit-style menu items run:"
    echo "  $PYTHON_NAUTILUS_HINT"
    echo "Then re-run this installer."
fi

echo
echo "Restart the file manager:"
echo "  nautilus -q"
echo
echo "Context menu locations:"
echo "  - Scripts → GitView → …"
echo "  - (with python3-nautilus) git_view items in the main menu inside repositories"
