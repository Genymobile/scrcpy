#!/bin/bash

sudo apt update
sudo apt install ffmpeg libsdl2-2.0-0 adb libusb-1.0-0 -y
sudo apt install gcc git pkg-config meson ninja-build libsdl2-dev \
                 libavcodec-dev libavdevice-dev libavformat-dev libavutil-dev \
                 libusb-1.0-0-dev -y

sudo rm -rf scrcpy
git clone https://github.com/Genymobile/scrcpy
cd scrcpy

latest_tag=$(git describe --tags --abbrev=0)
echo "LATEST_TAG=$latest_tag" >> $GITHUB_ENV # will be used to upload artifact with tag

wget https://github.com/Genymobile/scrcpy/releases/download/$latest_tag/scrcpy-server-$latest_tag \
-O scrcpy-server

meson x --buildtype=release --strip -Db_lto=true \
    -Dprebuilt_server=scrcpy-server
ninja -Cx

# icon name should be same as binary name for appimage creation
cp app/data/icon.png app/data/scrcpy.png

cd ../
