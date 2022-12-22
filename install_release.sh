#!/usr/bin/env bash
set -e

BUILDDIR=build-auto
PREBUILT_SERVER_URL=https://github.com/Genymobile/scrcpy/releases/download/v1.25/scrcpy-server-v1.25
PREBUILT_SERVER_SHA256=ce0306c7bbd06ae72f6d06f7ec0ee33774995a65de71e0a83813ecb67aec9bdb

echo "[scrcpy] Downloading prebuilt server..."
wget "$PREBUILT_SERVER_URL" -O scrcpy-server
echo "[scrcpy] Verifying prebuilt server..."
echo "$PREBUILT_SERVER_SHA256  scrcpy-server" | sha256sum --check

echo "[scrcpy] Building client..."
rm -rf "$BUILDDIR"
meson "$BUILDDIR" --buildtype=release --strip -Db_lto=true \
    -Dprebuilt_server=scrcpy-server
cd "$BUILDDIR"
ninja

echo "[scrcpy] Installing (sudo)..."
sudo ninja install
