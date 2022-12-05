#!/usr/bin/env bash
set -e
DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DIR"
. common
mkdir -p "$PREBUILT_DATA_DIR"
cd "$PREBUILT_DATA_DIR"

DEP_DIR=SDL2-2.0.22

FILENAME=SDL-windows-aarch64-mingw.7z
SHA256SUM=96a1bd6a0246419ab1e1ae510ba79dfbbfc94c2681f039426a632ffbd4d4b603

if [[ -d "$DEP_DIR" ]]
then
    echo "$DEP_DIR" found
    exit 0
fi

get_file "https://github.com/ZIXT233/SDL-windows-aarch64-mingw/releases/download/release-2.0.22/$FILENAME" "$FILENAME" "$SHA256SUM"

mkdir "$DEP_DIR"
cd "$DEP_DIR"

7z x "../$FILENAME" \
    SDL-windows-aarch64-mingw/bin/SDL2.dll \
    SDL-windows-aarch64-mingw/include/ \
    SDL-windows-aarch64-mingw/lib/

mv SDL-windows-aarch64-mingw aarch64-w64-mingw32
