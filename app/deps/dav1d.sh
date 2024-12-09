#!/usr/bin/env bash
set -ex
DEPS_DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DEPS_DIR"
. common
process_args "$@"

VERSION=1.5.0
FILENAME=dav1d-$VERSION.tar.gz
PROJECT_DIR=dav1d-$VERSION
SHA256SUM=78b15d9954b513ea92d27f39362535ded2243e1b0924fde39f37a31ebed5f76b

cd "$SOURCES_DIR"

if [[ -d "$PROJECT_DIR" ]]
then
    echo "$PWD/$PROJECT_DIR" found
else
    get_file "https://code.videolan.org/videolan/dav1d/-/archive/$VERSION/$FILENAME" "$FILENAME" "$SHA256SUM"
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
