#!/bin/bash
set -ex

case "$1" in
    32)
        WINXX=win32
        BUILD_TYPE=cross
        ;;
    64)
        WINXX=win64
        BUILD_TYPE=cross
        ;;
    arm64)
        WINXX=winarm64
        BUILD_TYPE=native
        ;;
    *)
        echo "ERROR: $0 must be called with one argument: 32, 64 or arm64" >&2
        exit 1
        ;;
esac

cd "$(dirname ${BASH_SOURCE[0]})"
. build_common
cd .. # root project dir

WINXX_BUILD_DIR="$WORK_DIR/build-$WINXX"

# Prefer Ninja for CMake-based deps on MSYS2 (avoids "MSYS Makefiles" quirks).
# MSYS2 CLANGARM64 provides clang, not gcc — make autotools/cmake/configure
# pick it up by default.
if [[ "$BUILD_TYPE" == native ]]
then
    export CMAKE_GENERATOR=Ninja
    export CC=clang
    export CXX=clang++
fi

app/deps/adb_windows.sh
app/deps/sdl.sh $WINXX $BUILD_TYPE shared
app/deps/dav1d.sh $WINXX $BUILD_TYPE shared
app/deps/ffmpeg.sh $WINXX $BUILD_TYPE shared
app/deps/libusb.sh $WINXX $BUILD_TYPE shared

DEPS_INSTALL_DIR="$PWD/app/deps/work/install/$WINXX-$BUILD_TYPE-shared"
ADB_INSTALL_DIR="$PWD/app/deps/work/install/adb-windows"

# Never fall back to system libs
unset PKG_CONFIG_PATH
export PKG_CONFIG_LIBDIR="$DEPS_INSTALL_DIR/lib/pkgconfig"

meson_args=(
    -Dc_args="-I$DEPS_INSTALL_DIR/include"
    -Dc_link_args="-L$DEPS_INSTALL_DIR/lib"
    --buildtype=release
    --strip
    -Db_lto=true
    -Dcompile_server=false
    -Dportable=true
)

if [[ "$BUILD_TYPE" == cross ]]
then
    meson_args+=(--cross-file=cross_$WINXX.txt)
fi

rm -rf "$WINXX_BUILD_DIR"
meson setup "$WINXX_BUILD_DIR" "${meson_args[@]}"
ninja -C "$WINXX_BUILD_DIR"

# Group intermediate outputs into a 'dist' directory
mkdir -p "$WINXX_BUILD_DIR/dist"
cp "$WINXX_BUILD_DIR"/app/scrcpy.exe "$WINXX_BUILD_DIR/dist/"
cp app/data/scrcpy-noconsole.vbs "$WINXX_BUILD_DIR/dist/"
cp app/data/scrcpy.png "$WINXX_BUILD_DIR/dist/"
cp app/data/disconnected.png "$WINXX_BUILD_DIR/dist/"
cp app/data/open_a_terminal_here.bat "$WINXX_BUILD_DIR/dist/"
cp "$DEPS_INSTALL_DIR"/bin/*.dll "$WINXX_BUILD_DIR/dist/"
cp -r "$ADB_INSTALL_DIR"/. "$WINXX_BUILD_DIR/dist/"
