#!/usr/bin/env sh
# This script executes three steps:
# - fetch the latest prebuild_server
# - invoke the build script
# - run the install process
set -e

export BUILDDIR=build-auto

./build.sh

echo "[scrcpy] Installing (sudo)..."
sudo ninja -C "$BUILDDIR" install
