#!/usr/bin/env bash
set -e
DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DIR"
. common
mkdir -p "$PREBUILT_DATA_DIR"
cd "$PREBUILT_DATA_DIR"

DEP_DIR=platform-tools-34.0.5

FILENAME=platform-tools_r34.0.5-windows.zip
SHA256SUM=3f8320152704377de150418a3c4c9d07d16d80a6c0d0d8f7289c22c499e33571

if [[ -d "$DEP_DIR" ]]
then
    echo "$DEP_DIR" found
    exit 0
fi

get_file "https://dl.google.com/android/repository/$FILENAME" \
    "$FILENAME" "$SHA256SUM"

mkdir "$DEP_DIR"
cd "$DEP_DIR"

ZIP_PREFIX=platform-tools
unzip "../$FILENAME" \
    "$ZIP_PREFIX"/AdbWinApi.dll \
    "$ZIP_PREFIX"/AdbWinUsbApi.dll \
    "$ZIP_PREFIX"/adb.exe
mv "$ZIP_PREFIX"/* .
rmdir "$ZIP_PREFIX"
