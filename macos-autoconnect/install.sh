#!/bin/bash
set -e

echo "=================================================="
echo "  Scrcpy Bluetooth / Radio Auto-Connect Installer"
echo "=================================================="

read -p "Enter your Android's Bluetooth/Hotspot Router IP [default: 192.168.44.1]: " INPUT_IP
PHONE_IP=${INPUT_IP:-192.168.44.1}

BIN_DIR="$HOME/.local/bin"
PLIST_DIR="$HOME/Library/LaunchAgents"
PLIST_FILE="$PLIST_DIR/com.user.autoscrpcy.plist"

mkdir -p "$BIN_DIR" "$PLIST_DIR"

cat << DAEMON > "$BIN_DIR/auto_scrcpy.sh"
#!/bin/bash
export PATH="/usr/local/bin:/opt/homebrew/bin:/opt/local/bin:\$HOME/.local/bin:/usr/bin:/bin:/usr/sbin:/sbin:\$PATH"

PHONE_IP="${PHONE_IP}"
TARGET="\${PHONE_IP}:5555"

if ! command -v adb &> /dev/null || ! command -v scrcpy &> /dev/null; then
    echo "Error: adb or scrcpy not found in PATH." >&2
    exit 1
fi

while true; do
    if ping -c 1 -W 1 "\$PHONE_IP" &> /dev/null; then
        if ! pgrep -f "scrcpy.*\${TARGET}" > /dev/null; then
            if ! adb devices | grep -q "\${TARGET}[[:space:]]*device"; then
                adb connect "\$TARGET" &> /dev/null
                sleep 2
            fi
            if adb devices | grep -q "\${TARGET}[[:space:]]*device"; then
                scrcpy -s "\$TARGET" --window-title="Android Radio Mirror" &> /dev/null
                sleep 5
            fi
        fi
    fi
    sleep 3
done
DAEMON

chmod +x "$BIN_DIR/auto_scrcpy.sh"

cat << PLIST > "$PLIST_FILE"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://apple.com">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>com.user.autoscrpcy</string>
    <key>ProgramArguments</key>
    <array>
        <string>${BIN_DIR}/auto_scrcpy.sh</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <true/>
    <key>StandardOutPath</key>
    <string>/tmp/autoscrpcy.out.log</string>
    <key>StandardErrorPath</key>
    <string>/tmp/autoscrpcy.err.log</string>
</dict>
</plist>
PLIST

launchctl unload "$PLIST_FILE" 2>/dev/null || true
launchctl load -w "$PLIST_FILE"

echo "✅ Installed and started auto-connect daemon for $PHONE_IP:5555"
