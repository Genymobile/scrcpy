#!/usr/bin/env bash
set -ex
. $(dirname ${BASH_SOURCE[0]})/_init
process_args "$@"

VERSION=1.3.2
URL="https://github.com/madler/zlib/releases/download/v$VERSION/zlib-$VERSION.tar.xz"
SHA256SUM=d7a0654783a4da529d1bb793b7ad9c3318020af77667bcae35f95d0e42a792f3

PROJECT_DIR="zlib-$VERSION"
FILENAME="$PROJECT_DIR.tar.xz"

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

export CFLAGS='-O2 -fPIC'

if [[ -d "$DIRNAME" ]]
then
    echo "'$PWD/$DIRNAME' already exists, not reconfigured"
    cd "$DIRNAME"
else
    mkdir "$DIRNAME"
    cd "$DIRNAME"

    conf=(
        --prefix="$INSTALL_DIR/$DIRNAME"
        --libdir="$INSTALL_DIR/$DIRNAME/lib"
        # Always build zlib statically
        --static
    )

    if [[ "$BUILD_TYPE" == cross ]]
    then
        # The configure script uses CHOST as the toolchain prefix
        export CHOST="$HOST_TRIPLET"
    fi

    "$SOURCES_DIR/$PROJECT_DIR"/configure "${conf[@]}"
fi

make -j
make install
