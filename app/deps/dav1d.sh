#!/usr/bin/env bash
set -ex
. $(dirname ${BASH_SOURCE[0]})/_init
process_args "$@"

VERSION=1.5.3
URL="https://code.videolan.org/videolan/dav1d/-/archive/$VERSION/dav1d-$VERSION.tar.gz"
SHA256SUM=cbe212b02faf8c6eed5b6d55ef8a6e363aaab83f15112e960701a9c3df813686

PROJECT_DIR="dav1d-$VERSION"
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
        --prefix="$INSTALL_DIR/$DIRNAME"
        --libdir=lib
        -Denable_tests=false
        -Denable_tools=false
        # Always build dav1d statically
        --default-library=static
    )

    if [[ "$BUILD_TYPE" == cross ]]
    then
        case "$HOST" in
            win32)
                conf+=(
                    --cross-file="$SOURCES_DIR/$PROJECT_DIR/package/crossfiles/i686-w64-mingw32.meson"
                )
                ;;

            win64)
                conf+=(
                    --cross-file="$SOURCES_DIR/$PROJECT_DIR/package/crossfiles/x86_64-w64-mingw32.meson"
                )
                ;;

            *)
                echo "Unsupported host: $HOST" >&2
                exit 1
        esac
    fi

    meson setup . "$SOURCES_DIR/$PROJECT_DIR" "${conf[@]}"
fi

ninja
ninja install
