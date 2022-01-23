Solo se garantiza que el archivo [README](README.md) original esté actualizado.

# scrcpy (v1.21)

<img src="data/icon.svg" width="128" height="128" alt="scrcpy" align="right" />

Esta aplicación proporciona control e imagen de un dispositivo Android conectado
por USB (o [por TCP/IP](#conexión)). No requiere acceso _root_.
Compatible con _GNU/Linux_, _Windows_ y _macOS_.

![screenshot](assets/screenshot-debian-600.jpg)

Se enfoca en:
 - **ser ligera**: aplicación nativa, solo muestra la imagen del dispositivo
 - **rendimiento**: 30~120fps, dependiendo del dispositivo
 - **calidad**: 1920×1080 o superior
 - **baja latencia**: [35~70ms][lowlatency]
 - **inicio rápido**: ~1 segundo para mostrar la primera imagen
 - **no intrusivo**: no deja nada instalado en el dispositivo
 - **beneficios**: sin cuentas, sin anuncios, no requiere acceso a internet
 - **libertad**: software gratis y de código abierto

[lowlatency]: https://github.com/Genymobile/scrcpy/pull/646

Con la aplicación puede:
 - [grabar la pantalla](#capturas-y-grabaciones)
 - duplicar la imagen con [la pantalla apagada](#apagar-la-pantalla)
 - [copiar y pegar](#copiar-y-pegar) en ambos sentidos
 - [configurar la calidad](#configuración-de-captura)
 - usar la pantalla del dispositivo [como webcam (V4L2)](#v4l2loopback) (solo en Linux)
 - [emular un teclado físico (HID)](#emular-teclado-físico-hid)
   (solo en Linux)
 - y mucho más…

## Requisitos

El dispositivo Android requiere como mínimo API 21 (Android 5.0).

Asegurate de [habilitar el adb debugging][enable-adb] en tu(s) dispositivo(s).

[enable-adb]: https://developer.android.com/studio/command-line/adb.html#Enabling

En algunos dispositivos, también necesitas habilitar [una opción adicional][control] para controlarlo con el teclado y ratón.

[control]: https://github.com/Genymobile/scrcpy/issues/70#issuecomment-373286323


## Consigue la app

<a href="https://repology.org/project/scrcpy/versions"><img src="https://repology.org/badge/vertical-allrepos/scrcpy.svg" alt="Packaging status" align="right"></a>

### Resumen

 - Linux: `apt install scrcpy`
 - Windows: [download](README.md#windows)
 - macOS: `brew install scrcpy`

Construir desde la fuente: [BUILD] ([proceso simplificado][BUILD_simple])

[BUILD]: BUILD.md
[BUILD_simple]: BUILD.md#simple


### Linux

En Debian y Ubuntu:

```
apt install scrcpy
```

Hay un paquete [Snap]: [`scrcpy`][snap-link].

[snap-link]: https://snapstats.org/snaps/scrcpy

[snap]: https://en.wikipedia.org/wiki/Snappy_(package_manager)

Para Fedora, hay un paquete [COPR]: [`scrcpy`][copr-link].

[COPR]: https://fedoraproject.org/wiki/Category:Copr
[copr-link]: https://copr.fedorainfracloud.org/coprs/zeno/scrcpy/

Para Arch Linux, hay un paquete [AUR]: [`scrcpy`][aur-link].

[AUR]: https://wiki.archlinux.org/index.php/Arch_User_Repository
[aur-link]: https://aur.archlinux.org/packages/scrcpy/

Para Gentoo, hay un paquete [Ebuild]: [`scrcpy/`][ebuild-link].

[Ebuild]: https://wiki.gentoo.org/wiki/Ebuild
[ebuild-link]: https://github.com/maggu2810/maggu2810-overlay/tree/master/app-mobilephone/scrcpy

También puedes [construir la aplicación manualmente][BUILD] ([proceso simplificado][BUILD_simple]).


### Windows

Para Windows, por simplicidad, hay un pre-compilado con todas las dependencias
(incluyendo `adb`):

 - [README](README.md#windows)

También está disponible en [Chocolatey]:

[Chocolatey]: https://chocolatey.org/

```bash
choco install scrcpy
choco install adb    # si aún no está instalado
```

Y en [Scoop]:

```bash
scoop install scrcpy
scoop install adb    # si aún no está instalado
```

[Scoop]: https://scoop.sh

También puedes [construir la aplicación manualmente][BUILD].


### macOS

La aplicación está disponible en [Homebrew]. Solo instalala:

[Homebrew]: https://brew.sh/

```bash
brew install scrcpy
```

Necesitarás `adb`, accesible desde `PATH`. Si aún no lo tienes:

```bash
brew install android-platform-tools
```

También está disponible en [MacPorts], que configura el adb automáticamente:

```bash
sudo port install scrcpy
```

[MacPorts]: https://www.macports.org/


También puedes [construir la aplicación manualmente][BUILD].


## Ejecutar

Enchufa el dispositivo Android, y ejecuta:

```bash
scrcpy
```

Acepta argumentos desde la línea de comandos, listados en:

```bash
scrcpy --help
```

## Características

### Configuración de captura

#### Reducir la definición

A veces es útil reducir la definición de la imagen del dispositivo Android para aumentar el desempeño.

Para limitar el ancho y la altura a un valor específico (ej. 1024):

```bash
scrcpy --max-size 1024
scrcpy -m 1024  # versión breve
```

La otra dimensión es calculada para conservar el aspect ratio del dispositivo.
De esta forma, un dispositivo en 1920×1080 será transmitido a 1024×576.


#### Cambiar el bit-rate

El bit-rate por defecto es 8 Mbps. Para cambiar el bit-rate del video (ej. a 2 Mbps):

```bash
scrcpy --bit-rate 2M
scrcpy -b 2M  # versión breve
```

#### Limitar los fps

El fps puede ser limitado:

```bash
scrcpy --max-fps 15
```

Es oficialmente soportado desde Android 10, pero puede funcionar en versiones anteriores.

#### Recortar

La imagen del dispositivo puede ser recortada para transmitir solo una parte de la pantalla.

Por ejemplo, puede ser útil para transmitir la imagen de un solo ojo del Oculus Go:

```bash
scrcpy --crop 1224:1440:0:0   # 1224x1440 con coordenadas de origen en (0,0)
```

Si `--max-size` también está especificado, el cambio de tamaño es aplicado después de cortar.


#### Fijar la rotación del video


Para fijar la rotación de la transmisión:

```bash
scrcpy --lock-video-orientation     # orientación inicial
scrcpy --lock-video-orientation=0   # orientación normal
scrcpy --lock-video-orientation=1   # 90° contrarreloj
scrcpy --lock-video-orientation=2   # 180°
scrcpy --lock-video-orientation=3   # 90° sentido de las agujas del reloj
```

Esto afecta la rotación de la grabación.

La [ventana también puede ser rotada](#rotación) independientemente.


#### Codificador

Algunos dispositivos pueden tener más de una rotación, y algunos pueden causar problemas o errores. Es posible seleccionar un codificador diferente:

```bash
scrcpy --encoder OMX.qcom.video.encoder.avc
```

Para listar los codificadores disponibles, puedes pasar un nombre de codificador inválido, el error te dará los codificadores disponibles:

```bash
scrcpy --encoder _
```

### Capturas y grabaciones


#### Grabación

Es posible grabar la pantalla mientras se transmite:

```bash
scrcpy --record file.mp4
scrcpy -r file.mkv
```

Para grabar sin transmitir la pantalla:

```bash
scrcpy --no-display --record file.mp4
scrcpy -Nr file.mkv
# interrumpe la grabación con Ctrl+C
```

Los "skipped frames" son grabados, incluso si no se mostrados en tiempo real (por razones de desempeño). Los frames tienen _marcas de tiempo_ en el dispositivo, por lo que el "[packet delay
variation]" no impacta el archivo grabado.

[packet delay variation]: https://en.wikipedia.org/wiki/Packet_delay_variation


#### v4l2loopback

En Linux se puede mandar el stream del video a un dispositivo loopback v4l2, por
lo que se puede abrir el dispositivo Android como una webcam con cualquier
programa compatible con v4l2.

Se debe instalar el modulo `v4l2loopback`:

```bash
sudo apt install v4l2loopback-dkms
```

Para crear un dispositivo v4l2:

```bash
sudo modprobe v4l2loopback
```

Esto va a crear un nuevo dispositivo de video en `/dev/videoN`, donde `N` es un número
(hay más [opciones](https://github.com/umlaeute/v4l2loopback#options) disponibles
para crear múltiples dispositivos o usar un ID en específico).

Para ver los dispositivos disponibles:

```bash
# requiere el paquete v4l-utils
v4l2-ctl --list-devices
# simple pero generalmente suficiente
ls /dev/video*
```

Para iniciar scrcpy usando una fuente v4l2:

```bash
scrcpy --v4l2-sink=/dev/videoN
scrcpy --v4l2-sink=/dev/videoN --no-display  # deshabilita la transmisión de imagen
scrcpy --v4l2-sink=/dev/videoN -N            # más corto
```

(reemplace `N` con el ID del dispositivo, compruebe con `ls /dev/video*`)

Una vez habilitado, podés abrir el stream del video con una herramienta compatible con v4l2:

```bash
ffplay -i /dev/videoN
vlc v4l2:///dev/videoN   # VLC puede agregar un delay por buffering
```

Por ejemplo, podrías capturar el video usando [OBS].

[OBS]: https://obsproject.com/


#### Buffering

Es posible agregar buffering al video. Esto reduce el ruido en la imagen ("jitter")
pero aumenta la latencia (vea [#2464]).

[#2464]: https://github.com/Genymobile/scrcpy/issues/2464

La opción de buffering está disponible para la transmisión de imagen:

```bash
scrcpy --display-buffer=50  # agrega 50 ms de buffering a la imagen
```

y las fuentes V4L2:

```bash
scrcpy --v4l2-buffer=500    # agrega 500 ms de buffering a la fuente v4l2
```


### Conexión

#### TCP/IP (Inalámbrica)

_Scrcpy_ usa `adb` para comunicarse con el dispositivo, y `adb` puede [conectarse] vía TCP/IP.
El dispositivo debe estar conectado a la misma red que la computadora:

##### Automático

La opción `--tcpip` permite configurar la conexión automáticamente. Hay 2 variables.

Si el dispositivo (accesible en 192.168.1.1 para este ejemplo) ya está escuchando
en un puerto (generalmente 5555) esperando una conexión adb entrante, entonces corré:

```bash
scrcpy --tcpip=192.168.1.1       # el puerto default es 5555
scrcpy --tcpip=192.168.1.1:5555
```

Si el dispositivo no tiene habilitado el modo adb TCP/IP (o si no sabés la dirección IP),
entonces conectá el dispositivo por USB y corré:

```bash
scrcpy --tcpip    # sin argumentos
```

El programa buscará automáticamente la IP del dispositivo, habilitará el modo TCP/IP, y
se conectará al dispositivo antes de comenzar a transmitir la imagen.

##### Manual

Como alternativa, se puede habilitar la conexión TCP/IP manualmente usando `adb`:

1. Conecta el dispositivo al mismo Wi-Fi que tu computadora.
2. Obtén la dirección IP del dispositivo, en Ajustes → Acerca del dispositivo → Estado, o ejecutando este comando:

    ```bash
    adb shell ip route | awk '{print $9}'
    ```

3. Habilita adb vía TCP/IP en el dispositivo: `adb tcpip 5555`.
4. Desenchufa el dispositivo.
5. Conéctate a tu dispositivo: `adb connect IP_DEL_DISPOSITIVO:5555` _(reemplaza `IP_DEL_DISPOSITIVO`)_.
6. Ejecuta `scrcpy` con normalidad.

Podría resultar útil reducir el bit-rate y la definición:

```bash
scrcpy --bit-rate 2M --max-size 800
scrcpy -b2M -m800  # versión breve
```

[conectarse]: https://developer.android.com/studio/command-line/adb.html#wireless


#### Múltiples dispositivos

Si hay muchos dispositivos listados en `adb devices`, será necesario especificar el _número de serie_:

```bash
scrcpy --serial 0123456789abcdef
scrcpy -s 0123456789abcdef  # versión breve
```

Si el dispositivo está conectado por TCP/IP:

```bash
scrcpy --serial 192.168.0.1:5555
scrcpy -s 192.168.0.1:5555  # versión breve
```

Puedes iniciar múltiples instancias de _scrcpy_ para múltiples dispositivos.

#### Iniciar automáticamente al detectar dispositivo

Puedes utilizar [AutoAdb]:

```bash
autoadb scrcpy -s '{}'
```

[AutoAdb]: https://github.com/rom1v/autoadb

#### Túneles

Para conectarse a un dispositivo remoto, es posible conectar un cliente local `adb` a un servidor remoto `adb` (siempre y cuando utilicen la misma versión de protocolos _adb_).

##### Servidor ADB remoto

Para conectarse a un servidor ADB remoto, haz que el servidor escuche en todas las interfaces:

```bash
adb kill-server
adb -a nodaemon server start
# conserva este servidor abierto
```

**Advertencia: todas las comunicaciones entre los clientes y el servidor ADB están desencriptadas.**

Supondremos que este servidor se puede acceder desde 192.168.1.2. Entonces, desde otra
terminal, corré scrcpy:

```bash
export ADB_SERVER_SOCKET=tcp:192.168.1.2:5037
scrcpy --tunnel-host=192.168.1.2
```

Por default, scrcpy usa el puerto local que se usó para establecer el tunel
`adb forward` (típicamente `27183`, vea `--port`). También es posible forzar un
puerto diferente (puede resultar útil en situaciones más complejas, donde haya
múltiples redirecciones):

```
scrcpy --tunnel-port=1234
```


##### Túnel SSH

Para comunicarse con un servidor ADB remoto de forma segura, es preferible usar un túnel SSH.

Primero, asegurate que el servidor ADB está corriendo en la computadora remota:

```bash
adb start-server
```

Después, establecé el túnel SSH:

```bash
# local  5038 --> remoto  5037
# local 27183 <-- remoto 27183
ssh -CN -L5038:localhost:5037 -R27183:localhost:27183 your_remote_computer
# conserva este servidor abierto
```

Desde otra terminal, corré scrcpy:

```bash
export ADB_SERVER_SOCKET=tcp:localhost:5038
scrcpy
```

Para evitar habilitar "remote port forwarding", puedes forzar una "forward connection" (nótese el argumento `-L` en vez de `-R`):

```bash
# local  5038 --> remoto  5037
# local 27183 --> remoto 27183
ssh -CN -L5038:localhost:5037 -L27183:localhost:27183 your_remote_computer
# conserva este servidor abierto
```

Desde otra terminal, corré scrcpy:

```bash
export ADB_SERVER_SOCKET=tcp:localhost:5038
scrcpy --force-adb-forward
```

Al igual que las conexiones inalámbricas, puede resultar útil reducir la calidad:

```
scrcpy -b2M -m800 --max-fps 15
```

### Configuración de la ventana

#### Título

Por defecto, el título de la ventana es el modelo del dispositivo. Puede ser modificado:

```bash
scrcpy --window-title 'My device'
```

#### Posición y tamaño

La posición y tamaño inicial de la ventana puede ser especificado:

```bash
scrcpy --window-x 100 --window-y 100 --window-width 800 --window-height 600
```

#### Sin bordes

Para deshabilitar el diseño de la ventana:

```bash
scrcpy --window-borderless
```

#### Siempre adelante

Para mantener la ventana de scrcpy siempre adelante:

```bash
scrcpy --always-on-top
```

#### Pantalla completa

La aplicación puede ser iniciada en pantalla completa:

```bash
scrcpy --fullscreen
scrcpy -f  # versión breve
```

Puede entrar y salir de la pantalla completa con la combinación <kbd>MOD</kbd>+<kbd>f</kbd>.

#### Rotación

Se puede rotar la ventana:

```bash
scrcpy --rotation 1
```

Los posibles valores son:
 - `0`: sin rotación
 - `1`: 90 grados contrarreloj
 - `2`: 180 grados
 - `3`: 90 grados en sentido de las agujas del reloj

La rotación también puede ser modificada con la combinación de teclas <kbd>MOD</kbd>+<kbd>←</kbd> _(izquierda)_ y <kbd>MOD</kbd>+<kbd>→</kbd> _(derecha)_.

Nótese que _scrcpy_ maneja 3 diferentes rotaciones:
 - <kbd>MOD</kbd>+<kbd>r</kbd> solicita al dispositivo cambiar entre vertical y horizontal (la aplicación en uso puede rechazarlo si no soporta la orientación solicitada).
 - [`--lock-video-orientation`](#fijar-la-rotación-del-video) cambia la rotación de la transmisión (la orientación del video enviado a la PC). Esto afecta a la grabación.
 - `--rotation` (o <kbd>MOD</kbd>+<kbd>←</kbd>/<kbd>MOD</kbd>+<kbd>→</kbd>) rota solo el contenido de la imagen. Esto solo afecta a la imagen mostrada, no a la grabación.


### Otras opciones

#### Solo lectura ("Read-only")

Para deshabilitar los controles (todo lo que interactúe con el dispositivo: eventos del teclado, eventos del mouse, arrastrar y soltar archivos):

```bash
scrcpy --no-control
scrcpy -n  # versión breve
```

#### Pantalla

Si múltiples pantallas están disponibles, es posible elegir cual transmitir:

```bash
scrcpy --display 1
```

Los ids de las pantallas se pueden obtener con el siguiente comando:

```bash
adb shell dumpsys display   # busque "mDisplayId=" en la respuesta
```

La segunda pantalla solo puede ser manejada si el dispositivo cuenta con Android 10 (en caso contrario será transmitida en el modo solo lectura).


#### Permanecer activo

Para evitar que el dispositivo descanse después de un tiempo mientras está conectado:

```bash
scrcpy --stay-awake
scrcpy -w  # versión breve
```

La configuración original se restaura al cerrar scrcpy.


#### Apagar la pantalla

Es posible apagar la pantalla mientras se transmite al iniciar con el siguiente comando:

```bash
scrcpy --turn-screen-off
scrcpy -S  # versión breve
```

O presionando <kbd>MOD</kbd>+<kbd>o</kbd> en cualquier momento.

Para volver a prenderla, presione <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>o</kbd>.

En Android, el botón de `POWER` siempre prende la pantalla. Por conveniencia, si `POWER` es enviado vía scrcpy (con click-derecho o <kbd>MOD</kbd>+<kbd>p</kbd>), esto forzará a apagar la pantalla con un poco de atraso (en la mejor de las situaciones). El botón físico `POWER` seguirá prendiendo la pantalla.

También puede resultar útil para evitar que el dispositivo entre en inactividad:

```bash
scrcpy --turn-screen-off --stay-awake
scrcpy -Sw  # versión breve
```


#### Apagar al cerrar la aplicación

Para apagar la pantalla del dispositivo al cerrar scrcpy:

```bash
scrcpy --power-off-on-close
```

#### Mostrar clicks

Para presentaciones, puede resultar útil mostrar los clicks físicos (en el dispositivo físicamente).

Android provee esta opción en _Opciones para desarrolladores_.

_Scrcpy_ provee una opción para habilitar esta función al iniciar la aplicación y restaurar el valor original al salir:

```bash
scrcpy --show-touches
scrcpy -t  # versión breve
```

Nótese que solo muestra los clicks _físicos_ (con el dedo en el dispositivo).


#### Desactivar protector de pantalla

Por defecto, scrcpy no evita que el protector de pantalla se active en la computadora.

Para deshabilitarlo:

```bash
scrcpy --disable-screensaver
```


### Control

#### Rotar pantalla del dispositivo

Presione <kbd>MOD</kbd>+<kbd>r</kbd> para cambiar entre posición vertical y horizontal.

Nótese que solo rotará si la aplicación activa soporta la orientación solicitada.

#### Copiar y pegar

Cuando que el portapapeles de Android cambia, automáticamente se sincroniza al portapapeles de la computadora.

Cualquier shortcut con <kbd>Ctrl</kbd> es enviado al dispositivo. En particular:
 - <kbd>Ctrl</kbd>+<kbd>c</kbd> normalmente copia
 - <kbd>Ctrl</kbd>+<kbd>x</kbd> normalmente corta
 - <kbd>Ctrl</kbd>+<kbd>v</kbd> normalmente pega (después de la sincronización de portapapeles entre la computadora y el dispositivo)

Esto normalmente funciona como es esperado.

Sin embargo, este comportamiento depende de la aplicación en uso. Por ejemplo, _Termux_ envía SIGINT con <kbd>Ctrl</kbd>+<kbd>c</kbd>, y _K-9 Mail_ crea un nuevo mensaje.

Para copiar, cortar y pegar, en tales casos (solo soportado en Android >= 7):
 - <kbd>MOD</kbd>+<kbd>c</kbd> inyecta `COPY`
 - <kbd>MOD</kbd>+<kbd>x</kbd> inyecta `CUT`
 - <kbd>MOD</kbd>+<kbd>v</kbd> inyecta `PASTE` (después de la sincronización de portapapeles entre la computadora y el dispositivo)

Además, <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd> permite inyectar el texto en el portapapeles de la computadora como una secuencia de teclas. Esto es útil cuando el componente no acepta pegado de texto (por ejemplo en _Termux_), pero puede romper caracteres no pertenecientes a ASCII.

**AVISO:** Pegar de la computadora al dispositivo (tanto con <kbd>Ctrl</kbd>+<kbd>v</kbd> o <kbd>MOD</kbd>+<kbd>v</kbd>) copia el contenido al portapapeles del dispositivo. Como consecuencia, cualquier aplicación de Android puede leer su contenido. Debería evitar pegar contenido sensible (como contraseñas) de esta forma.

Algunos dispositivos no se comportan como es esperado al establecer el portapapeles programáticamente. La opción `--legacy-paste` está disponible para cambiar el comportamiento de <kbd>Ctrl</kbd>+<kbd>v</kbd> y <kbd>MOD</kbd>+<kbd>v</kbd> para que también inyecten el texto del portapapeles de la computadora como una secuencia de teclas (de la misma forma que <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd>).

Para deshabilitar la auto-sincronización del portapapeles, use `--no-clipboard-autosync`.

#### Pellizcar para zoom

Para simular "pinch-to-zoom": <kbd>Ctrl</kbd>+_click-y-mover_.

Más precisamente, mantén <kbd>Ctrl</kbd> mientras presionas botón izquierdo. Hasta que no se suelte el botón, todos los movimientos del mouse cambiarán el tamaño y rotación del contenido (si es soportado por la app en uso) respecto al centro de la pantalla.

Concretamente, scrcpy genera clicks adicionales con un "dedo virtual" en la posición invertida respecto al centro de la pantalla.

#### Emular teclado físico (HID)

Por default, scrcpy usa el sistema de Android para la injección de teclas o texto:
funciona en todas partes, pero está limitado a ASCII.

En Linux, scrcpy puede emular un teclado USB físico en Android para proveer
una mejor experiencia al enviar _inputs_ (usando [USB HID vía AOAv2][hid-aoav2]):
deshabilita el teclado virtual y funciona para todos los caracteres y IME.

[hid-aoav2]: https://source.android.com/devices/accessories/aoa2#hid-support

Sin embargo, solo funciona si el dispositivo está conectado por USB, y por ahora
solo funciona en Linux.

Para habilitar este modo:

```bash
scrcpy --hid-keyboard
scrcpy -K  # más corto
```

Si por alguna razón falla (por ejemplo si el dispositivo no está conectado vía
USB), automáticamente vuelve al modo default (un mensaje se escribirá en la consola).
Se puede usar los mismos argumentos en la línea de comandos tanto si se conecta con
USB o vía TCP/IP.

En este modo, los _raw key events_ (_scancodes_) se envían al dispositivo, independientemente
del mapeo del teclado en el host. Por eso, si el diseño de tu teclado no concuerda, debe ser
configurado en el dispositivo Android, en Ajustes → Sistema → Idioma y Entrada de Texto
→ [Teclado Físico].

Se puede iniciar automáticamente en esta página de ajustes:

```bash
adb shell am start -a android.settings.HARD_KEYBOARD_SETTINGS
```

Sin embargo, la opción solo está disponible cuando el teclado HID está activo
(o cuando se conecta un teclado físico).

[Teclado Físico]: https://github.com/Genymobile/scrcpy/pull/2632#issuecomment-923756915


#### Preferencias de inyección de texto

Existen dos tipos de [eventos][textevents] generados al escribir texto:
 - _key events_, marcando si la tecla es presionada o soltada;
 - _text events_, marcando si un texto fue introducido.

Por defecto, las letras son inyectadas usando _key events_, para que el teclado funcione como es esperado en juegos (típicamente las teclas WASD).

Pero esto puede [causar problemas][prefertext]. Si encuentras tales problemas, los puedes evitar con:

```bash
scrcpy --prefer-text
```

(Pero esto romperá el comportamiento del teclado en los juegos)

Por el contrario, se puede forzar scrcpy para siempre injectar _raw key events_:

```bash
scrcpy --raw-key-events
```

Estas opciones no tienen efecto en los teclados HID (todos los _key events_ son enviados como
_scancodes_ en este modo).

[textevents]: https://blog.rom1v.com/2018/03/introducing-scrcpy/#handle-text-input
[prefertext]: https://github.com/Genymobile/scrcpy/issues/650#issuecomment-512945343


#### Repetir tecla

Por defecto, mantener una tecla presionada genera múltiples _key events_. Esto puede
causar problemas de desempeño en algunos juegos, donde estos eventos no tienen sentido de todos modos.

Para evitar enviar _key events_ repetidos:

```bash
scrcpy --no-key-repeat
```

Estas opciones no tienen efecto en los teclados HID (Android maneja directamente
las repeticiones de teclas en este modo)


#### Botón derecho y botón del medio

Por defecto, botón derecho ejecuta RETROCEDER (o ENCENDIDO) y botón del medio INICIO. Para inhabilitar estos atajos y enviar los clicks al dispositivo:

```bash
scrcpy --forward-all-clicks
```


### Arrastrar y soltar archivos

#### Instalar APKs

Para instalar un APK, arrastre y suelte el archivo APK (terminado en `.apk`) a la ventana de _scrcpy_.

No hay respuesta visual, un mensaje se escribirá en la consola.


#### Enviar archivos al dispositivo

Para enviar un archivo a `/sdcard/Download/` en el dispositivo, arrastre y suelte
un archivo (no APK) a la ventana de _scrcpy_.

No hay ninguna respuesta visual, un mensaje se escribirá en la consola.

El directorio de destino puede ser modificado al iniciar:

```bash
scrcpy --push-target=/sdcard/Movies/
```


### Envío de Audio

_Scrcpy_ no envía el audio. Use [sndcpy].

También lea [issue #14].

[sndcpy]: https://github.com/rom1v/sndcpy
[issue #14]: https://github.com/Genymobile/scrcpy/issues/14


## Atajos

En la siguiente lista, <kbd>MOD</kbd> es el atajo modificador. Por defecto es <kbd>Alt</kbd> (izquierdo) o <kbd>Super</kbd> (izquierdo).

Se puede modificar usando `--shortcut-mod`. Las posibles teclas son `lctrl` (izquierdo), `rctrl` (derecho), `lalt` (izquierdo), `ralt` (derecho), `lsuper` (izquierdo) y `rsuper` (derecho). Por ejemplo:

```bash
# use RCtrl para los atajos
scrcpy --shortcut-mod=rctrl

# use tanto LCtrl+LAlt o LSuper para los atajos
scrcpy --shortcut-mod=lctrl+lalt,lsuper
```

_<kbd>[Super]</kbd> es generalmente la tecla <kbd>Windows</kbd> o <kbd>Cmd</kbd>._

[Super]: https://en.wikipedia.org/wiki/Super_key_(keyboard_button)

 | Acción                                       |   Atajo
 | -------------------------------------------  |:-----------------------------
 | Alterne entre pantalla compelta              | <kbd>MOD</kbd>+<kbd>f</kbd>
 | Rotar pantalla hacia la izquierda            | <kbd>MOD</kbd>+<kbd>←</kbd> _(izquierda)_
 | Rotar pantalla hacia la derecha              | <kbd>MOD</kbd>+<kbd>→</kbd> _(derecha)_
 | Ajustar ventana a 1:1 ("pixel-perfect")      | <kbd>MOD</kbd>+<kbd>g</kbd>
 | Ajustar ventana para quitar los bordes negros| <kbd>MOD</kbd>+<kbd>w</kbd> \| _Doble click izquierdo¹_
 | Click en `INICIO`                            | <kbd>MOD</kbd>+<kbd>h</kbd> \| _Click medio_
 | Click en `RETROCEDER`                        | <kbd>MOD</kbd>+<kbd>b</kbd> \| _Click derecho²_
 | Click en `CAMBIAR APLICACIÓN`                | <kbd>MOD</kbd>+<kbd>s</kbd> \| _Cuarto botón³_
 | Click en `MENÚ` (desbloquear pantalla)⁴      | <kbd>MOD</kbd>+<kbd>m</kbd>
 | Click en `SUBIR VOLUMEN`                     | <kbd>MOD</kbd>+<kbd>↑</kbd> _(arriba)_
 | Click en `BAJAR VOLUME`                      | <kbd>MOD</kbd>+<kbd>↓</kbd> _(abajo)_
 | Click en `ENCENDIDO`                         | <kbd>MOD</kbd>+<kbd>p</kbd>
 | Encendido                                    | _Botón derecho²_
 | Apagar pantalla (manteniendo la transmisión) | <kbd>MOD</kbd>+<kbd>o</kbd>
 | Encender pantalla                            | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>o</kbd>
 | Rotar pantalla del dispositivo               | <kbd>MOD</kbd>+<kbd>r</kbd>
 | Abrir panel de notificaciones                | <kbd>MOD</kbd>+<kbd>n</kbd> \| _Quinto botón³_
 | Abrir panel de configuración                 | <kbd>MOD</kbd>+<kbd>n</kbd>+<kbd>n</kbd> \| _Doble quinto botón³_
 | Cerrar paneles                               | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>n</kbd>
 | Copiar al portapapeles⁵                      | <kbd>MOD</kbd>+<kbd>c</kbd>
 | Cortar al portapapeles⁵                      | <kbd>MOD</kbd>+<kbd>x</kbd>
 | Synchronizar portapapeles y pegar⁵           | <kbd>MOD</kbd>+<kbd>v</kbd>
 | Inyectar texto del portapapeles de la PC     | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd>
 | Habilitar/Deshabilitar contador de FPS (en stdout)      | <kbd>MOD</kbd>+<kbd>i</kbd>
 | Pellizcar para zoom                          | <kbd>Ctrl</kbd>+_click-y-mover_
 | Arrastrar y soltar un archivo (APK)          | Instalar APK desde la computadora
 | Arrastrar y soltar un archivo (no APK)       | [Mover archivo al dispositivo](#enviar-archivos-al-dispositivo)

_¹Doble click en los bordes negros para eliminarlos._  
_²Botón derecho enciende la pantalla si estaba apagada, sino ejecuta RETROCEDER._  
_³Cuarto y quinto botón del mouse, si tu mouse los tiene._  
_⁴Para las apps react-native en desarrollo, `MENU` activa el menú de desarrollo._  
_⁵Solo en Android >= 7._

Los shortcuts con teclas repetidas se ejecutan soltando y volviendo a apretar la tecla
por segunda vez. Por ejemplo, para ejecutar "Abrir panel de configuración":

 1. Apretá y mantené apretado <kbd>MOD</kbd>.
 2. Después apretá dos veces la tecla <kbd>n</kbd>.
 3. Por último, soltá la tecla <kbd>MOD</kbd>.

Todos los atajos <kbd>Ctrl</kbd>+_tecla_ son enviados al dispositivo para que sean manejados por la aplicación activa.


## Path personalizado

Para usar un binario de _adb_ en particular, configure el path `ADB` en las variables de entorno:

```bash
ADB=/path/to/adb scrcpy
```

Para sobreescribir el path del archivo `scrcpy-server`, configure el path en `SCRCPY_SERVER_PATH`.

Para sobreescribir el ícono, configure el path en `SCRCPY_ICON_PATH`.


## ¿Por qué _scrcpy_?

Un colega me retó a encontrar un nombre tan impronunciable como [gnirehtet].

[`strcpy`] copia un **str**ing; `scrcpy` copia un **scr**een.

[gnirehtet]: https://github.com/Genymobile/gnirehtet
[`strcpy`]: http://man7.org/linux/man-pages/man3/strcpy.3.html


## ¿Cómo construir (BUILD)?

Véase [BUILD] (en inglés).


## Problemas generales

Vea las [preguntas frecuentes (en inglés)](FAQ.md).


## Desarrolladores

Lea la [hoja de desarrolladores (en inglés)](DEVELOP.md).


## Licencia

    Copyright (C) 2018 Genymobile
    Copyright (C) 2018-2021 Romain Vimont

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.

## Artículos

- [Introducing scrcpy][article-intro] (en inglés)
- [Scrcpy now works wirelessly][article-tcpip] (en inglés)

[article-intro]: https://blog.rom1v.com/2018/03/introducing-scrcpy/
[article-tcpip]: https://www.genymotion.com/blog/open-source-project-scrcpy-now-works-wirelessly/
