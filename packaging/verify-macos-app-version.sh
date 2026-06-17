#!/usr/bin/env bash
# Verify git_view.app bundle version matches VERSION.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=read-version.sh
source "$SCRIPT_DIR/read-version.sh"

APP_PATH="${1:-}"
if [[ -z "$APP_PATH" ]]; then
    echo "Usage: $0 /path/to/git_view.app" >&2
    exit 1
fi

PLIST="$APP_PATH/Contents/Info.plist"
if [[ ! -f "$PLIST" ]]; then
    echo "Info.plist not found: $PLIST" >&2
    exit 1
fi

SHORT_VERSION="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$PLIST")"
BUNDLE_VERSION="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleVersion' "$PLIST")"

if [[ "$SHORT_VERSION" != "$VERSION" ]]; then
    echo "CFBundleShortVersionString mismatch: expected $VERSION, got $SHORT_VERSION" >&2
    exit 1
fi

if [[ "$BUNDLE_VERSION" != "$VERSION" ]]; then
    echo "CFBundleVersion mismatch: expected $VERSION, got $BUNDLE_VERSION" >&2
    exit 1
fi

echo "App bundle version OK: $VERSION"
