#!/bin/bash
# Quick test script

cd /home/sofinco/project/scrcpy/build-linux/app

echo "Testing scrcpy with optimizations..."
echo ""
echo "Binary: $(pwd)/scrcpy"
echo "Size: $(ls -lh scrcpy | awk '{print $5}')"
echo ""

# Test 1: Basic connection
echo "Test 1: Basic connection (no audio)"
timeout 10s ./scrcpy --no-audio --max-fps=60 2>&1 | grep -E "INFO|ERROR|WARN" | head -20

echo ""
echo "If device disconnects, try:"
echo "  1. adb kill-server && adb start-server"
echo "  2. Reconnect USB cable"
echo "  3. Check USB debugging is enabled"
echo "  4. Try: ./scrcpy --no-audio --max-fps=60 --no-control"
