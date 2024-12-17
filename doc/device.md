# Device

Some command line arguments perform actions on the device itself while scrcpy is
running.

## Stay awake

To prevent the device from sleeping after a delay **when the device is plugged
in**:

```bash
scrcpy --stay-awake
scrcpy -w
```

The initial state is restored when _scrcpy_ is closed.

If the device is not plugged in (i.e. only connected over TCP/IP),
`--stay-awake` has no effect (this is the Android behavior).

This changes the value of [`stay_on_while_plugged_in`], setting which can be
changed manually:

[`stay_on_while_plugged_in`]: https://developer.android.com/reference/android/provider/Settings.Global#STAY_ON_WHILE_PLUGGED_IN


```bash
# get the current show_touches value
adb shell settings get global stay_on_while_plugged_in
# enable for AC/USB/wireless chargers
adb shell settings put global stay_on_while_plugged_in 7
# disable
adb shell settings put global stay_on_while_plugged_in 0
```


## Screen off timeout

The Android screen automatically turns off after some delay.

To change this delay while scrcpy is running:

```bash
scrcpy --screen-off-timeout=300  # 300 seconds (5 minutes)
```

The initial value is restored on exit.

It is possible to change this setting manually:

```bash
# get the current screen_off_timeout value
adb shell settings get system screen_off_timeout
# set a new value (in milliseconds)
adb shell settings put system screen_off_timeout 30000
```

Note that the Android value is in milliseconds, but the scrcpy command line
argument is in seconds.


## Turn screen off

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

Since Android 15, it is possible to change this setting manually:

```
# turn screen off (0 for main display)
adb shell cmd display power-off 0
# turn screen on
adb shell cmd display power-on 0
```


## Show touches

For presentations, it may be useful to show physical touches (on the physical
device). Android exposes this feature in _Developers options_.

_Scrcpy_ provides an option to enable this feature on start and restore the
initial value on exit:

```bash
scrcpy --show-touches
scrcpy -t   # short version
```

Note that it only shows _physical_ touches (by a finger on the device).

It is possible to change this setting manually:

```bash
# get the current show_touches value
adb shell settings get system show_touches
# enable show_touches
adb shell settings put system show_touches 1
# disable show_touches
adb shell settings put system show_touches 0
```

## Power off on close

To turn the device screen off when closing _scrcpy_:

```bash
scrcpy --power-off-on-close
```

## Power on on start

By default, on start, the device is powered on. To prevent this behavior:

```bash
scrcpy --no-power-on
```


## Start Android app

To list the Android apps installed on the device:

```bash
scrcpy --list-apps
```

An app, selected by its package name, can be launched on start:

```
scrcpy --start-app=org.mozilla.firefox
```

This feature can be used to run an app in a [virtual
display](virtual_display.md):

```
scrcpy --new-display=1920x1080 --start-app=org.videolan.vlc
```

The app can be optionally forced-stop before being started, by adding a `+`
prefix:

```
scrcpy --start-app=+org.mozilla.firefox
```

For convenience, it is also possible to select an app by its name, by adding a
`?` prefix:

```
scrcpy --start-app=?firefox
```

But retrieving app names may take some time (sometimes several seconds), so
passing the package name is recommended.

The `+` and `?` prefixes can be combined (in that order):

```
scrcpy --start-app=+?firefox
```
