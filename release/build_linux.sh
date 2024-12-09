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
LINUX_BUILD_DIR="$WORK_DIR/build-linux-$ARCH"

app/deps/adb_linux.sh
app/deps/sdl.sh linux native static
app/deps/dav1d.sh linux native static
app/deps/ffmpeg.sh linux native static
app/deps/libusb.sh linux native static

DEPS_INSTALL_DIR="$PWD/app/deps/work/install/linux-native-static"
ADB_INSTALL_DIR="$PWD/app/deps/work/install/adb-linux"

rm -rf "$LINUX_BUILD_DIR"
meson setup "$LINUX_BUILD_DIR" \
    --pkg-config-path="$DEPS_INSTALL_DIR/lib/pkgconfig" \
    -Dc_args="-I$DEPS_INSTALL_DIR/include" \
    -Dc_link_args="-L$DEPS_INSTALL_DIR/lib" \
    --buildtype=release \
    --strip \
    -Db_lto=true \
    -Dcompile_server=false \
    -Dportable=true \
    -Dstatic=true
ninja -C "$LINUX_BUILD_DIR"

# Group intermediate outputs into a 'dist' directory
mkdir -p "$LINUX_BUILD_DIR/dist"
cp "$LINUX_BUILD_DIR"/app/scrcpy "$LINUX_BUILD_DIR/dist/"
cp app/data/icon.png "$LINUX_BUILD_DIR/dist/"
cp app/scrcpy.1 "$LINUX_BUILD_DIR/dist/"
cp -r "$ADB_INSTALL_DIR"/. "$LINUX_BUILD_DIR/dist/"
