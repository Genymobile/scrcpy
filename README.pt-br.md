# scrcpy (v1.12.1)

Esta aplicação fornece visualização e controle de dispositivos Androids conectados via
USB (ou [via TCP/IP][article-tcpip]). Não requer nenhum acesso root.
Funciona em _GNU/Linux_, _Windows_ e _macOS_.

![screenshot](assets/screenshot-debian-600.jpg)

Foco em:

 - **leveza** (Nativo, mostra apenas a tela do dispositivo)
 - **performance** (30~60fps)
 - **qualidade** (1920×1080 ou acima)
 - **baixa latência** ([35~70ms][lowlatency])
 - **baixo tempo de inicialização** (~1 segundo para mostrar a primeira imagem)
 - **não intrusivo** (nada é deixado instalado no dispositivo)

[lowlatency]: https://github.com/Genymobile/scrcpy/pull/646


## Requisitos

O Dispositivo Android requer pelo menos a API 21 (Android 5.0).


Tenha certeza de ter [ativado a depuração USB][enable-adb] no(s) seu(s) dispositivo(s).

[enable-adb]: https://developer.android.com/studio/command-line/adb.html#Enabling


Em alguns dispositivos, você também precisará ativar [uma opção adicional][control] para controlá-lo usando o teclado e mouse.

[control]: https://github.com/Genymobile/scrcpy/issues/70#issuecomment-373286323


## Obtendo o app


### Linux

No Debian (_em testes_ e _sid_ por enquanto):

```
apt install scrcpy
```

O pacote [Snap] está disponível: [`scrcpy`][snap-link].

[snap-link]: https://snapstats.org/snaps/scrcpy

[snap]: https://en.wikipedia.org/wiki/Snappy_(package_manager)

Para Arch Linux, um pacote [AUR] está disponível: [`scrcpy`][aur-link].

[AUR]: https://wiki.archlinux.org/index.php/Arch_User_Repository
[aur-link]: https://aur.archlinux.org/packages/scrcpy/

Para Gentoo, uma [Ebuild] está disponível: [`scrcpy/`][ebuild-link].

[Ebuild]: https://wiki.gentoo.org/wiki/Ebuild
[ebuild-link]: https://github.com/maggu2810/maggu2810-overlay/tree/master/app-mobilephone/scrcpy


Você também pode [buildar a aplicação manualmente][BUILD] (não se preocupe, não é tão difícil).


### Windows

Para Windows, para simplicidade, um arquivo pré-buildado com todas as dependências
(incluindo `adb`) está disponível:

 - [`scrcpy-win64-v1.12.1.zip`][direct-win64]  
   _(SHA-256: 57d34b6d16cfd9fe169bc37c4df58ebd256d05c1ea3febc63d9cb0a027ab47c9)_

[direct-win64]: https://github.com/Genymobile/scrcpy/releases/download/v1.12.1/scrcpy-win64-v1.12.1.zip

Também disponível em [Chocolatey]:

[Chocolatey]: https://chocolatey.org/

```bash
choco install scrcpy
choco install adb    # se você ainda não o tem
```

And in [Scoop]:

```bash
scoop install scrcpy
scoop install adb    # se você ainda não o tem
```

[Scoop]: https://scoop.sh

Você também pode [bui]
You can also [buildar a aplicação manualmente][BUILD].


### macOS

A aplicação está disponível em [Homebrew]. Apenas a instale:

[Homebrew]: https://brew.sh/

```bash
brew install scrcpy
```

Você precisa do `adb`, acessível através do seu `PATH`. Se você ainda não o tem:

```bash
brew cask install android-platform-tools
```

Você também pode [buildar a aplicação manualmente][BUILD].


## Executar

Plugue um dispositivo Android e execute:

```bash
scrcpy
```

Também aceita argumentos de linha de comando, listados por:

```bash
scrcpy --help
```

## Funcionalidades

### Configuração de captura

#### Redução de tamanho

Algumas vezes, é útil espelhar um dispositivo Android em uma resolução menor para
aumentar performance.

Para limitar ambos(largura e altura) para algum valor (ex: 1024):

```bash
scrcpy --max-size 1024
scrcpy -m 1024  # versão reduzida
```

A outra dimensão é calculada para que a proporção do dispositivo seja preservada.
Dessa forma, um dispositivo em 1920x1080 será espelhado em 1024x576.


#### Mudanças no bit-rate

O Padrão de bit-rate é 8 mbps. Para mudar o bitrate do vídeo (ex: para 2 Mbps):

```bash
scrcpy --bit-rate 2M
scrcpy -b 2M  # versão reduzida
```

#### Limitar frame rates

Em dispositivos com Android >= 10, a captura de frame rate pode ser limitada:

```bash
scrcpy --max-fps 15
```

#### Cortar

A tela do dispositivo pode ser cortada para espelhar apenas uma parte da tela.

Isso é útil por exemplo, ao espelhar apenas um olho do Oculus Go:

```bash
scrcpy --crop 1224:1440:0:0   # 1224x1440 no deslocamento (0,0)
```

Se `--max-size` também for especificado, redimensionar é aplicado após os cortes.


### Gravando

É possível gravar a tela enquanto ocorre o espelhamento:

```bash
scrcpy --record file.mp4
scrcpy -r file.mkv
```

Para desativar o espelhamento durante a gravação:

```bash
scrcpy --no-display --record file.mp4
scrcpy -Nr file.mkv
# interrompe a gravação com Ctrl+C
# Ctrl+C não encerrar propriamente no Windows, então desconecte o dispositivo
```

"Frames pulados" são gravados, mesmo que não sejam mostrado em tempo real (por motivos de performance).
Frames tem seu _horário_ _carimbado_ no dispositivo, então [Variação de atraso nos pacotes] não impacta na gravação do arquivo.

[Variação de atraso de pacote]: https://en.wikipedia.org/wiki/Packet_delay_variation


### Conexão

#### Wireless/Sem fio

_Scrcpy_ usa `adb` para se comunicar com o dispositivo, e `adb` pode [connect] to a
device over TCP/IP:

1. Connect the device to the same Wi-Fi as your computer.
2. Get your device IP address (in Settings → About phone → Status).
3. Enable adb over TCP/IP on your device: `adb tcpip 5555`.
4. Unplug your device.
5. Connect to your device: `adb connect DEVICE_IP:5555` _(replace `DEVICE_IP`)_.
6. Run `scrcpy` as usual.

It may be useful to decrease the bit-rate and the definition:

```bash
scrcpy --bit-rate 2M --max-size 800
scrcpy -b2M -m800  # short version
```

[connect]: https://developer.android.com/studio/command-line/adb.html#wireless


#### Multi-devices

If several devices are listed in `adb devices`, you must specify the _serial_:

```bash
scrcpy --serial 0123456789abcdef
scrcpy -s 0123456789abcdef  # short version
```

If the device is connected over TCP/IP:

```bash
scrcpy --serial 192.168.0.1:5555
scrcpy -s 192.168.0.1:5555  # short version
```

You can start several instances of _scrcpy_ for several devices.

#### SSH tunnel

To connect to a remote device, it is possible to connect a local `adb` client to
a remote `adb` server (provided they use the same version of the _adb_
protocol):

```bash
adb kill-server    # kill the local adb server on 5037
ssh -CN -L5037:localhost:5037 -R27183:localhost:27183 your_remote_computer
# keep this open
```

From another terminal:

```bash
scrcpy
```

Like for wireless connections, it may be useful to reduce quality:

```
scrcpy -b2M -m800 --max-fps 15
```

### Window configuration

#### Title

By default, the window title is the device model. It can be changed:

```bash
scrcpy --window-title 'My device'
```

#### Position and size

The initial window position and size may be specified:

```bash
scrcpy --window-x 100 --window-y 100 --window-width 800 --window-height 600
```

#### Borderless

To disable window decorations:

```bash
scrcpy --window-borderless
```

#### Always on top

To keep the scrcpy window always on top:

```bash
scrcpy --always-on-top
```

#### Fullscreen

The app may be started directly in fullscreen:

```bash
scrcpy --fullscreen
scrcpy -f  # short version
```

Fullscreen can then be toggled dynamically with `Ctrl`+`f`.


### Other mirroring options

#### Read-only

To disable controls (everything which can interact with the device: input keys,
mouse events, drag&drop files):

```bash
scrcpy --no-control
scrcpy -n
```

#### Turn screen off

It is possible to turn the device screen off while mirroring on start with a
command-line option:

```bash
scrcpy --turn-screen-off
scrcpy -S
```

Or by pressing `Ctrl`+`o` at any time.

To turn it back on, press `POWER` (or `Ctrl`+`p`).

#### Render expired frames

By default, to minimize latency, _scrcpy_ always renders the last decoded frame
available, and drops any previous one.

To force the rendering of all frames (at a cost of a possible increased
latency), use:

```bash
scrcpy --render-expired-frames
```

#### Show touches

For presentations, it may be useful to show physical touches (on the physical
device).

Android provides this feature in _Developers options_.

_Scrcpy_ provides an option to enable this feature on start and disable on exit:

```bash
scrcpy --show-touches
scrcpy -t
```

Note that it only shows _physical_ touches (with the finger on the device).


### Input control

#### Rotate device screen

Press `Ctrl`+`r` to switch between portrait and landscape modes.

Note that it rotates only if the application in foreground supports the
requested orientation.

#### Copy-paste

It is possible to synchronize clipboards between the computer and the device, in
both directions:

 - `Ctrl`+`c` copies the device clipboard to the computer clipboard;
 - `Ctrl`+`Shift`+`v` copies the computer clipboard to the device clipboard;
 - `Ctrl`+`v` _pastes_ the computer clipboard as a sequence of text events (but
   breaks non-ASCII characters).

#### Text injection preference

There are two kinds of [events][textevents] generated when typing text:
 - _key events_, signaling that a key is pressed or released;
 - _text events_, signaling that a text has been entered.

By default, letters are injected using key events, so that the keyboard behaves
as expected in games (typically for WASD keys).

But this may [cause issues][prefertext]. If you encounter such a problem, you
can avoid it by:

```bash
scrcpy --prefer-text
```

(but this will break keyboard behavior in games)

[textevents]: https://blog.rom1v.com/2018/03/introducing-scrcpy/#handle-text-input
[prefertext]: https://github.com/Genymobile/scrcpy/issues/650#issuecomment-512945343


### File drop

#### Install APK

To install an APK, drag & drop an APK file (ending with `.apk`) to the _scrcpy_
window.

There is no visual feedback, a log is printed to the console.


#### Push file to device

To push a file to `/sdcard/` on the device, drag & drop a (non-APK) file to the
_scrcpy_ window.

There is no visual feedback, a log is printed to the console.

The target directory can be changed on start:

```bash
scrcpy --push-target /sdcard/foo/bar/
```


### Audio forwarding

Audio is not forwarded by _scrcpy_. Use [USBaudio] (Linux-only).

Also see [issue #14].

[USBaudio]: https://github.com/rom1v/usbaudio
[issue #14]: https://github.com/Genymobile/scrcpy/issues/14


## Shortcuts

 | Action                                 |   Shortcut                    |   Shortcut (macOS)
 | -------------------------------------- |:----------------------------- |:-----------------------------
 | Switch fullscreen mode                 | `Ctrl`+`f`                    | `Cmd`+`f`
 | Resize window to 1:1 (pixel-perfect)   | `Ctrl`+`g`                    | `Cmd`+`g`
 | Resize window to remove black borders  | `Ctrl`+`x` \| _Double-click¹_ | `Cmd`+`x`  \| _Double-click¹_
 | Click on `HOME`                        | `Ctrl`+`h` \| _Middle-click_  | `Ctrl`+`h` \| _Middle-click_
 | Click on `BACK`                        | `Ctrl`+`b` \| _Right-click²_  | `Cmd`+`b`  \| _Right-click²_
 | Click on `APP_SWITCH`                  | `Ctrl`+`s`                    | `Cmd`+`s`
 | Click on `MENU`                        | `Ctrl`+`m`                    | `Ctrl`+`m`
 | Click on `VOLUME_UP`                   | `Ctrl`+`↑` _(up)_             | `Cmd`+`↑` _(up)_
 | Click on `VOLUME_DOWN`                 | `Ctrl`+`↓` _(down)_           | `Cmd`+`↓` _(down)_
 | Click on `POWER`                       | `Ctrl`+`p`                    | `Cmd`+`p`
 | Power on                               | _Right-click²_                | _Right-click²_
 | Turn device screen off (keep mirroring)| `Ctrl`+`o`                    | `Cmd`+`o`
 | Rotate device screen                   | `Ctrl`+`r`                    | `Cmd`+`r`
 | Expand notification panel              | `Ctrl`+`n`                    | `Cmd`+`n`
 | Collapse notification panel            | `Ctrl`+`Shift`+`n`            | `Cmd`+`Shift`+`n`
 | Copy device clipboard to computer      | `Ctrl`+`c`                    | `Cmd`+`c`
 | Paste computer clipboard to device     | `Ctrl`+`v`                    | `Cmd`+`v`
 | Copy computer clipboard to device      | `Ctrl`+`Shift`+`v`            | `Cmd`+`Shift`+`v`
 | Enable/disable FPS counter (on stdout) | `Ctrl`+`i`                    | `Cmd`+`i`

_¹Double-click on black borders to remove them._  
_²Right-click turns the screen on if it was off, presses BACK otherwise._


## Custom paths

To use a specific _adb_ binary, configure its path in the environment variable
`ADB`:

    ADB=/path/to/adb scrcpy

To override the path of the `scrcpy-server` file, configure its path in
`SCRCPY_SERVER_PATH`.

[useful]: https://github.com/Genymobile/scrcpy/issues/278#issuecomment-429330345


## Why _scrcpy_?

A colleague challenged me to find a name as unpronounceable as [gnirehtet].

[`strcpy`] copies a **str**ing; `scrcpy` copies a **scr**een.

[gnirehtet]: https://github.com/Genymobile/gnirehtet
[`strcpy`]: http://man7.org/linux/man-pages/man3/strcpy.3.html


## How to build?

See [BUILD].

[BUILD]: BUILD.md


## Common issues

See the [FAQ](FAQ.md).


## Developers

Read the [developers page].

[developers page]: DEVELOP.md


## Licence

    Copyright (C) 2018 Genymobile
    Copyright (C) 2018-2020 Romain Vimont

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.

## Articles

- [Introducing scrcpy][article-intro]
- [Scrcpy now works wirelessly][article-tcpip]

[article-intro]: https://blog.rom1v.com/2018/03/introducing-scrcpy/
[article-tcpip]: https://www.genymotion.com/blog/open-source-project-scrcpy-now-works-wirelessly/
