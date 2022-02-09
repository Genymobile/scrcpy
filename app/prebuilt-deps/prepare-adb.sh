#!/usr/bin/env bash
set -e
DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DIR"
. common
mkdir -p "$PREBUILT_DATA_DIR"
cd "$PREBUILT_DATA_DIR"

DEP_DIR=platform-tools-31.0.3

FILENAME=platform-tools_r31.0.3-windows.zip
SHA256SUM=0f4b8fdd26af2c3733539d6eebb3c2ed499ea1d4bb1f4e0ecc2d6016961a6e24

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
