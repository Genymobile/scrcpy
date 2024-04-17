# Shortcuts

Actions can be performed on the scrcpy window using keyboard and mouse
shortcuts.

In the following list, <kbd>MOD</kbd> is the shortcut modifier. By default, it's
(left) <kbd>Alt</kbd> or (left) <kbd>Super</kbd>.

It can be changed using `--shortcut-mod`. Possible keys are `lctrl`, `rctrl`,
`lalt`, `ralt`, `lsuper` and `rsuper`. For example:

```bash
# use RCtrl for shortcuts
scrcpy --shortcut-mod=rctrl

# use either LCtrl+LAlt or LSuper for shortcuts
scrcpy --shortcut-mod=lctrl+lalt,lsuper
```

_<kbd>[Super]</kbd> is typically the <kbd>Windows</kbd> or <kbd>Cmd</kbd> key._

[Super]: https://en.wikipedia.org/wiki/Super_key_(keyboard_button)

 | Action                                      |   Shortcut
 | ------------------------------------------- |:-----------------------------
 | Switch fullscreen mode                      | <kbd>MOD</kbd>+<kbd>f</kbd>
 | Rotate display left                         | <kbd>MOD</kbd>+<kbd>←</kbd> _(left)_
 | Rotate display right                        | <kbd>MOD</kbd>+<kbd>→</kbd> _(right)_
 | Flip display horizontally                   | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>←</kbd> _(left)_ \| <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>→</kbd> _(right)_
 | Flip display vertically                     | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>↑</kbd> _(up)_ \| <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>↓</kbd> _(down)_
 | Resize window to 1:1 (pixel-perfect)        | <kbd>MOD</kbd>+<kbd>g</kbd>
 | Resize window to remove black borders       | <kbd>MOD</kbd>+<kbd>w</kbd> \| _Double-left-click¹_
 | Click on `HOME`                             | <kbd>MOD</kbd>+<kbd>h</kbd> \| _Middle-click_
 | Click on `BACK`                             | <kbd>MOD</kbd>+<kbd>b</kbd> \| <kbd>MOD</kbd>+<kbd>Backspace</kbd> \| _Right-click²_
 | Click on `APP_SWITCH`                       | <kbd>MOD</kbd>+<kbd>s</kbd> \| _4th-click³_
 | Click on `MENU` (unlock screen)⁴            | <kbd>MOD</kbd>+<kbd>m</kbd>
 | Click on `VOLUME_UP`                        | <kbd>MOD</kbd>+<kbd>↑</kbd> _(up)_
 | Click on `VOLUME_DOWN`                      | <kbd>MOD</kbd>+<kbd>↓</kbd> _(down)_
 | Click on `POWER`                            | <kbd>MOD</kbd>+<kbd>p</kbd>
 | Power on                                    | _Right-click²_
 | Turn device screen off (keep mirroring)     | <kbd>MOD</kbd>+<kbd>o</kbd>
 | Turn device screen on                       | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>o</kbd>
 | Rotate device screen                        | <kbd>MOD</kbd>+<kbd>r</kbd>
 | Expand notification panel                   | <kbd>MOD</kbd>+<kbd>n</kbd> \| _5th-click³_
 | Expand settings panel                       | <kbd>MOD</kbd>+<kbd>n</kbd>+<kbd>n</kbd> \| _Double-5th-click³_
 | Collapse panels                             | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>n</kbd>
 | Copy to clipboard⁵                          | <kbd>MOD</kbd>+<kbd>c</kbd>
 | Cut to clipboard⁵                           | <kbd>MOD</kbd>+<kbd>x</kbd>
 | Synchronize clipboards and paste⁵           | <kbd>MOD</kbd>+<kbd>v</kbd>
 | Inject computer clipboard text              | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd>
 | Open keyboard settings (HID keyboard only)  | <kbd>MOD</kbd>+<kbd>k</kbd>
 | Enable/disable FPS counter (on stdout)      | <kbd>MOD</kbd>+<kbd>i</kbd>
 | Pinch-to-zoom/rotate                        | <kbd>Ctrl</kbd>+_click-and-move_
 | Tilt (slide vertically with 2 fingers)      | <kbd>Shift</kbd>+_click-and-move_
 | Drag & drop APK file                        | Install APK from computer
 | Drag & drop non-APK file                    | [Push file to device](control.md#push-file-to-device)

_¹Double-click on black borders to remove them._  
_²Right-click turns the screen on if it was off, presses BACK otherwise._  
_³4th and 5th mouse buttons, if your mouse has them._  
_⁴For react-native apps in development, `MENU` triggers development menu._  
_⁵Only on Android >= 7._

Shortcuts with repeated keys are executed by releasing and pressing the key a
second time. For example, to execute "Expand settings panel":

 1. Press and keep pressing <kbd>MOD</kbd>.
 2. Then double-press <kbd>n</kbd>.
 3. Finally, release <kbd>MOD</kbd>.

All <kbd>Ctrl</kbd>+_key_ shortcuts are forwarded to the device, so they are
handled by the active application.
