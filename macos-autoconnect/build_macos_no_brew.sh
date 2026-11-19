#!/bin/bash
set -e

PREFIX="$HOME/.local"
BUILD_DEPS="/tmp/scrcpy-deps"
mkdir -p "$PREFIX/bin" "$BUILD_DEPS"
export PATH="$PREFIX/bin:$PATH"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"

echo "1. Checking Xcode Command Line Tools..."
if ! xcode-select -p &> /dev/null; then
    xcode-select --install
    echo "Please complete the Xcode Command Line Tools popup and re-run this script."
    exit 1
fi

echo "2. Installing Meson & Ninja locally via Python..."
python3 -m pip install --user --upgrade meson ninja

echo "3. Downloading standalone Android Platform-Tools (adb)..."
cd "$BUILD_DEPS"
curl -LO https://google.com
unzip -qo platform-tools-latest-darwin.zip
cp -f platform-tools/adb "$PREFIX/bin/"
chmod +x "$PREFIX/bin/adb"

echo "4. Building pkg-config, libusb, SDL2, and FFmpeg locally (no Homebrew/sudo)..."
# pkg-config
curl -LO https://freedesktop.org
tar xf pkg-config-0.29.2.tar.gz && cd pkg-config-0.29.2
./configure --prefix="$PREFIX" --with-internal-glib CFLAGS="-Wno-int-conversion" && make -j$(sysctl -n hw.ncpu) && make install
cd "$BUILD_DEPS"

# libusb
curl -LO https://github.com
tar xf libusb-1.0.27.tar.bz2 && cd libusb-1.0.27
./configure --prefix="$PREFIX" && make -j$(sysctl -n hw.ncpu) && make install
cd "$BUILD_DEPS"

# SDL2
curl -LO https://github.com
tar xf SDL2-2.30.8.tar.gz && cd SDL2-2.30.8
./configure --prefix="$PREFIX" && make -j$(sysctl -n hw.ncpu) && make install
cd "$BUILD_DEPS"

# FFmpeg (minimal decoder-only build for fast compilation)
curl -LO https://ffmpeg.org
tar xf ffmpeg-7.1.tar.xz && cd ffmpeg-7.1
./configure --prefix="$PREFIX" --enable-shared --disable-static --disable-programs --disable-doc --disable-encoders --disable-muxers --disable-x86asm
make -j$(sysctl -n hw.ncpu) && make install

echo "5. Compiling custom scrcpy..."
cd - > /dev/null
cd "$(dirname "$0")/.."
rm -rf build
meson setup build --prefix="$PREFIX" --buildtype=release --strip -Db_lto=true
ninja -Cbuild
ninja -Cbuild install

echo "✅ Build complete! Binary installed to $PREFIX/bin/scrcpy"
