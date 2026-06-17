#!/usr/bin/env bash
# Read application version from the root VERSION file.
# Usage:
#   source packaging/read-version.sh   # sets ROOT, VERSION_FILE, VERSION
#   packaging/read-version.sh          # prints VERSION
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION_FILE="$ROOT/VERSION"

if [[ ! -f "$VERSION_FILE" ]]; then
    echo "Missing VERSION file: $VERSION_FILE" >&2
    exit 1
fi

VERSION="$(tr -d '[:space:]' < "$VERSION_FILE")"
if [[ -z "$VERSION" ]]; then
    echo "VERSION file is empty: $VERSION_FILE" >&2
    exit 1
fi

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    echo "$VERSION"
fi
