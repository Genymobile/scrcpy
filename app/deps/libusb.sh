#!/usr/bin/env bash
set -ex
DEPS_DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DEPS_DIR"
. common

VERSION=1.0.27
FILENAME=libusb-$VERSION.tar.bz2
PROJECT_DIR=libusb-$VERSION
SHA256SUM=ffaa41d741a8a3bee244ac8e54a72ea05bf2879663c098c82fc5757853441575

cd "$SOURCES_DIR"

if [[ -d "$PROJECT_DIR" ]]
then
    echo "$PWD/$PROJECT_DIR" found
else
    get_file "https://github.com/libusb/libusb/releases/download/v$VERSION/libusb-$VERSION.tar.bz2" "$FILENAME" "$SHA256SUM"
    tar xf "$FILENAME"  # First level directory is "$PROJECT_DIR"
fi

mkdir -p "$BUILD_DIR/$PROJECT_DIR"
cd "$BUILD_DIR/$PROJECT_DIR"

export CFLAGS='-O2'
export CXXFLAGS="$CFLAGS"

if [[ -d "$HOST" ]]
then
    echo "'$PWD/$HOST' already exists, not reconfigured"
    cd "$HOST"
else
    mkdir "$HOST"
    cd "$HOST"

    "$SOURCES_DIR/$PROJECT_DIR"/configure \
        --prefix="$INSTALL_DIR/$HOST" \
        --host="$HOST_TRIPLET" \
        --enable-shared \
        --disable-static
fi

make -j
make install-strip
