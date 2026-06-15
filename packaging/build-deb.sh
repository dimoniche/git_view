#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if ! command -v dpkg-buildpackage >/dev/null 2>&1; then
    echo "Missing dpkg-buildpackage. Install build tools:" >&2
    echo "  sudo apt install build-essential devscripts debhelper cmake g++ qt6-base-dev" >&2
    exit 1
fi

chmod +x debian/rules debian/postinst debian/postrm 2>/dev/null || true
chmod +x integrations/nautilus/*.sh integrations/nautilus/scripts/* 2>/dev/null || true

echo "Building git-view deb package..."
dpkg-buildpackage -us -uc -b

PARENT="$(dirname "$ROOT")"
echo
echo "Done. Install with:"
echo "  sudo apt install ./git-view_*.deb"
echo
ls -1 "$PARENT"/git-view_*.deb 2>/dev/null || ls -1 "$PARENT"/*.deb 2>/dev/null || true
