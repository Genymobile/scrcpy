#!/usr/bin/env bash
set -e
DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DIR"
. common
mkdir -p "$PREBUILT_DATA_DIR"
cd "$PREBUILT_DATA_DIR"

VERSION=5.1.2
DEP_DIR=ffmpeg-win64-$VERSION

FILENAME=ffmpeg-$VERSION-full_build-shared.7z
SHA256SUM=d9eb97b72d7cfdae4d0f7eaea59ccffb8c364d67d88018ea715d5e2e193f00e9

if [[ -d "$DEP_DIR" ]]
then
    echo "$DEP_DIR" found
    exit 0
fi

get_file "https://github.com/GyanD/codexffmpeg/releases/download/$VERSION/$FILENAME" \
    "$FILENAME" "$SHA256SUM"

mkdir "$DEP_DIR"
cd "$DEP_DIR"

ZIP_PREFIX=ffmpeg-$VERSION-full_build-shared
7z x "../$FILENAME" \
    "$ZIP_PREFIX"/bin/avutil-57.dll \
    "$ZIP_PREFIX"/bin/avcodec-59.dll \
    "$ZIP_PREFIX"/bin/avformat-59.dll \
    "$ZIP_PREFIX"/bin/swresample-4.dll \
    "$ZIP_PREFIX"/bin/swscale-6.dll \
    "$ZIP_PREFIX"/include
mv "$ZIP_PREFIX"/* .
rmdir "$ZIP_PREFIX"
