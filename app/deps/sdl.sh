#!/usr/bin/env bash
set -ex
DEPS_DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DEPS_DIR"
. common

VERSION=2.30.7
FILENAME=SDL-$VERSION.tar.gz
PROJECT_DIR=SDL-release-$VERSION
SHA256SUM=1578c96f62c9ae36b64e431b2aa0e0b0fd07c275dedbc694afc38e19056688f5

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
