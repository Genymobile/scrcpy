#!/usr/bin/env bash
# Automated test script for scrcpy --adb-overlay / --adb-overlay-all
# Run this in Terminal 2 while scrcpy-overlay is running in Terminal 1.

echo "=== 1. Success (Exit 0) — Single-line stdout ==="
adb shell getprop ro.product.model
sleep 3

echo "=== 2. Success (Exit 0) — Multi-line stdout ==="
adb shell "getprop ro.product.model; getprop ro.build.version.release; getprop ro.build.id"
sleep 3

echo "=== 3. Error (Exit 1) — stderr output ==="
adb shell ls /nonexistent_path
sleep 3

echo "=== 4. Command Not Found (Exit 127) ==="
adb shell non_existent_binary_xyz
sleep 3

echo "=== 5. Custom Return Code (Exit 42) ==="
adb shell "echo 'Custom error: sensor calibration failed' >&2; exit 42"
sleep 3

echo "=== 6. Long-Running Command (3s Duration: RUNNING -> DONE) ==="
adb shell "sleep 3; echo 'Completed 3-second task'"
sleep 3

echo "=== 7. Rapid Command Stack (3 commands in quick succession) ==="
adb shell getprop ro.product.device
sleep 0.4
adb shell getprop ro.build.type
sleep 0.4
adb shell ls /missing_dir
sleep 4

echo "=== 8. Mutating UI Action (Volume Up keyevent, Exit 0) ==="
adb shell input keyevent 24
sleep 3

echo "=== All overlay test scenarios completed! ==="
