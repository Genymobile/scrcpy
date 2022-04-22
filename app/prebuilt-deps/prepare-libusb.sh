#!/usr/bin/env bash
set -e
DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DIR"
. common
mkdir -p "$PREBUILT_DATA_DIR"
cd "$PREBUILT_DATA_DIR"

DEP_DIR=libusb-1.0.26

FILENAME=libusb-1.0.26-binaries.7z
SHA256SUM=9c242696342dbde9cdc47239391f71833939bf9f7aa2bbb28cdaabe890465ec5

if [[ -d "$DEP_DIR" ]]
then
    echo "$DEP_DIR" found
    exit 0
fi

get_file "https://github.com/libusb/libusb/releases/download/v1.0.26/$FILENAME" "$FILENAME" "$SHA256SUM"

mkdir "$DEP_DIR"
cd "$DEP_DIR"

# include/ is the same in all folders of the archive
7z x "../$FILENAME" \
    libusb-1.0.26-binaries/libusb-MinGW-Win32/bin/msys-usb-1.0.dll \
    libusb-1.0.26-binaries/libusb-MinGW-x64/bin/msys-usb-1.0.dll \
    libusb-1.0.26-binaries/libusb-MinGW-x64/include/

mv libusb-1.0.26-binaries/libusb-MinGW-Win32/bin MinGW-Win32
mv libusb-1.0.26-binaries/libusb-MinGW-x64/bin MinGW-x64
mv libusb-1.0.26-binaries/libusb-MinGW-x64/include .
rm -rf libusb-1.0.26-binaries
