#!/usr/bin/env bash
set -ex
DEPS_DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DEPS_DIR"
. common
process_args "$@"

VERSION=1.0.27
FILENAME=libusb-$VERSION.tar.gz
PROJECT_DIR=libusb-$VERSION
SHA256SUM=e8f18a7a36ecbb11fb820bd71540350d8f61bcd9db0d2e8c18a6fb80b214a3de

cd "$SOURCES_DIR"

if [[ -d "$PROJECT_DIR" ]]
then
    echo "$PWD/$PROJECT_DIR" found
else
    get_file "https://github.com/libusb/libusb/archive/refs/tags/v$VERSION.tar.gz" "$FILENAME" "$SHA256SUM"
    tar xf "$FILENAME"  # First level directory is "$PROJECT_DIR"
fi

mkdir -p "$BUILD_DIR/$PROJECT_DIR"
cd "$BUILD_DIR/$PROJECT_DIR"

export CFLAGS='-O2'
export CXXFLAGS="$CFLAGS"

if [[ -d "$DIRNAME" ]]
then
    echo "'$PWD/$DIRNAME' already exists, not reconfigured"
    cd "$DIRNAME"
else
    mkdir "$DIRNAME"
    cd "$DIRNAME"

    conf=(
        --prefix="$INSTALL_DIR/$DIRNAME"
    )

    if [[ "$LINK_TYPE" == static ]]
    then
        conf+=(
            --enable-static
            --disable-shared
        )
    else
        conf+=(
            --disable-static
            --enable-shared
        )
    fi

    if [[ "$BUILD_TYPE" == cross ]]
    then
        conf+=(
            --host="$HOST_TRIPLET"
        )
    fi

    "$SOURCES_DIR/$PROJECT_DIR"/bootstrap.sh
    "$SOURCES_DIR/$PROJECT_DIR"/configure "${conf[@]}"
fi

make -j
make install-strip
