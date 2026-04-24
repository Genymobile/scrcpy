#!/usr/bin/env bash
set -ex
. $(dirname ${BASH_SOURCE[0]})/_init
process_args "$@"

VERSION=3.4.4
URL="https://github.com/libsdl-org/SDL/archive/refs/tags/release-$VERSION.tar.gz"
SHA256SUM=bbe4f47ffc784857dc51b63a029e1116be2b4557fc1c61213ecef91be22ec2b6

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
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR/$DIRNAME"
        -DSDL_TESTS=OFF
    )

    if [[ "$HOST" == linux ]]
    then
        conf+=(
            -DSDL_WAYLAND=ON
            -DSDL_X11=ON
        )
    fi

    if [[ "$LINK_TYPE" == static ]]
    then
        conf+=(
            -DBUILD_SHARED_LIBS=OFF
        )
    else
        conf+=(
            -DBUILD_SHARED_LIBS=ON
        )
    fi

    if [[ "$BUILD_TYPE" == cross ]]
    then
        if [[ "$HOST" = win32 ]]
        then
            TOOLCHAIN_FILENAME="cmake-toolchain-mingw64-i686.cmake"
        elif [[ "$HOST" = win64 ]]
        then
            TOOLCHAIN_FILENAME="cmake-toolchain-mingw64-x86_64.cmake"
        else
            echo "Unsupported cross-build to host: $HOST" >&2
            exit 1
        fi

        conf+=(
            -DCMAKE_TOOLCHAIN_FILE="$SOURCES_DIR/$PROJECT_DIR/build-scripts/$TOOLCHAIN_FILENAME"
        )
    fi

    cmake "$SOURCES_DIR/$PROJECT_DIR" "${conf[@]}"
fi

cmake --build .
cmake --install .
