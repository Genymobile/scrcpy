#!/usr/bin/env bash
sudo apt install -y meson libavdevice-dev libusb-1.0-0-dev
set -e

BUILDDIR=build-auto
PREBUILT_SERVER_URL=https://github.com/Genymobile/scrcpy/releases/download/v2.4/scrcpy-server-v2.4
PREBUILT_SERVER_SHA256=93c272b7438605c055e127f7444064ed78fa9ca49f81156777fd201e79ce7ba3

echo "[scrcpy] Downloading prebuilt server..."
wget -e use_proxy=yes -e http_proxy=192.168.1.20:8889 -e https_proxy=192.168.1.20:8889  "$PREBUILT_SERVER_URL" -O scrcpy-server
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
