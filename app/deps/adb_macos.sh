#!/usr/bin/env bash
set -ex
. $(dirname ${BASH_SOURCE[0]})/_init "$@"

VERSION=36.0.0
URL="https://dl.google.com/android/repository/platform-tools_r$VERSION-darwin.zip"
SHA256SUM=d3e9fa1df3345cf728586908426615a60863d2632f73f1ce14f0f1349ef000fd

PROJECT_DIR="platform-tools-$VERSION-darwin"
FILENAME="$PROJECT_DIR.zip"

cd "$SOURCES_DIR"

if [[ -d "$PROJECT_DIR" ]]
then
    echo "$PWD/$PROJECT_DIR" found
else
    get_file "$URL" "$FILENAME" "$SHA256SUM"
    mkdir -p "$PROJECT_DIR"
    cd "$PROJECT_DIR"
    ZIP_PREFIX=platform-tools
    unzip "../$FILENAME" "$ZIP_PREFIX"/adb
    mv "$ZIP_PREFIX"/* .
    rmdir "$ZIP_PREFIX"
fi

mkdir -p "$INSTALL_DIR/adb-macos"
cd "$INSTALL_DIR/adb-macos"
cp -r "$SOURCES_DIR/$PROJECT_DIR"/. "$INSTALL_DIR/adb-macos/"
