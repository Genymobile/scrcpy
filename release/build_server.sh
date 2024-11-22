#!/bin/bash
set -ex
cd "$(dirname ${BASH_SOURCE[0]})"
. build_common
cd .. # root project dir

GRADLE="${GRADLE:-./gradlew}"
SERVER_BUILD_DIR="$WORK_DIR/build-server"

rm -rf "$SERVER_BUILD_DIR"
"$GRADLE" -p server assembleRelease
mkdir -p "$SERVER_BUILD_DIR/server"
cp server/build/outputs/apk/release/server-release-unsigned.apk \
    "$SERVER_BUILD_DIR/server/scrcpy-server"
