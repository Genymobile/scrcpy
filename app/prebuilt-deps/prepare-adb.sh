#!/usr/bin/env bash
set -e
DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DIR"
. common
mkdir -p "$PREBUILT_DATA_DIR"
cd "$PREBUILT_DATA_DIR"

DEP_DIR=platform-tools-34.0.3

FILENAME=platform-tools_r34.0.3-windows.zip
SHA256SUM=fce992e93eb786fc9f47df93d83a7b912c46742d45c39d712c02e06d05b72e2b

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
