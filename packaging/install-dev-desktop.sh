#!/usr/bin/env bash
# Install git_view.desktop and icons into ~/.local for Dock/menu when running from build/.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_BIN="${BUILD_BIN:-$ROOT/build/src/git_view}"
ICON_DIR="$HOME/.local/share/icons/hicolor"
APPS_DIR="$HOME/.local/share/applications"

if [[ ! -x "$BUILD_BIN" ]]; then
    echo "Build binary not found: $BUILD_BIN" >&2
    echo "Build first: cmake -B build && cmake --build build" >&2
    exit 1
fi

python3 "$ROOT/resources/icons/render_icon.py"

for size in 48 128 256 512; do
    dest="$ICON_DIR/${size}x${size}/apps"
    mkdir -p "$dest"
    cp "$ROOT/resources/icons/git_view_${size}.png" "$dest/git_view.png"
done

mkdir -p "$APPS_DIR"
cat > "$APPS_DIR/git_view.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=git_view
GenericName=Git repository viewer
Comment=Minimal Git GUI for branch history and working tree changes
Exec=${BUILD_BIN}
Icon=git_view
StartupWMClass=git_view
Terminal=false
Categories=Development;RevisionControl;
Keywords=git;vcs;version control;merge;branch;
StartupNotify=true
EOF

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q "$HOME/.local/share/applications" || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -q "$ICON_DIR" || true
fi

echo "Installed local desktop entry and icons."
echo "Launch from the app menu or: $BUILD_BIN"
