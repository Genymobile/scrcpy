#!/bin/bash
set -ex
cd "$(dirname ${BASH_SOURCE[0]})"
. build_common

cd "$OUTPUT_DIR"
sha256sum "scrcpy-auto-server-$VERSION" \
    "scrcpy-auto-linux-x86_64-$VERSION.tar.gz" \
    "scrcpy-auto-win32-$VERSION.zip" \
    "scrcpy-auto-win64-$VERSION.zip" \
    "scrcpy-auto-macos-arm64-$VERSION.tar.gz" \
    "scrcpy-auto-macos-x86_64-$VERSION.tar.gz" \
        | tee SHA256SUMS.txt
echo "Release checksums generated in $PWD/SHA256SUMS.txt"
