#!/usr/bin/env bash
set -e

BUILDDIR=build-auto
PREBUILT_SERVER_URL=https://github.com/Genymobile/scrcpy/releases/download/v3.3.4/scrcpy-server-v3.3.4
PREBUILT_SERVER_SHA256=8588238c9a5a00aa542906b6ec7e6d5541d9ffb9b5d0f6e1bc0e365e2303079e

echo "[scrcpy] Downloading prebuilt server..."
wget "$PREBUILT_SERVER_URL" -O scrcpy-server
echo "[scrcpy] Verifying prebuilt server..."
echo "$PREBUILT_SERVER_SHA256  scrcpy-server" | sha256sum --check

echo "[scrcpy] Building client..."
rm -rf "$BUILDDIR"
meson setup "$BUILDDIR" --buildtype=release --strip -Db_lto=true \
    -Dprebuilt_server=scrcpy-server
cd "$BUILDDIR"
ninja

echo "[scrcpy] Installing (sudo)..."
sudo ninja install
