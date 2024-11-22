#!/bin/bash
set -ex
cd "$(dirname ${BASH_SOURCE[0]})"
. build_common

cd "$OUTPUT_DIR"
sha256sum "scrcpy-server-$VERSION" \
    "scrcpy-linux-$VERSION.zip" \
    "scrcpy-win32-$VERSION.zip" \
    "scrcpy-win64-$VERSION.zip" \
    "scrcpy-macos-$VERSION.zip" \
        | tee SHA256SUMS.txt
echo "Release checksums generated in $PWD/SHA256SUMS.txt"
