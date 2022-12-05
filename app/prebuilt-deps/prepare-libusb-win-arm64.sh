#!/usr/bin/env bash
set -e
DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DIR"
. common
mkdir -p "$PREBUILT_DATA_DIR"
cd "$PREBUILT_DATA_DIR"

DEP_DIR=libusb-1.0.26

FILENAME=libusb-windows-aarch64-mingw.7z
SHA256SUM=4c83624edc89f95c3500fda3f982f038e7ce996979a8cca0e2e0133d3ef9ef64

if [[ -d "$DEP_DIR" ]]
then
    echo "$DEP_DIR" found
    exit 0
fi

get_file "https://github.com/ZIXT233/libusb-windows-aarch64-mingw/releases/download/v1.0.26/$FILENAME" "$FILENAME" "$SHA256SUM"

mkdir "$DEP_DIR"
cd "$DEP_DIR"

# include/ is the same in all folders of the archive
7z x "../$FILENAME" \
    libusb-windows-aarch64-mingw/bin/libusb-1.0.dll \
    libusb-windows-aarch64-mingw/include/

mv libusb-windows-aarch64-mingw/bin MinGW-aarch64
mv libusb-windows-aarch64-mingw/include .
rm -rf libusb-windows-aarch64-mingw
