#!/usr/bin/env bash
set -ex
. $(dirname ${BASH_SOURCE[0]})/_init
process_args "$@"

VERSION=1.5.7
URL="https://github.com/facebook/zstd/releases/download/v$VERSION/zstd-$VERSION.tar.gz"
SHA256SUM=eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3

PROJECT_DIR="zstd-$VERSION"
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

if [[ -d "$DIRNAME" ]]
then
    echo "'$PWD/$DIRNAME' already exists, not reconfigured"
    cd "$DIRNAME"
else
    mkdir "$DIRNAME"
    cd "$DIRNAME"

    conf=(
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR/$DIRNAME"
        -DCMAKE_INSTALL_LIBDIR=lib
        -DCMAKE_BUILD_TYPE=Release
        -DZSTD_BUILD_PROGRAMS=OFF
        # Always build zstd statically
        -DZSTD_BUILD_SHARED=OFF
    )

    cmake "$SOURCES_DIR/$PROJECT_DIR/build/cmake" "${conf[@]}"
fi

cmake --build . -j"$NPROC"
cmake --install .
