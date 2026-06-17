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
chmod +x packaging/sync-debian-version.sh
chmod +x packaging/read-version.sh packaging/verify-macos-app-version.sh 2>/dev/null || true

echo "Syncing debian/changelog from VERSION..."
"$(dirname "$0")/sync-debian-version.sh"

UPSTREAM="$( "$(dirname "$0")/read-version.sh" )"
echo "Building git-view deb package (upstream ${UPSTREAM})..."
dpkg-buildpackage -us -uc -b

PARENT="$(dirname "$ROOT")"
echo
echo "Done. Install with:"
echo "  sudo apt install ${PARENT}/git-view_${UPSTREAM}*.deb"
echo
ls -lh "$PARENT"/git-view_"${UPSTREAM}"*.deb 2>/dev/null \
    || ls -lh "$PARENT"/git-view_*.deb 2>/dev/null \
    || true
