#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
OUTPUT_DIR="${1:-$SCRIPT_DIR/dist}"
APP_DIR="$OUTPUT_DIR/Scrcpy Desk.app"
SERVER_APK="$ROOT_DIR/server/build/outputs/apk/release/server-release-unsigned.apk"

if [[ ! -f "$SERVER_APK" ]]; then
    export ANDROID_HOME="${ANDROID_HOME:-$HOME/Library/Android/sdk}"
    if [[ -z "${JAVA_HOME:-}" ]] || ! "$JAVA_HOME/bin/java" -version 2>&1 | grep -Eq 'version "(17|18|19|2[0-9])'; then
        export JAVA_HOME="$(/usr/libexec/java_home -v 17+)"
    fi
    "$ROOT_DIR/gradlew" -p "$ROOT_DIR/server" assembleRelease
fi

if [[ -z "${SDKROOT:-}" ]] && [[ -d /Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk ]]; then
    export SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk
fi
export CLANG_MODULE_CACHE_PATH="${CLANG_MODULE_CACHE_PATH:-${TMPDIR:-/tmp}/scrcpy-desk-clang-cache}"
export SWIFTPM_MODULECACHE_OVERRIDE="${SWIFTPM_MODULECACHE_OVERRIDE:-${TMPDIR:-/tmp}/scrcpy-desk-swiftpm-cache}"
mkdir -p "$CLANG_MODULE_CACHE_PATH" "$SWIFTPM_MODULECACHE_OVERRIDE"

SWIFT_ARGS=(-c release --disable-sandbox --package-path "$ROOT_DIR")
if [[ -n "${SDKROOT:-}" ]]; then
    SWIFT_ARGS+=(--sdk "$SDKROOT")
fi

swift build "${SWIFT_ARGS[@]}"
BIN_DIR="$(swift build "${SWIFT_ARGS[@]}" --show-bin-path)"

# Always assemble a clean bundle so stale resources from a previous version
# (especially cached icon filenames) cannot remain in Contents/Resources.
rm -rf "$APP_DIR"
mkdir -p "$APP_DIR/Contents/MacOS" "$APP_DIR/Contents/Resources"
cp "$BIN_DIR/ScrcpyDesk" "$APP_DIR/Contents/MacOS/ScrcpyDesk"
cp "$SCRIPT_DIR/Support/Info.plist" "$APP_DIR/Contents/Info.plist"
cp "$SCRIPT_DIR/Assets/AppIcon.icns" "$APP_DIR/Contents/Resources/ScrcpyDeskAppIcon.icns"
cp "$SCRIPT_DIR/Assets/AppIcon.png" "$APP_DIR/Contents/Resources/ScrcpyDeskAppIcon.png"
cp "$SERVER_APK" "$APP_DIR/Contents/Resources/scrcpy-server"
cp "$ROOT_DIR/app/data/scrcpy.png" "$APP_DIR/Contents/Resources/scrcpy.png"
cp "$ROOT_DIR/app/data/disconnected.png" "$APP_DIR/Contents/Resources/disconnected.png"

ADB_BIN="$(command -v adb || true)"
if [[ -z "$ADB_BIN" ]] && [[ -x "${ANDROID_HOME:-$HOME/Library/Android/sdk}/platform-tools/adb" ]]; then
    ADB_BIN="${ANDROID_HOME:-$HOME/Library/Android/sdk}/platform-tools/adb"
fi
if [[ -n "$ADB_BIN" ]]; then
    cp "$ADB_BIN" "$APP_DIR/Contents/Resources/adb"
fi

codesign --force --deep --sign - "$APP_DIR"
echo "Built: $APP_DIR"
