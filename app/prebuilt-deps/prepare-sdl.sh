#!/usr/bin/env bash
set -e
DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DIR"
. common
mkdir -p "$PREBUILT_DATA_DIR"
cd "$PREBUILT_DATA_DIR"

VERSION=2.28.5
DEP_DIR="SDL2-$VERSION"

FILENAME="SDL2-devel-$VERSION-mingw.tar.gz"
SHA256SUM=3c0c655c2ebf67cad48fead72761d1601740ded30808952c3274ba223d226c21

if [[ -d "$DEP_DIR" ]]
then
    echo "$DEP_DIR" found
    exit 0
fi

get_file "https://github.com/libsdl-org/SDL/releases/download/release-$VERSION/$FILENAME" \
    "$FILENAME" "$SHA256SUM"

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
