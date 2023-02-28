#!/usr/bin/env bash
set -e
DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DIR"
. common
mkdir -p "$PREBUILT_DATA_DIR"
cd "$PREBUILT_DATA_DIR"

VERSION=6.0-scrcpy
DEP_DIR="ffmpeg-$VERSION"

FILENAME="$DEP_DIR".7z
SHA256SUM=f3956295b4325a84aada05447ba3f314fbed96697811666d495de4de40d59f98

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
