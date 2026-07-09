#!/usr/bin/env bash
set -ex
. $(dirname ${BASH_SOURCE[0]})/_init
process_args "$@"

VERSION=1.0.30
URL="https://github.com/libusb/libusb/archive/refs/tags/v$VERSION.tar.gz"
SHA256SUM=2ae28adb0bb9558c86135c4e1c11b320b0805461e207a64a6e520a114094bf07

PROJECT_DIR="libusb-$VERSION"
FILENAME="$PROJECT_DIR.tar.gz"

cd "$SOURCES_DIR"

if [[ -d "$PROJECT_DIR" ]]
then
    echo "$PWD/$PROJECT_DIR" found
else
    get_file "$URL" "$FILENAME" "$SHA256SUM"
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
