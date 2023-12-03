#!/usr/bin/env bash
set -e
DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DIR"
. common
mkdir -p "$PREBUILT_DATA_DIR"
cd "$PREBUILT_DATA_DIR"

VERSION=6.1-scrcpy-3
DEP_DIR="ffmpeg-$VERSION"

FILENAME="$DEP_DIR".7z
SHA256SUM=b646d18a3d543a4e4c46881568213499f22e4454a464e1552f03f2ac9cc3a05a

if [[ -d "$DEP_DIR" ]]
then
    echo "$DEP_DIR" found
    exit 0
fi

get_file "https://github.com/rom1v/scrcpy-deps/releases/download/$VERSION/$FILENAME" \
    "$FILENAME" "$SHA256SUM"

mkdir "$DEP_DIR"
cd "$DEP_DIR"

ZIP_PREFIX=ffmpeg
7z x "../$FILENAME"
mv "$ZIP_PREFIX"/* .
rmdir "$ZIP_PREFIX"
