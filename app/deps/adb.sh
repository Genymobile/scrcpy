#!/usr/bin/env bash
set -ex
DEPS_DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DEPS_DIR"
. common

VERSION=35.0.0
FILENAME=platform-tools_r$VERSION-windows.zip
PROJECT_DIR=platform-tools-$VERSION
SHA256SUM=7ab78a8f8b305ae4d0de647d99c43599744de61a0838d3a47bda0cdffefee87e

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
