#!/bin/bash
set -euo pipefail

REPOSITORY="${SCRCPY_AUTO_REPOSITORY:-fish-dapangyu-dev/scrcpy-auto}"
VERSION="${SCRCPY_AUTO_VERSION:-v4.1-auto5}"
PREFIX="${SCRCPY_AUTO_PREFIX:-$HOME/.local}"
BIN_DIR="${SCRCPY_AUTO_BIN_DIR:-$PREFIX/bin}"
INSTALL_BASE="${SCRCPY_AUTO_INSTALL_BASE:-$PREFIX/lib/scrcpy-auto}"

case "$VERSION" in
    ""|*[!A-Za-z0-9._-]*)
        echo "Invalid SCRCPY_AUTO_VERSION: $VERSION" >&2
        exit 1
        ;;
esac

if [[ "$(uname -s)" != Darwin ]]
then
    echo "This installer supports macOS only." >&2
    exit 1
fi

MACHINE="$(uname -m)"
if [[ "$MACHINE" == x86_64 ]] &&
        [[ "$(sysctl -in sysctl.proc_translated 2>/dev/null || true)" == 1 ]]
then
    MACHINE=arm64
fi

case "$MACHINE" in
    arm64)
        ARCH=arm64
        ;;
    x86_64)
        ARCH=x86_64
        ;;
    *)
        echo "Unsupported Mac architecture: $MACHINE" >&2
        exit 1
        ;;
esac

for COMMAND in curl tar shasum
do
    if ! command -v "$COMMAND" >/dev/null 2>&1
    then
        echo "Required command is missing: $COMMAND" >&2
        exit 1
    fi
done

ASSET="scrcpy-auto-macos-$ARCH-$VERSION.tar.gz"
RELEASE_BASE="https://github.com/$REPOSITORY/releases/download/$VERSION"
TMP_PARENT="${TMPDIR:-/tmp}"
TMP_DIR="$(mktemp -d "$TMP_PARENT/scrcpy-auto-install.XXXXXX")"

cleanup() {
    if [[ -n "${TMP_DIR:-}" && -d "$TMP_DIR" ]]
    then
        rm -rf -- "$TMP_DIR"
    fi
}
trap cleanup EXIT INT TERM

echo "Downloading $ASSET..."
curl --fail --location --retry 3 --silent --show-error \
    "$RELEASE_BASE/$ASSET" \
    --output "$TMP_DIR/$ASSET"
curl --fail --location --retry 3 --silent --show-error \
    "$RELEASE_BASE/SHA256SUMS-macos.txt" \
    --output "$TMP_DIR/SHA256SUMS-macos.txt"

EXPECTED_LINE="$(grep "  $ASSET\$" "$TMP_DIR/SHA256SUMS-macos.txt" || true)"
if [[ -z "$EXPECTED_LINE" ]]
then
    echo "No checksum published for $ASSET" >&2
    exit 1
fi
printf '%s\n' "$EXPECTED_LINE" > "$TMP_DIR/selected.sha256"
(
    cd "$TMP_DIR"
    shasum -a 256 -c selected.sha256
)

tar xzf "$TMP_DIR/$ASSET" -C "$TMP_DIR"
PACKAGE_DIR="$TMP_DIR/${ASSET%.tar.gz}"

if [[ ! -x "$PACKAGE_DIR/scrcpy-auto" ]]
then
    echo "Archive does not contain an executable scrcpy-auto client." >&2
    exit 1
fi
if [[ ! -s "$PACKAGE_DIR/scrcpy-auto-server" ]]
then
    echo "Archive does not contain the matching scrcpy-auto-server." >&2
    exit 1
fi

ARCHIVE_SHA="$(shasum -a 256 "$TMP_DIR/$ASSET" | awk '{print $1}')"
DESTINATION="$INSTALL_BASE/$VERSION-$ARCH-${ARCHIVE_SHA:0:12}"

mkdir -p "$INSTALL_BASE" "$BIN_DIR"
if [[ ! -d "$DESTINATION" ]]
then
    mv "$PACKAGE_DIR" "$DESTINATION"
fi

LINK_PATH="$BIN_DIR/scrcpy-auto"
if [[ -e "$LINK_PATH" && ! -L "$LINK_PATH" ]]
then
    echo "Refusing to replace non-symlink: $LINK_PATH" >&2
    echo "Set SCRCPY_AUTO_BIN_DIR to another directory and retry." >&2
    exit 1
fi

TEMP_LINK="$BIN_DIR/.scrcpy-auto-link.$$"
ln -s "$DESTINATION/scrcpy-auto" "$TEMP_LINK"
mv -f "$TEMP_LINK" "$LINK_PATH"

"$LINK_PATH" --version

echo
echo "Installed scrcpy-auto $VERSION for $ARCH."
echo "  command: $LINK_PATH"
echo "  bundle:  $DESTINATION"
echo "  server:  $DESTINATION/scrcpy-auto-server"

ADB_COMMAND="${ADB:-adb}"
if ! command -v "$ADB_COMMAND" >/dev/null 2>&1
then
    echo
    echo "WARNING: adb was not found in the system environment."
    echo "Install it separately before running scrcpy-auto:"
    echo "  brew install android-platform-tools"
fi

case ":$PATH:" in
    *":$BIN_DIR:"*)
        ;;
    *)
        echo
        echo "Add this directory to PATH:"
        echo "  export PATH=\"$BIN_DIR:\$PATH\""
        ;;
esac
