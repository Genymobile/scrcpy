#!/usr/bin/env bash
set -e
DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DIR"
. common
mkdir -p "$PREBUILT_DATA_DIR"
cd "$PREBUILT_DATA_DIR"

DEP_DIR=ffmpeg-aarch64-4.3.1

FILENAME=ffmpeg-4.3.1-windows-aarch64.7z
SHA256SUM=8dd5ed9692f36c2f012a64903b8b2baec6581973d0d4dad872d1a476bcf5018b 

if [[ -d "$DEP_DIR" ]]
then
    echo "$DEP_DIR" found
    exit 0
fi

get_file "https://github.com/ZIXT233/FFmpeg-windows-aarch64-mingw/releases/download/n4.3.1/$FILENAME" \
    "$FILENAME" "$SHA256SUM"

mkdir "$DEP_DIR"
cd "$DEP_DIR"

ZIP_PREFIX=ffmpeg-4.3.1-windows-aarch64
7z x "../$FILENAME" \
    "$ZIP_PREFIX"/bin/avutil-56.dll \
    "$ZIP_PREFIX"/bin/avcodec-58.dll \
    "$ZIP_PREFIX"/bin/avformat-58.dll \
    "$ZIP_PREFIX"/bin/swresample-3.dll \
    "$ZIP_PREFIX"/bin/swscale-5.dll \
    "$ZIP_PREFIX"/include

mv "$ZIP_PREFIX"/* .
rmdir "$ZIP_PREFIX"
