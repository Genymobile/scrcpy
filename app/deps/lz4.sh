#!/usr/bin/env bash
set -ex
. $(dirname ${BASH_SOURCE[0]})/_init
process_args "$@"

VERSION=1.10.0
URL="https://github.com/lz4/lz4/releases/download/v$VERSION/lz4-$VERSION.tar.gz"
SHA256SUM=537512904744b35e232912055ccf8ec66d768639ff3abe5788d90d792ec5f48b

PROJECT_DIR="lz4-$VERSION"
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
        -DLZ4_BUILD_CLI=OFF
        # Always build lz4 statically
        -DBUILD_SHARED_LIBS=OFF
    )

    cmake "$SOURCES_DIR/$PROJECT_DIR/build/cmake" "${conf[@]}"
fi

cmake --build . -j"$NPROC"
cmake --install .
