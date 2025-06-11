#!/usr/bin/env bash
set -ex
DEPS_DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DEPS_DIR"
. common

VERSION=36.0.0
FILENAME=platform-tools_r$VERSION-win.zip
PROJECT_DIR=platform-tools-$VERSION-windows
SHA256SUM=24bd8bebbbb58b9870db202b5c6775c4a49992632021c60750d9d8ec8179d5f0

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
