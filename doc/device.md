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
