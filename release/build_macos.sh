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

case "$ARCH" in
    arm64)
        EXPECTED_HOST_ARCH=arm64
        DEFAULT_DEPLOYMENT_TARGET=11.0
        ;;
    x86_64)
        EXPECTED_HOST_ARCH=x86_64
        DEFAULT_DEPLOYMENT_TARGET=10.13
        ;;
    *)
        echo "Unsupported macOS architecture: $ARCH (expected arm64 or x86_64)" >&2
        exit 1
        ;;
esac

HOST_ARCH="$(uname -m)"
if [[ "$HOST_ARCH" != "$EXPECTED_HOST_ARCH" ]]
then
    echo "Cannot label a $HOST_ARCH build as $ARCH" >&2
    exit 1
fi

: "${MACOSX_DEPLOYMENT_TARGET:=$DEFAULT_DEPLOYMENT_TARGET}"
export MACOSX_DEPLOYMENT_TARGET

MACOS_BUILD_DIR="$WORK_DIR/build-macos-$ARCH"

app/deps/sdl.sh macos native static
app/deps/dav1d.sh macos native static
app/deps/ffmpeg.sh macos native static
app/deps/libusb.sh macos native static

DEPS_INSTALL_DIR="$PWD/app/deps/work/install/macos-native-static"

# Never fall back to system libs
unset PKG_CONFIG_PATH
export PKG_CONFIG_LIBDIR="$DEPS_INSTALL_DIR/lib/pkgconfig"

rm -rf "$MACOS_BUILD_DIR"
meson setup "$MACOS_BUILD_DIR" \
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
cp "$MACOS_BUILD_DIR"/app/scrcpy-auto "$MACOS_BUILD_DIR/dist/"
cp app/data/scrcpy-auto.png "$MACOS_BUILD_DIR/dist/"
cp app/data/scrcpy-auto-disconnected.png "$MACOS_BUILD_DIR/dist/"
cp app/scrcpy-auto.1 "$MACOS_BUILD_DIR/dist/"
cp LICENSE "$MACOS_BUILD_DIR/dist"

# Apple Silicon requires a valid code signature. An ad-hoc signature is enough
# for a command-line binary distributed as a portable archive.
codesign --force --sign - "$MACOS_BUILD_DIR/dist/scrcpy-auto"
codesign --verify --strict "$MACOS_BUILD_DIR/dist/scrcpy-auto"

BUILT_ARCHS="$(lipo -archs "$MACOS_BUILD_DIR/dist/scrcpy-auto")"
if [[ "$BUILT_ARCHS" != "$ARCH" ]]
then
    echo "Unexpected binary architecture: $BUILT_ARCHS (expected $ARCH)" >&2
    exit 1
fi
