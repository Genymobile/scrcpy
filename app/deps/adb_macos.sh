#!/usr/bin/env bash
set -ex
DEPS_DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DEPS_DIR"
. common

VERSION=36.0.0
FILENAME=platform-tools_r$VERSION-darwin.zip
PROJECT_DIR=platform-tools-$VERSION-darwin
SHA256SUM=b241878e6ec20650b041bf715ea05f7d5dc73bd24529464bd9cf68946e3132bd

cd "$SOURCES_DIR"

if [[ -d "$PROJECT_DIR" ]]
then
    echo "$PWD/$PROJECT_DIR" found
else
    get_file "https://dl.google.com/android/repository/$FILENAME" "$FILENAME" "$SHA256SUM"
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
