#!/usr/bin/env bash
set -e
DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DIR"
. common
mkdir -p "$PREBUILT_DATA_DIR"
cd "$PREBUILT_DATA_DIR"

DEP_DIR=platform-tools-33.0.3

FILENAME=platform-tools_r33.0.3-windows.zip
SHA256SUM=1e59afd40a74c5c0eab0a9fad3f0faf8a674267106e0b19921be9f67081808c2

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
