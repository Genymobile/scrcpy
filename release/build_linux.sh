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

# Only the headers of the system libdrm and libva are used (the libraries are
# loaded at runtime), expose only their pkg-config files
SYSTEM_PC_DIR="$WORK_DIR/system-pkgconfig-linux-$ARCH"
mkdir -p "$SYSTEM_PC_DIR"
for pc in libdrm libva libva-drm
do
    pc_dir="$(pkg-config --variable=pcfiledir "$pc")"
    ln -sf "$pc_dir/$pc.pc" "$SYSTEM_PC_DIR/"
done

# Never fall back to system libs
unset PKG_CONFIG_PATH
export PKG_CONFIG_LIBDIR="$DEPS_INSTALL_DIR/lib/pkgconfig:$SYSTEM_PC_DIR"

rm -rf "$LINUX_BUILD_DIR"
meson setup "$LINUX_BUILD_DIR" \
    -Dc_args="-I$DEPS_INSTALL_DIR/include" \
    -Dc_link_args="-L$DEPS_INSTALL_DIR/lib" \
    --buildtype=release \
    --strip \
    -Db_lto=true \
    -Dcompile_server=false \
    -Dportable=true \
    -Dstatic=true
ninja -C "$LINUX_BUILD_DIR"

# libva is loaded at runtime, so that scrcpy also runs where it is not installed
if readelf -d "$LINUX_BUILD_DIR/app/scrcpy" | grep -qE 'NEEDED.*libva'
then
    echo "The scrcpy binary must not depend on libva," \
         "functions missing from app/src/vaapi_shim.c:" >&2
    nm -D --undefined-only "$LINUX_BUILD_DIR/app/scrcpy" \
        | awk '{print $2}' | grep -E '^(va|drm)[A-Z]' >&2
    exit 1
fi

# Group intermediate outputs into a 'dist' directory
mkdir -p "$LINUX_BUILD_DIR/dist"
cp "$LINUX_BUILD_DIR"/app/scrcpy "$LINUX_BUILD_DIR/dist/"
cp app/data/scrcpy.png "$LINUX_BUILD_DIR/dist/"
cp app/data/disconnected.png "$LINUX_BUILD_DIR/dist/"
cp app/scrcpy.1 "$LINUX_BUILD_DIR/dist/"
cp LICENSE "$LINUX_BUILD_DIR/dist"
cp -r "$ADB_INSTALL_DIR"/. "$LINUX_BUILD_DIR/dist/"
