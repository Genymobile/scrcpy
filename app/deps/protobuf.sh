#!/usr/bin/env bash
set -ex
. $(dirname ${BASH_SOURCE[0]})/_init
process_args "$@"

VERSION=36.2
URL="https://github.com/protocolbuffers/protobuf/releases/download/v$VERSION/protobuf-$VERSION.tar.gz"
SHA256SUM=3d9642a662d10e68ebae5e53f14dcce5105684212d5078f8e0d47d1ab3ae6b64

PROJECT_DIR="protobuf-$VERSION"
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
        # Use the abseil built by abseil.sh
        -DCMAKE_PREFIX_PATH="$INSTALL_DIR/$DIRNAME"
        -DCMAKE_BUILD_TYPE=Release
        -Dprotobuf_LOCAL_DEPENDENCIES_ONLY=ON
        -Dprotobuf_WITH_ZLIB=OFF
        # Always build protobuf statically
        -Dprotobuf_BUILD_SHARED_LIBS=OFF
    )

    cmake "$SOURCES_DIR/$PROJECT_DIR" "${conf[@]}"
fi

cmake --build . -j"$NPROC"
cmake --install .
