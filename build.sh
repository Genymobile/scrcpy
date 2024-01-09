#!/usr/bin/env sh
# Build the current state depending on a prebuld server.
# Rquired env paramter "BUILDDIR": target path of the build
set -e
set -u

# fetch the prebuilt server
PREBUILT_SERVER_URL=https://github.com/Genymobile/scrcpy/releases/download/v2.3.1/scrcpy-server-v2.3.1
PREBUILT_SERVER_SHA256=f6814822fc308a7a532f253485c9038183c6296a6c5df470a9e383b4f8e7605b

echo "[scrcpy] Downloading prebuilt server..."
wget "$PREBUILT_SERVER_URL" -O scrcpy-server
echo "[scrcpy] Verifying prebuilt server..."
echo "$PREBUILT_SERVER_SHA256  scrcpy-server" | sha256sum --check

echo "[scrcpy] Building client..."
rm -rf "$BUILDDIR"

# prepare the build
meson setup "$BUILDDIR" --buildtype=release --strip -Db_lto=true \
    -Dprebuilt_server=scrcpy-server

# build
ninja -C "$BUILDDIR"

# clean up
rm scrcpy-server
