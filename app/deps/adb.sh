#!/usr/bin/env bash
set -ex
DEPS_DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DEPS_DIR"
. common

VERSION=34.0.5
FILENAME=platform-tools_r$VERSION-windows.zip
PROJECT_DIR=platform-tools-$VERSION
SHA256SUM=3f8320152704377de150418a3c4c9d07d16d80a6c0d0d8f7289c22c499e33571

cd "$SOURCES_DIR"

if [[ -d "$PROJECT_DIR" ]]
then
    echo "$PWD/$PROJECT_DIR" found
else
    get_file "https://dl.google.com/android/repository/$FILENAME" "$FILENAME" "$SHA256SUM"
    mkdir -p "$PROJECT_DIR"
    cd "$PROJECT_DIR"
    ZIP_PREFIX=platform-tools
    unzip "../$FILENAME" \
        "$ZIP_PREFIX"/AdbWinApi.dll \
        "$ZIP_PREFIX"/AdbWinUsbApi.dll \
        "$ZIP_PREFIX"/adb.exe
    mv "$ZIP_PREFIX"/* .
    rmdir "$ZIP_PREFIX"
fi

mkdir -p "$INSTALL_DIR/$HOST/bin"
cd "$INSTALL_DIR/$HOST/bin"
cp -r "$SOURCES_DIR/$PROJECT_DIR"/. "$INSTALL_DIR/$HOST/bin/"
