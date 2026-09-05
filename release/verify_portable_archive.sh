#!/bin/bash
set -e

cd "$(dirname "${BASH_SOURCE[0]}")"
. build_common

if [[ $# != 1 ]]
then
    echo "Syntax: $0 <target>" >&2
    exit 1
fi

TARGET="$1"
ARCHIVE="$OUTPUT_DIR/scrcpy-auto-$TARGET-$VERSION.tar.gz"
ROOT="scrcpy-auto-$TARGET-$VERSION"

if [[ ! -f "$ARCHIVE" ]]
then
    echo "Missing archive: $ARCHIVE" >&2
    exit 1
fi

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT
tar xzf "$ARCHIVE" -C "$TMP_DIR"

PACKAGE_DIR="$TMP_DIR/$ROOT"
test -x "$PACKAGE_DIR/scrcpy-auto"
test -s "$PACKAGE_DIR/scrcpy-auto-server"

# adb must always come from the user's PATH (or the ADB environment variable).
# Shipping another client could restart a shared adb server with a mismatched
# protocol version.
if [[ -e "$PACKAGE_DIR/adb" ]]
then
    echo "Portable archive must not bundle adb" >&2
    exit 1
fi

# The device-side server is version-coupled to the client and stays beside it.
test -f "$PACKAGE_DIR/scrcpy-auto.1"
test -f "$PACKAGE_DIR/LICENSE"

case "$TARGET" in
    linux-*)
        "$PACKAGE_DIR/scrcpy-auto" --version
        ;;
esac

echo "Verified portable archive: $ARCHIVE"
