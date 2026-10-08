#!/usr/bin/env bash
set -ex
. $(dirname ${BASH_SOURCE[0]})/_init
process_args "$@"

# Google only provides x86_64 Linux platform-tools, so on other architectures
# adb is built from source, using the standalone build system of android-tools
# <https://github.com/nmeum/android-tools>.

VERSION=37.0.0
URL="https://github.com/nmeum/android-tools/releases/download/$VERSION/android-tools-$VERSION.tar.xz"
SHA256SUM=2725d09f892a3a38e534429f47a321f58ecf6a3169caa42c915fb2cb7d46be0e

PROJECT_DIR="android-tools-$VERSION"
FILENAME="$PROJECT_DIR.tar.xz"

# Corrosion is necessary to build the Rust mDNS backend of adb
CORROSION_VERSION=0.6.1
CORROSION_URL="https://github.com/corrosion-rs/corrosion/archive/refs/tags/v$CORROSION_VERSION.tar.gz"
CORROSION_SHA256SUM=e9e95b1ee2bad52681f347993fb1a5af5cce458c5ce8a2636c9e476e4babf8e3
CORROSION_FILENAME="corrosion-$CORROSION_VERSION.tar.gz"

cd "$SOURCES_DIR"

if [[ -d "$PROJECT_DIR" ]]
then
    echo "$PWD/$PROJECT_DIR" found
else
    get_file "$URL" "$FILENAME" "$SHA256SUM"
    tar xf "$FILENAME"  # First level directory is "$PROJECT_DIR"
    # Fix the build with protobuf >= 36
    # <https://github.com/nmeum/android-tools/commit/69a8c54dcd8c49d5b5eb22fa5c3c992d80e0bb2b>
    patch -d "$PROJECT_DIR/vendor/adb" -p1 < "$PATCHES_DIR"/android-tools-adb-keep-libbase-logging-macros-after-protobuf-includes.patch
    # Add mDNS support (backport, not released yet)
    # <https://github.com/nmeum/android-tools/commit/0b7aaaf81f349fd657a907b228703328aa0c29d5>
    patch -d "$PROJECT_DIR" -p1 < "$PATCHES_DIR"/android-tools-add-option-to-enable-new-rust-based-mdns-backend.patch
    # Pin the versions of the Rust dependencies
    cp "$PATCHES_DIR"/android-tools-adbmdns-Cargo.lock \
        "$PROJECT_DIR"/vendor/adb/client/adbmdns/Cargo.lock
    get_file "$CORROSION_URL" "$CORROSION_FILENAME" "$CORROSION_SHA256SUM"
    mkdir "$PROJECT_DIR"/vendor/corrosion
    tar xf "$CORROSION_FILENAME" -C "$PROJECT_DIR"/vendor/corrosion \
        --strip-components=1
fi

mkdir -p "$BUILD_DIR/$PROJECT_DIR"
cd "$BUILD_DIR/$PROJECT_DIR"

if [[ -d "$DIRNAME" ]]
then
    echo "'$PWD/$DIRNAME' already exists, not reconfigured"
    cd "$DIRNAME"
else
    mkdir "$DIRNAME"
    cd "$DIRNAME"

    # Only use the dependencies built by the other scripts, never the system
    # libraries, so that adb is as portable as the Google binary
    export PKG_CONFIG_LIBDIR="$INSTALL_DIR/$DIRNAME/lib/pkgconfig"

    conf=(
        -DCMAKE_PREFIX_PATH="$INSTALL_DIR/$DIRNAME"
        # The dependencies are static libraries
        -DPKG_CONFIG_ARGN=--static
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc -s"
        # Like the Google binary, use the libusb bundled with adb (without udev)
        -DANDROID_TOOLS_USE_BUNDLED_LIBUSB=ON
        -DANDROID_TOOLS_USE_BUNDLED_FMT=ON
        -DANDROID_TOOLS_USE_BUNDLED_CORROSION=ON
        -DANDROID_TOOLS_ADB_ENABLE_MDNS=ON
    )

    cmake "$SOURCES_DIR/$PROJECT_DIR" "${conf[@]}"
fi

cmake --build . -j"$NPROC" --target adb

mkdir -p "$INSTALL_DIR/adb-linux"
cp vendor/adb "$INSTALL_DIR/adb-linux/"
