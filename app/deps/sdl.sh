#!/usr/bin/env bash
set -ex
. $(dirname ${BASH_SOURCE[0]})/_init
process_args "$@"

VERSION=2.32.8
URL="https://github.com/libsdl-org/SDL/archive/refs/tags/release-$VERSION.tar.gz"
SHA256SUM=dd35e05644ae527848d02433bec24dd0ea65db59faecf1a0e5d1880c533dac2c

PROJECT_DIR="sdl-$VERSION"
FILENAME="$PROJECT_DIR.tar.gz"

cd "$SOURCES_DIR"

if [[ -d "$PROJECT_DIR" ]]
then
    echo "$PWD/$PROJECT_DIR" found
else
    get_file "$URL" "$FILENAME" "$SHA256SUM"
    tar xf "$FILENAME"  # First level directory is "SDL-release-$VERSION"
    mv "SDL-release-$VERSION" "$PROJECT_DIR"
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

    if [[ "$HOST" == linux ]]
    then
        conf+=(
            --enable-video-wayland
            --enable-video-x11
        )
    fi

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

    "$SOURCES_DIR/$PROJECT_DIR"/configure "${conf[@]}"
fi

make -j
# There is no "make install-strip"
make install
# Strip manually
if [[ "$LINK_TYPE" == shared && "$HOST" == win* ]]
then
    ${HOST_TRIPLET}-strip "$INSTALL_DIR/$DIRNAME/bin/SDL2.dll"
fi
