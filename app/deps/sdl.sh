#!/usr/bin/env bash
set -ex
DEPS_DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DEPS_DIR"
. common

VERSION=2.30.8
FILENAME=SDL-$VERSION.tar.gz
PROJECT_DIR=SDL-release-$VERSION
SHA256SUM=380C295EA76B9BD72D90075793971C8BCB232BA0A69A9B14DA4AE8F603350058

cd "$SOURCES_DIR"

if [[ -d "$PROJECT_DIR" ]]
then
    echo "$PWD/$PROJECT_DIR" found
else
    get_file "https://github.com/libsdl-org/SDL/archive/refs/tags/release-$VERSION.tar.gz" "$FILENAME" "$SHA256SUM"
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
# There is no "make install-strip"
make install
# Strip manually
${HOST_TRIPLET}-strip "$INSTALL_DIR/$HOST/bin/SDL2.dll"
