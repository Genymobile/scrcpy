#!/usr/bin/env bash
set -ex
DEPS_DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DEPS_DIR"
. common

VERSION=36.0.0
FILENAME=platform-tools_r$VERSION-win.zip
PROJECT_DIR=platform-tools-$VERSION-windows
SHA256SUM=12c2841f354e92a0eb2fd7bf6f0f9bf8538abce7bd6b060ac8349d6f6a61107c

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

mkdir -p "$INSTALL_DIR/adb-windows"
cd "$INSTALL_DIR/adb-windows"
cp -r "$SOURCES_DIR/$PROJECT_DIR"/. "$INSTALL_DIR/adb-windows/"
