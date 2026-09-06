#!/usr/bin/env bash
set -ex
. $(dirname ${BASH_SOURCE[0]})/_init
process_args "$@"

VERSION=3.4.12
URL="https://github.com/libsdl-org/SDL/archive/refs/tags/release-$VERSION.tar.gz"
SHA256SUM=b68381f06a7580e63400b3b6eb547ec57d8c3ebde70f9f40e0aba530ba05da27

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
        -DCMAKE_BUILD_TYPE=Release
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
        case "$HOST" in
            win32)
                conf+=(
                    -DCMAKE_TOOLCHAIN_FILE="$SOURCES_DIR/$PROJECT_DIR/build-scripts/cmake-toolchain-mingw64-i686.cmake"
                )
                ;;

            win64)
                conf+=(
                    -DCMAKE_TOOLCHAIN_FILE="$SOURCES_DIR/$PROJECT_DIR/build-scripts/cmake-toolchain-mingw64-x86_64.cmake"
                )
                ;;

            winarm64)
                # SDL does not provide a toolchain file for aarch64 mingw
                conf+=(
                    -DCMAKE_SYSTEM_NAME=Windows
                    -DCMAKE_SYSTEM_PROCESSOR=aarch64
                    -DCMAKE_C_COMPILER="$HOST_TRIPLET-gcc"
                    -DCMAKE_CXX_COMPILER="$HOST_TRIPLET-g++"
                    -DCMAKE_RC_COMPILER="$HOST_TRIPLET-windres"
                )
                ;;

            *)
                echo "Unsupported cross-build to host: $HOST" >&2
                exit 1
        esac
    fi

    cmake "$SOURCES_DIR/$PROJECT_DIR" "${conf[@]}"
fi

cmake --build . -j
cmake --install .
