#!/bin/bash
PLIST_FILE="$HOME/Library/LaunchAgents/com.user.autoscrpcy.plist"
launchctl unload -w "$PLIST_FILE" 2>/dev/null || true
rm -f "$PLIST_FILE" "$HOME/.local/bin/auto_scrcpy.sh"
echo "✅ Uninstalled scrcpy auto-connect daemon."
