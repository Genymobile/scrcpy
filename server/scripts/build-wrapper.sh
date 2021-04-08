#!/usr/bin/env bash
# Wrapper script to invoke gradle from meson
set -e

# Do not execute gradle when ninja is called as root (it would download the
# whole gradle world in /root/.gradle).
# This is typically useful for calling "sudo ninja install" after a "ninja
# install"
if [[ "$EUID" == 0 ]]
then
    echo "(not invoking gradle, since we are root)" >&2
    exit 0
fi

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
