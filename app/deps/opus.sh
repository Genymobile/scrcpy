#!/usr/bin/env bash
set -ex
. $(dirname ${BASH_SOURCE[0]})/_init
process_args "$@"

# libopus provides the Opus encoder used by client audio (microphone)
# forwarding; FFmpeg's native Opus encoder is experimental and only accepts
# planar float samples.

VERSION=1.6.1
URL="https://downloads.xiph.org/releases/opus/opus-$VERSION.tar.gz"
SHA256SUM=6ffcb593207be92584df15b32466ed64bbec99109f007c82205f0194572411a1

PROJECT_DIR="opus-$VERSION"
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
        --disable-doc
        --disable-extra-programs
        # The stack protector would require libssp with mingw
        --disable-stack-protector
        # Always build libopus statically (it is linked into libavcodec)
        --enable-static
        --disable-shared
        --with-pic
    )

    if [[ "$BUILD_TYPE" == cross ]]
    then
        conf+=(
            --host="$HOST_TRIPLET"
        )
    fi

    "$SOURCES_DIR/$PROJECT_DIR"/configure "${conf[@]}"
fi

make -j"$NPROC"
make install
