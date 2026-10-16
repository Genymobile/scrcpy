# MPRIS

On Linux, scrcpy can expose the media sessions of the device on the D-Bus
session bus using the [MPRIS] interface, so that the media controls of the
desktop (media widgets, media keys, tools like [playerctl]) can display and
control what is playing on the device.

[MPRIS]: https://specifications.freedesktop.org/mpris-spec/latest/
[playerctl]: https://github.com/altdesktop/playerctl

To enable it:

```bash
scrcpy --mpris
```

For example, with `playerctl`:

```bash
playerctl --player=scrcpy status
playerctl --player=scrcpy metadata
playerctl --player=scrcpy play-pause
playerctl --player=scrcpy next
```

Seeking is only available if the app on the device supports it.

The bus name is `org.mpris.MediaPlayer2.scrcpy`. If several scrcpy instances
run at the same time, the additional instances use
`org.mpris.MediaPlayer2.scrcpy.instance<pid>`.

If several apps have an active media session on the device, the one controlled
by the media keys of the device is exposed.

This feature requires device control, so it is not compatible with
`--no-control`, `--otg` and `--video-source=camera`.
