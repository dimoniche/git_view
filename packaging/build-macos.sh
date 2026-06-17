#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=read-version.sh
source "$SCRIPT_DIR/read-version.sh"

cd "$ROOT"

BUILD_DIR="${BUILD_DIR:-$ROOT/build-macos}"
OUTPUT_DIR="${OUTPUT_DIR:-$ROOT/dist}"
APP_NAME="git_view.app"
APP_PATH="$BUILD_DIR/src/$APP_NAME"
DMG_NAME="git_view-${VERSION}-macos.dmg"
DMG_PATH="$OUTPUT_DIR/$DMG_NAME"

resolve_qt_prefix() {
    if [[ -n "${QT_PREFIX:-}" && -d "$QT_PREFIX/lib/cmake/Qt6" ]]; then
        echo "$QT_PREFIX"
        return 0
    fi

    if command -v brew >/dev/null 2>&1; then
        local brew_qt
        brew_qt="$(brew --prefix qt@6 2>/dev/null || true)"
        if [[ -n "$brew_qt" && -d "$brew_qt/lib/cmake/Qt6" ]]; then
            echo "$brew_qt"
            return 0
        fi
    fi

    for candidate in /opt/homebrew/opt/qt@6 /usr/local/opt/qt@6; do
        if [[ -d "$candidate/lib/cmake/Qt6" ]]; then
            echo "$candidate"
            return 0
        fi
    done

    return 1
}

QT_PREFIX="$(resolve_qt_prefix || true)"
if [[ -z "$QT_PREFIX" ]]; then
    echo "Qt 6 not found. Install with Homebrew:" >&2
    echo "  brew install qt@6 cmake git" >&2
    echo "Or set QT_PREFIX to your Qt installation." >&2
    exit 1
fi

MACDEPLOYQT="$QT_PREFIX/bin/macdeployqt"
if [[ ! -x "$MACDEPLOYQT" ]]; then
    echo "macdeployqt not found: $MACDEPLOYQT" >&2
    exit 1
fi

echo "Using Qt from: $QT_PREFIX"
echo "Building $APP_NAME (version $VERSION from VERSION)..."

cmake -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$QT_PREFIX" \
    -DGIT_VIEW_BUILD_TESTS=OFF

cmake --build "$BUILD_DIR" --config Release --target git_view

if [[ ! -d "$APP_PATH" ]]; then
    echo "App bundle not found: $APP_PATH" >&2
    exit 1
fi

chmod +x "$SCRIPT_DIR/verify-macos-app-version.sh"
"$SCRIPT_DIR/verify-macos-app-version.sh" "$APP_PATH"

echo "Bundling Qt frameworks with macdeployqt..."
MACDEPLOYQT_ARGS=(-always-overwrite -codesign=-)
if command -v brew >/dev/null 2>&1; then
    MACDEPLOYQT_ARGS+=(-libpath="$(brew --prefix)/lib")
fi
if ! "$MACDEPLOYQT" "$APP_PATH" "${MACDEPLOYQT_ARGS[@]}"; then
    echo "Warning: macdeployqt reported errors; continuing if the app bundle exists." >&2
fi

"$SCRIPT_DIR/verify-macos-app-version.sh" "$APP_PATH"

mkdir -p "$OUTPUT_DIR"
STAGING="$BUILD_DIR/dmg-staging"
rm -rf "$STAGING"
mkdir -p "$STAGING"
cp -R "$APP_PATH" "$STAGING/"
ln -sf /Applications "$STAGING/Applications"

echo "Creating disk image $DMG_NAME..."
rm -f "$DMG_PATH"
hdiutil create \
    -volname "git_view $VERSION" \
    -srcfolder "$STAGING" \
    -ov \
    -format UDZO \
    "$DMG_PATH" >/dev/null

rm -rf "$STAGING"

echo
echo "Done."
echo "  Version:    $VERSION"
echo "  App bundle: $APP_PATH"
echo "  Disk image: $DMG_PATH"
echo
echo "Install:"
echo "  open \"$DMG_PATH\""
echo "  drag git_view.app to Applications"
echo
echo "Requires git in PATH (Xcode Command Line Tools or Homebrew)."
