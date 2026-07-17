#!/usr/bin/env bash
#
# build-in-container.sh — runs INSIDE a clean ubuntu:24.04 container and produces
# a self-contained scrcpy-auto .deb.
#
# It is normally invoked by ../deb/build-deb.sh (the host-side Docker launcher),
# but can be run directly inside any Ubuntu 24.04 environment with internet.
#
# Strategy: statically link the heavy media deps (FFmpeg + SDL3 + dav1d + libusb)
# into the client binary, then bundle the Android server and adb. The resulting
# package depends only on base-image libraries, so `dpkg -i` succeeds on an
# offline host.
#
# Inputs (env):
#   SRC          source tree mount (read-only)      default /src
#   OUT          output dir for the .deb            default /out
#   SERVER_MODE  prebuilt | none | path:/abs/file   default prebuilt
#   BUNDLE_ADB   1 to bundle adb, 0 to skip         default 1
#   DEB_VERSION  override the package version        default <meson version>-1
set -euo pipefail

SRC=${SRC:-/src}
OUT=${OUT:-/out}
BUILD=${BUILD:-/build}
SERVER_MODE=${SERVER_MODE:-prebuilt}
BUNDLE_ADB=${BUNDLE_ADB:-1}
JOBS=$(nproc)

# Pinned upstream v4.0 server (server/src is byte-for-byte upstream in this fork,
# so the official prebuilt is functionally identical; we only rename it).
PREBUILT_SERVER_URL=https://github.com/Genymobile/scrcpy/releases/download/v4.0/scrcpy-server-v4.0
PREBUILT_SERVER_SHA256=84924bd564a1eb6089c872c7521f968058977f91f5ff02514a8c74aff3210f3a

log() { printf '\n\033[1;34m==> %s\033[0m\n' "$*"; }

########################################
log "Installing build toolchain"
########################################
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y --no-install-recommends \
    build-essential meson ninja-build cmake nasm pkg-config \
    git wget curl ca-certificates xz-utils unzip zip rsync python3 file \
    autoconf automake libtool m4 \
    dpkg-dev fakeroot \
    zlib1g-dev libudev-dev \
    libx11-dev libxext-dev libxcursor-dev libxi-dev libxrandr-dev \
    libxfixes-dev libxss-dev libxkbcommon-dev libxtst-dev libxrender-dev \
    libwayland-dev libwayland-bin wayland-protocols libdecor-0-dev \
    libdrm-dev libgbm-dev \
    libgl1-mesa-dev libegl1-mesa-dev libgles2-mesa-dev \
    libasound2-dev libpulse-dev libdbus-1-dev libibus-1.0-dev

########################################
log "Copying source into a writable build tree"
########################################
# $BUILD lives in the container's ephemeral layer; only $BUILD/app/deps/work may
# be a persistent cache volume, so do not wipe $BUILD. rsync skips the cache dir.
mkdir -p "$BUILD" "$BUILD/app/deps/work"
rsync -a \
    --exclude '.git' --exclude 'dist' --exclude 'build' \
    --exclude 'build-*' --exclude 'build_*' \
    --exclude 'app/deps/work' \
    "$SRC"/ "$BUILD"/
cd "$BUILD"

VERSION=$(grep -oP "version:\s*'\K[^']+" meson.build | head -1)
DEB_VERSION=${DEB_VERSION:-${VERSION}-1}
log "scrcpy-auto version: $VERSION  (deb: $DEB_VERSION)"

########################################
log "Patching FFmpeg recipe for fork needs (swscale + PNG encoder)"
########################################
# The fork's screencap uses libswscale (sws_scale) and the PNG encoder, both of
# which the stock deps recipe disables. Patch the throwaway copy only.
sed -i 's/--disable-swscale/--enable-swscale/' app/deps/ffmpeg.sh
grep -q -- '--enable-encoder=png' app/deps/ffmpeg.sh \
    || sed -i 's/--enable-decoder=png/--enable-decoder=png\n        --enable-encoder=png/' app/deps/ffmpeg.sh
# The client is built with -Dv4l2=false to keep the runtime dependency surface
# minimal, so drop v4l2/libv4l2 from the FFmpeg build too (otherwise configure
# fails on a missing libv4l2).
sed -i '/--enable-libv4l2/d; /--enable-outdev=v4l2/d; /--enable-encoder=rawvideo/d' app/deps/ffmpeg.sh
# Drop any poisoned FFmpeg configure state left by a previous failed cached run.
rm -rf app/deps/work/build/ffmpeg-*/linux-native-static 2>/dev/null || true

########################################
log "Building static dependencies (this is the long part)"
########################################
cd app/deps
./sdl.sh   linux native static
./dav1d.sh linux native static
./ffmpeg.sh linux native static
./libusb.sh linux native static
if [[ "$BUNDLE_ADB" == 1 ]]; then
    ./adb_linux.sh linux native static
fi
cd "$BUILD"

DEPS="$BUILD/app/deps/work/install/linux-native-static"
ADBDIR="$BUILD/app/deps/work/install/adb-linux"

########################################
log "Configuring and building the client"
########################################
unset PKG_CONFIG_PATH
export PKG_CONFIG_LIBDIR="$DEPS/lib/pkgconfig"

rm -rf build-deb
meson setup build-deb \
    -Dc_args="-I$DEPS/include" \
    -Dc_link_args="-L$DEPS/lib" \
    --buildtype=release \
    --strip \
    -Db_lto=true \
    --prefix=/usr \
    -Dcompile_server=false \
    -Dportable=false \
    -Dstatic=true \
    -Dv4l2=false \
    -Dusb=true
ninja -C build-deb

########################################
log "Staging install tree (meson install to DESTDIR)"
########################################
STAGE="$BUILD/stage"
rm -rf "$STAGE"
DESTDIR="$STAGE" ninja -C build-deb install

# Compress the man page (keeps the package tidy / lintian-clean).
if [[ -f "$STAGE/usr/share/man/man1/scrcpy-auto.1" ]]; then
    gzip -9n "$STAGE/usr/share/man/man1/scrcpy-auto.1"
fi

########################################
log "Bundling the Android server"
########################################
if [[ "$SERVER_MODE" != none ]]; then
    install -d "$STAGE/usr/share/scrcpy-auto"
    case "$SERVER_MODE" in
        prebuilt)
            wget -q "$PREBUILT_SERVER_URL" -O /tmp/scrcpy-auto-server
            echo "$PREBUILT_SERVER_SHA256  /tmp/scrcpy-auto-server" | sha256sum --check
            install -m 0644 /tmp/scrcpy-auto-server \
                "$STAGE/usr/share/scrcpy-auto/scrcpy-auto-server"
            ;;
        path:*)
            install -m 0644 "${SERVER_MODE#path:}" \
                "$STAGE/usr/share/scrcpy-auto/scrcpy-auto-server"
            ;;
        *)
            echo "Unknown SERVER_MODE: $SERVER_MODE" >&2; exit 1 ;;
    esac
fi

########################################
log "Bundling adb"
########################################
if [[ "$BUNDLE_ADB" == 1 ]]; then
    install -d "$STAGE/usr/lib/scrcpy-auto"
    install -m 0755 "$ADBDIR/adb" "$STAGE/usr/lib/scrcpy-auto/adb"
fi

########################################
log "Computing dependencies and writing package metadata"
########################################
mkdir -p "$STAGE/DEBIAN"

# Resolve runtime library dependencies with dpkg-shlibdeps; fall back to a
# minimal, base-image-only set if it cannot run.
DEPENDS=""
(
    cd "$STAGE"
    mkdir -p debian
    printf 'Source: scrcpy-auto\nPackage: scrcpy-auto\nArchitecture: amd64\n' > debian/control
    dpkg-shlibdeps -O --ignore-missing-info usr/bin/scrcpy-auto 2>/dev/null \
        | sed -n 's/^shlibs:Depends=//p'
    rm -rf debian
) > /tmp/shlibdeps.txt || true
DEPENDS=$(cat /tmp/shlibdeps.txt)
if ! grep -q libc6 <<<"$DEPENDS"; then
    echo "dpkg-shlibdeps produced no usable output; using minimal fallback"
    DEPENDS="libc6, libgcc-s1, zlib1g"
fi
echo "Depends: $DEPENDS"

INSTALLED_SIZE=$(du -sk --exclude=DEBIAN "$STAGE" | cut -f1)

cat > "$STAGE/DEBIAN/control" <<EOF
Package: scrcpy-auto
Version: $DEB_VERSION
Architecture: amd64
Maintainer: scrcpy-auto packaging <scrcpy-auto@localhost>
Section: utils
Priority: optional
Installed-Size: $INSTALLED_SIZE
Depends: $DEPENDS
Recommends: libgl1, libegl1, libx11-6, libxext6, libwayland-client0, libxkbcommon0, libpulse0, libasound2t64
Homepage: https://github.com/Genymobile/scrcpy
Description: Display and control Android devices (scrcpy-auto automation fork)
 scrcpy-auto is an automation-focused fork of scrcpy that adds a daemon mode,
 headless control/screencap commands and a plugin protocol on top of upstream
 mirroring.
 .
 This build statically links FFmpeg (with swscale + PNG), SDL3, dav1d and
 libusb into the client, and bundles a prebuilt Android server and adb, so it
 installs and runs on an offline host with no additional packages. The
 Recommends are only needed for on-screen mirroring on a desktop session.
EOF

# Maintainer scripts: provide adb on PATH without clobbering an existing adb.
if [[ "$BUNDLE_ADB" == 1 ]]; then
    cat > "$STAGE/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
if [ "$1" = configure ]; then
    if [ ! -e /usr/bin/adb ]; then
        ln -s /usr/lib/scrcpy-auto/adb /usr/bin/adb
    fi
fi
exit 0
EOF
    cat > "$STAGE/DEBIAN/prerm" <<'EOF'
#!/bin/sh
set -e
if [ -L /usr/bin/adb ] && [ "$(readlink /usr/bin/adb)" = /usr/lib/scrcpy-auto/adb ]; then
    rm -f /usr/bin/adb
fi
exit 0
EOF
    chmod 0755 "$STAGE/DEBIAN/postinst" "$STAGE/DEBIAN/prerm"
fi

########################################
log "Building the .deb"
########################################
mkdir -p "$OUT"
DEB="$OUT/scrcpy-auto_${DEB_VERSION}_amd64.deb"
dpkg-deb --root-owner-group --build "$STAGE" "$DEB"
dpkg-deb --info "$DEB"
echo
echo "BUILT: $DEB"
