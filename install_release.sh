#!/usr/bin/env bash
set -e

BUILDDIR=build-auto
PREBUILT_SERVER_URL=https://github.com/Genymobile/scrcpy/releases/download/v1.19/scrcpy-server-v1.19
PREBUILT_SERVER_SHA256=876f9322182e6aac6a58db1334f4225855ef3a17eaebc80aab6601d9d1ecb867

echo "[scrcpy] Downloading prebuilt server..."
wget "$PREBUILT_SERVER_URL" -O scrcpy-server
echo "[scrcpy] Verifying prebuilt server..."
echo "$PREBUILT_SERVER_SHA256  scrcpy-server" | sha256sum --check

echo "[scrcpy] Building client..."
rm -rf "$BUILDDIR"
meson "$BUILDDIR" --buildtype release --strip -Db_lto=true \
    -Dprebuilt_server=scrcpy-server
cd "$BUILDDIR"
ninja

echo "[scrcpy] Installing (sudo)..."
sudo ninja install
