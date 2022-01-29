#!/usr/bin/env bash
set -e
DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DIR"
. common
mkdir -p "$PREBUILT_DATA_DIR"
cd "$PREBUILT_DATA_DIR"

DEP_DIR=ffmpeg-win64-5.0

FILENAME=ffmpeg-5.0-full_build-shared.7z
SHA256SUM=e5900f6cecd4c438d398bd2fc308736c10b857cd8dd61c11bcfb05bff5d1211a

if [[ -d "$DEP_DIR" ]]
then
    echo "$DEP_DIR" found
    exit 0
fi

get_file "https://github.com/GyanD/codexffmpeg/releases/download/5.0/$FILENAME" \
    "$FILENAME" "$SHA256SUM"

mkdir "$DEP_DIR"
cd "$DEP_DIR"

ZIP_PREFIX=ffmpeg-5.0-full_build-shared
7z x "../$FILENAME" \
    "$ZIP_PREFIX"/bin/avutil-57.dll \
    "$ZIP_PREFIX"/bin/avcodec-59.dll \
    "$ZIP_PREFIX"/bin/avformat-59.dll \
    "$ZIP_PREFIX"/bin/swresample-4.dll \
    "$ZIP_PREFIX"/bin/swscale-6.dll \
    "$ZIP_PREFIX"/include
mv "$ZIP_PREFIX"/* .
rmdir "$ZIP_PREFIX"
