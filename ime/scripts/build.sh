#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ROOT_DIR="$(cd "$PROJECT_DIR/.." && pwd)"
GRADLE="${GRADLE:-$ROOT_DIR/gradlew}"
VARIANT="${1:-debug}"

case "$VARIANT" in
    debug)
        TASK=assembleDebug
        APK="$PROJECT_DIR/build/outputs/apk/debug/scrcpy-ime-debug.apk"
        ;;
    release)
        TASK=assembleRelease
        APK="$PROJECT_DIR/build/outputs/apk/release/scrcpy-ime-release.apk"
        ;;
    *)
        echo "Usage: $0 [debug|release]" >&2
        exit 1
        ;;
esac

"$GRADLE" -p "$PROJECT_DIR" "$TASK"
mkdir -p "$PROJECT_DIR/dist"
cp "$APK" "$PROJECT_DIR/dist/scrcpy-ime.apk"
echo "Generated $PROJECT_DIR/dist/scrcpy-ime.apk"
