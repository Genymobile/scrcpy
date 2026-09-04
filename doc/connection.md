# Connection

## Selection

If exactly one device is connected (i.e. listed by `adb devices`), then it is
automatically selected.

However, if there are multiple devices connected, you must specify the one to
use in one of 4 ways:
 - by its serial:
   ```bash
   slink --serial=0123456789abcdef
   slink -s 0123456789abcdef   # short version

   # the serial is the ip:port if connected over TCP/IP (same behavior as adb)
   slink --serial=192.168.1.1:5555
   ```
 - the one connected over USB (if there is exactly one):
   ```bash
   slink --select-usb
   slink -d   # short version
   ```
 - the one connected over TCP/IP (if there is exactly one):
   ```bash
   slink --select-tcpip
   slink -e   # short version
   ```
 - a device already listening on TCP/IP (see [below](#tcpip-wireless)):
   ```bash
   slink --tcpip=192.168.1.1:5555
   slink --tcpip=192.168.1.1        # default port is 5555
   ```

The serial may also be provided via the environment variable `ANDROID_SERIAL`
(also used by `adb`):

```bash
# in bash
export ANDROID_SERIAL=0123456789abcdef
slink
```

```cmd
:: in cmd
set ANDROID_SERIAL=0123456789abcdef
slink
```

```powershell
# in PowerShell
$env:ANDROID_SERIAL = '0123456789abcdef'
slink
```


## TCP/IP (wireless)

_slink_ uses `adb` to communicate with the device, and `adb` can [connect] to a
device over TCP/IP. The device must be connected on the same network as the
computer.

[connect]: https://developer.android.com/studio/command-line/adb.html#wireless


### Automatic

An option `--tcpip` allows to configure the connection automatically. There are
two variants.

If _adb_ TCP/IP mode is disabled on the device (or if you don't know the IP
address), connect the device over USB, then run:

```bash
slink --tcpip   # without arguments
```

It will automatically find the device IP address and adb port, enable TCP/IP
mode if necessary, then connect to the device before starting.

If the device (accessible at 192.168.1.1 in this example) already listens on a
port (typically 5555) for incoming _adb_ connections, then run:

```bash
slink --tcpip=192.168.1.1       # default port is 5555
slink --tcpip=192.168.1.1:5555
```

Prefix the address with a '+' to force a reconnection:

```bash
slink --tcpip=+192.168.1.1
```


### Manual

Alternatively, it is possible to enable the TCP/IP connection manually using
`adb`:

1. Plug the device into a USB port on your computer.
2. Connect the device to the same Wi-Fi network as your computer.
3. Get your device IP address, in Settings → About phone → Status, or by
   executing this command:

    ```bash
    adb shell ip route | awk '{print $9}'
    ```

4. Enable `adb` over TCP/IP on your device: `adb tcpip 5555`.
5. Unplug your device.
6. Connect to your device: `adb connect DEVICE_IP:5555` _(replace `DEVICE_IP`
with the device IP address you found)_.
7. Run `slink` as usual.
8. Run `adb disconnect` once you're done.

Since Android 11, a [wireless debugging option][adb-wireless] allows you to
bypass having to physically connect your device to your computer.

[adb-wireless]: https://developer.android.com/studio/command-line/adb#wireless-android11-command-line


## Autostart

A small tool (by the slink author) allows you to run arbitrary commands
whenever a new Android device is connected: [AutoAdb]. It can be used to start
slink:

```bash
autoadb slink -s '{}'
```

[AutoAdb]: https://github.com/rom1v/autoadb
