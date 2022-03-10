#!/bin/bash

sudo apt update
sudo apt install ffmpeg libsdl2-2.0-0 adb libusb-1.0-0 -y
sudo apt install gcc git pkg-config meson ninja-build libsdl2-dev \
                 libavcodec-dev libavdevice-dev libavformat-dev libavutil-dev \
                 libusb-1.0-0-dev -y

sudo rm -rf scrcpy
git clone https://github.com/Genymobile/scrcpy
cd scrcpy

wget https://github.com/Genymobile/scrcpy/releases/download/v1.23/scrcpy-server-v1.23 \
-O scrcpy-server

meson x --buildtype=release --strip -Db_lto=true \
    -Dprebuilt_server=scrcpy-server
ninja -Cx

# icon name should be same as binary name for appimage creation
cp app/data/icon.png app/data/scrcpy.png

cd ../
