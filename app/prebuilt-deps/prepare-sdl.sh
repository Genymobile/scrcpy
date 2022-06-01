#!/usr/bin/env bash
set -e
DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DIR"
. common
mkdir -p "$PREBUILT_DATA_DIR"
cd "$PREBUILT_DATA_DIR"

DEP_DIR=SDL2-2.0.22

FILENAME=SDL2-devel-2.0.22-mingw.tar.gz
SHA256SUM=0e91e35973366aa1e6f81ee368924d9b4f93f9da4d2f2a89ec80b06eadcf23d1

if [[ -d "$DEP_DIR" ]]
then
    echo "$DEP_DIR" found
    exit 0
fi

get_file "https://libsdl.org/release/$FILENAME" "$FILENAME" "$SHA256SUM"

mkdir "$DEP_DIR"
cd "$DEP_DIR"

TAR_PREFIX="$DEP_DIR" # root directory inside the tar has the same name
tar xf "../$FILENAME" --strip-components=1 \
    "$TAR_PREFIX"/i686-w64-mingw32/bin/SDL2.dll \
    "$TAR_PREFIX"/i686-w64-mingw32/include/ \
    "$TAR_PREFIX"/i686-w64-mingw32/lib/ \
    "$TAR_PREFIX"/x86_64-w64-mingw32/bin/SDL2.dll \
    "$TAR_PREFIX"/x86_64-w64-mingw32/include/ \
    "$TAR_PREFIX"/x86_64-w64-mingw32/lib/ \
