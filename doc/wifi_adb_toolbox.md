
# ADB over Wi-Fi quick guide (with optional toolbox)

This page explains how to use **ADB over Wi-Fi** with **scrcpy**, on Windows, macOS, and Linux.  
It also links to an optional third-party “Wi-Fi ADB Toolbox” that automates common tasks.

> **Note:** scrcpy works over USB or Wi-Fi. Wi-Fi is convenient for mobility, but **do not** expose ADB over TCP to untrusted networks.

---

## Prerequisites

- **Android device** with **Developer options** enabled.
  - For Android ≤10: classic TCP/IP mode (`adb tcpip 5555`).
  - For Android 11+ (recommended): **Wireless debugging** with **pairing**.
- **ADB** (Android Platform Tools) available in your `PATH`.
- **scrcpy** installed and available in your `PATH`.
- **Phone and computer on the same Wi-Fi/SSID** (same LAN segment).
- Allow your firewall to let `adb` communicate on the local network (port **5555** for classic TCP/IP mode, and a random port for **pairing** on Android 11+).

---

## Quick commands

### Windows (PowerShell or CMD)

#### A) Classic TCP/IP (Android ≤10 or if you prefer tcpip mode)

1. Connect the phone via **USB** once and authorize the RSA prompt.
2. Enable TCP mode:
   ```bat
   adb devices
   adb tcpip 5555
   ```
3. Find your phone IP (on the phone: Wi-Fi details) or via ADB:
   ```bat
   adb shell ip -o -4 addr show wlan0
   ```
4. Connect and start scrcpy:
   ```bat
   adb connect <PHONE_IP>:5555
   scrcpy
   ```
5. (Optional) Quality presets:
   ```bat
   scrcpy --video-bit-rate 8M  --max-size 1080    :: Default
   scrcpy --video-bit-rate 16M --max-size 1440    :: High
   scrcpy --video-bit-rate 32M --max-size 2160    :: Ultra
   ```
6. Return to USB later (when reconnected by cable):
   ```bat
   adb -s <PHONE_IP>:5555 usb
   ```

#### B) Wireless debugging pairing (Android 11+)

1. On the phone: **Developer options → Wireless debugging → Pair device with pairing code**.  
   Note the **IP:PORT** and 6-digit **pairing code**.
2. Pair:
   ```bat
   adb pair <PHONE_IP>:<PAIR_PORT>
   (enter the 6-digit code when prompted)
   ```
3. Connect the device endpoint (usually `:5555`):
   ```bat
   adb connect <PHONE_IP>:5555
   scrcpy
   ```

---

### macOS / Linux (Terminal)

Commands are the same:

#### A) Classic TCP/IP
```bash
adb devices
adb tcpip 5555
adb shell ip -o -4 addr show wlan0
adb connect <PHONE_IP>:5555
scrcpy
```

#### B) Wireless debugging (Android 11+)
```bash
adb pair <PHONE_IP>:<PAIR_PORT>      # enter the 6-digit code
adb connect <PHONE_IP>:5555
scrcpy
```

---

## Optional helper (third-party)

For a friendlier workflow (menus, presets, JSON inventory, and cross-platform helper), see:

- **Wi-Fi ADB Toolbox** (third-party):  
  <https://github.com/kakrusliandika/Scrcpy-Mode-Wifi>

This project automates the steps above (connect, list, presets, disconnect, etc.).  
It is external to scrcpy; please report issues and updates there.

---

## Network & security notes

- **Local network only.** Avoid enabling ADB TCP on public or untrusted networks.
- **Unique IP per device.** If multiple phones share the same hotspot/router IP range (e.g. captive hotspot or phone-as-hotspot), ensure each device gets a unique IP. Duplicate IPs will cause conflicts.
- **Firewalls.** Allow `adb` through your OS firewall for private networks. Classic TCP mode uses **5555/TCP**; Android 11 pairing uses a **random TCP port** for the pairing step.
- **Disable when done.** For classic TCP mode, switch back to USB:
  ```bash
  adb -s <PHONE_IP>:5555 usb
  ```
  Or just reboot the device.

---

## Troubleshooting

- **`device unauthorized`**  
  Reconnect USB, accept the RSA fingerprint prompt. You can also revoke authorizations on the phone (Developer options → Revoke USB debugging authorizations), then re-authorize.

- **`offline` or not listed**  
  Toggle **Wireless debugging** off/on (Android 11+), or run:
  ```bash
  adb kill-server
  adb start-server
  adb devices
  ```
  Ensure phone and PC are on the **same SSID**.

- **Cannot find phone IP**  
  Use:
  ```bash
  adb shell ip -o -4 addr show wlan0
  adb shell ip -o -4 addr show        # check other Wi-Fi/P2P interfaces (e.g. p2p0)
  ```
  Or read the IP from the phone’s Wi-Fi settings.

- **Multiple devices error (`more than one device/emulator`)**  
  Specify the target with `-s`, e.g.:
  ```bash
  adb -s <PHONE_IP>:5555 shell getprop ro.product.model
  scrcpy -s <PHONE_IP>:5555
  ```

- **Pairing fails (Android 11+)**  
  Ensure you use the **pairing port** for `adb pair` (not 5555), and the **6-digit code** shown on the phone. After pairing, connect to `<PHONE_IP>:5555`.

- **scrcpy not found**  
  Install scrcpy and ensure it’s in `PATH`. Launch `scrcpy -v` to verify.

- **HEVC/H.265 option doesn't work**  
  The device must support HEVC encoding. If not, omit `--video-codec=h265`.

---

## Useful commands (reference)

```bash
# List devices (USB/TCP)
adb devices

# Check state for a specific endpoint
adb -s <PHONE_IP>:5555 get-state

# Disconnect a specific endpoint or all
adb disconnect <PHONE_IP>:5555
adb disconnect

# Classic mode: enable/disable TCP server on device
adb tcpip 5555
adb -s <PHONE_IP>:5555 usb

# scrcpy examples
scrcpy                              # defaults
scrcpy --video-bit-rate 8M --max-size 1080
scrcpy --always-on-top --fullscreen
scrcpy --record session.mp4
```

---

*This page links to a third-party helper for convenience. It is not part of scrcpy and is maintained separately.*
