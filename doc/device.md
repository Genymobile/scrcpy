# Device

## Selection

If exactly one device is connected (i.e. listed by `adb devices`), then it is
automatically selected.

However, if there are multiple devices connected, you must specify the one to
use in one of 4 ways:
 - by its serial:
   ```bash
   scrcpy --serial=0123456789abcdef
   scrcpy -s 0123456789abcdef   # short version

   # the serial is the ip:port if connected over TCP/IP (same behavior as adb)
   scrcpy --serial=192.168.1.1:5555
   ```
 - the one connected over USB (if there is exactly one):
   ```bash
   scrcpy --select-usb
   scrcpy -d   # short version
   ```
 - the one connected over TCP/IP (if there is exactly one):
   ```bash
   scrcpy --select-tcpip
   scrcpy -e   # short version
   ```
 - a device already listening on TCP/IP (see [below](#tcpip-wireless)):
   ```bash
   scrcpy --tcpip=192.168.1.1:5555
   scrcpy --tcpip=192.168.1.1        # default port is 5555
   ```

The serial may also be provided via the environment variable `ANDROID_SERIAL`
(also used by `adb`):

```bash
# in bash
export ANDROID_SERIAL=0123456789abcdef
scrcpy
```

```cmd
:: in cmd
set ANDROID_SERIAL=0123456789abcdef
scrcpy
```

```powershell
# in PowerShell
$env:ANDROID_SERIAL = '0123456789abcdef'
scrcpy
```


## TCP/IP (wireless)

_Scrcpy_ uses `adb` to communicate with the device, and `adb` can [connect] to a
device over TCP/IP. The device must be connected on the same network as the
computer.

[connect]: https://developer.android.com/studio/command-line/adb.html#wireless


### Automatic

An option `--tcpip` allows to configure the connection automatically. There are
two variants.

If the device (accessible at 192.168.1.1 in this example) already listens on a
port (typically 5555) for incoming _adb_ connections, then run:

```bash
scrcpy --tcpip=192.168.1.1       # default port is 5555
scrcpy --tcpip=192.168.1.1:5555
```

If _adb_ TCP/IP mode is disabled on the device (or if you don't know the IP
address), connect the device over USB, then run:

```bash
scrcpy --tcpip   # without arguments
```

It will automatically find the device IP address and adb port, enable TCP/IP
mode if necessary, then connect to the device before starting.


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
7. Run `scrcpy` as usual.
8. Run `adb disconnect` once you're done.

Since Android 11, a [wireless debugging option][adb-wireless] allows to bypass
having to physically connect your device directly to your computer.

[adb-wireless]: https://developer.android.com/studio/command-line/adb#wireless-android11-command-line


## Autostart

A small tool (by the scrcpy author) allows to run arbitrary commands whenever a
new Android device is connected: [AutoAdb]. It can be used to start scrcpy:

```bash
autoadb scrcpy -s '{}'
```

[AutoAdb]: https://github.com/rom1v/autoadb


## Display

If several displays are available on the Android device, it is possible to
select the display to mirror:

```bash
scrcpy --display=1
```

The list of display ids can be retrieved by:

```bash
scrcpy --list-displays
```

A secondary display may only be controlled if the device runs at least Android
10 (otherwise it is mirrored as read-only).


## Actions

Some command line arguments perform actions on the device itself while scrcpy is
running.


### Stay awake

To prevent the device from sleeping after a delay **when the device is plugged
in**:

```bash
scrcpy --stay-awake
scrcpy -w
```

The initial state is restored when _scrcpy_ is closed.

If the device is not plugged in (i.e. only connected over TCP/IP),
`--stay-awake` has no effect (this is the Android behavior).


### Turn screen off

It is possible to turn the device screen off while mirroring on start with a
command-line option:

```bash
scrcpy --turn-screen-off
scrcpy -S   # short version
```

Or by pressing <kbd>MOD</kbd>+<kbd>o</kbd> at any time (see
[shortcuts](shortcuts.md)).

To turn it back on, press <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>o</kbd>.

On Android, the `POWER` button always turns the screen on. For convenience, if
`POWER` is sent via _scrcpy_ (via right-click or <kbd>MOD</kbd>+<kbd>p</kbd>),
it will force to turn the screen off after a small delay (on a best effort
basis). The physical `POWER` button will still cause the screen to be turned on.

It can also be useful to prevent the device from sleeping:

```bash
scrcpy --turn-screen-off --stay-awake
scrcpy -Sw   # short version
```


### Show touches

For presentations, it may be useful to show physical touches (on the physical
device). Android exposes this feature in _Developers options_.

_Scrcpy_ provides an option to enable this feature on start and restore the
initial value on exit:

```bash
scrcpy --show-touches
scrcpy -t   # short version
```

Note that it only shows _physical_ touches (by a finger on the device).


### Power off on close

To turn the device screen off when closing _scrcpy_:

```bash
scrcpy --power-off-on-close
```

### Power on on start

By default, on start, the device is powered on. To prevent this behavior:

```bash
scrcpy --no-power-on
```

