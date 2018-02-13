#!/bin/bash
# Wrapper script to invoke gradle from meson
set -e

PROJECT_ROOT="$1"
OUTPUT="$2"
BUILDTYPE="$3"

# gradlew is in the parent of the server directory
GRADLE=${GRADLE:-$PROJECT_ROOT/../gradlew}

if [[ "$BUILDTYPE" == debug ]]
then
    "$GRADLE" -p "$PROJECT_ROOT" assembleDebug
    cp "$PROJECT_ROOT/build/outputs/apk/debug/server-debug.apk" "$OUTPUT"
else
    "$GRADLE" -p "$PROJECT_ROOT" assembleRelease
    cp "$PROJECT_ROOT/build/outputs/apk/release/server-release-unsigned.apk" "$OUTPUT"
fi
