#!/bin/bash
set -ex
cd "$(dirname ${BASH_SOURCE[0]})"
. build_common
cd .. # root project dir

if [[ $# != 1 ]]
then
    echo "Syntax: $0 <arch>" >&2
    exit 1
fi

ARCH="$1"
MACOS_BUILD_DIR="$WORK_DIR/build-macos-$ARCH"

app/deps/adb_macos.sh
app/deps/sdl.sh macos native static
app/deps/dav1d.sh macos native static
app/deps/ffmpeg.sh macos native static
app/deps/libusb.sh macos native static

DEPS_INSTALL_DIR="$PWD/app/deps/work/install/macos-native-static"
ADB_INSTALL_DIR="$PWD/app/deps/work/install/adb-macos"

rm -rf "$MACOS_BUILD_DIR"
meson setup "$MACOS_BUILD_DIR" \
    --pkg-config-path="$DEPS_INSTALL_DIR/lib/pkgconfig" \
    -Dc_args="-I$DEPS_INSTALL_DIR/include" \
    -Dc_link_args="-L$DEPS_INSTALL_DIR/lib" \
    --buildtype=release \
    --strip \
    -Db_lto=true \
    -Dcompile_server=false \
    -Dportable=true \
    -Dstatic=true
ninja -C "$MACOS_BUILD_DIR"

# Group intermediate outputs into a 'dist' directory
mkdir -p "$MACOS_BUILD_DIR/dist"
cp "$MACOS_BUILD_DIR"/app/scrcpy "$MACOS_BUILD_DIR/dist/"
cp app/data/icon.png "$MACOS_BUILD_DIR/dist/"
cp app/scrcpy.1 "$MACOS_BUILD_DIR/dist/"
cp -r "$ADB_INSTALL_DIR"/. "$MACOS_BUILD_DIR/dist/"
