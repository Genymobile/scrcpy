#!/usr/bin/env bash
set -e
DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DIR"
. common
mkdir -p "$PREBUILT_DATA_DIR"
cd "$PREBUILT_DATA_DIR"

DEP_DIR=platform-tools-33.0.1

FILENAME=platform-tools_r33.0.1-windows.zip
SHA256SUM=c1f02d42ea24ef4ff2a405ae7370e764ef4546f9b3e4520f5571a00ed5012c42

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
