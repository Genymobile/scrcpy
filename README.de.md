_Only the original [README](README.md) is guaranteed to be up-to-date._

# scrcpy (v1.22)

<img src="data/icon.svg" width="128" height="128" alt="scrcpy" align="right" />

_ausgesprochen "**scr**een **c**o**py**"_

Diese Anwendung liefert sowohl Anzeige als auch Steuerung eines Android-Gerätes über USB (oder [über TCP/IP](#tcpip-kabellos)). Dabei wird kein _root_ Zugriff benötigt.
Die Anwendung funktioniert unter _GNU/Linux_, _Windows_ und _macOS_.

![screenshot](assets/screenshot-debian-600.jpg)

Dabei liegt der Fokus auf:

 - **Leichtigkeit**: native, nur Anzeige des Gerätedisplays
 - **Leistung**: 30~120fps, abhängig vom Gerät
 - **Qualität**: 1920×1080 oder mehr
 - **Geringe Latenz**: [35~70ms][lowlatency]
 - **Kurze Startzeit**: ~1 Sekunde um das erste Bild anzuzeigen
 - **Keine Aufdringlichkeit**: Es wird keine installierte Software auf dem Gerät zurückgelassen
 - **Nutzervorteile**: kein Account, keine Werbung, kein Internetzugriff notwendig
 - **Freiheit**: gratis und open-source

[lowlatency]: https://github.com/Genymobile/scrcpy/pull/646

Die Features beinhalten:
 - [Aufnahme](#Aufnahme)
 - Spiegeln mit [ausgeschaltetem Bildschirm](#bildschirm-ausschalten)
 - [Copy&Paste](#copy-paste) in beide Richtungen
 - [Einstellbare Qualität](#Aufnahmekonfiguration)
 - Gerätebildschirm [als Webcam (V4L2)](#v4l2loopback) (nur Linux)
 - [Simulation einer physischen Tastatur (HID)](#simulation-einer-physischen-tastatur-mit-hid)
   (nur Linux)
 - [Simulation einer physischen Maus (HID)](#simulation-einer-physischen-maus-mit-hid)
   (nur Linux)
 - [OTG Modus](#otg) (nur Linux)
 - und mehr…

## Voraussetzungen

Das Android-Gerät benötigt mindestens API 21 (Android 5.0).

Es muss sichergestellt sein, dass [adb debugging][enable-adb] auf dem Gerät aktiv ist.

[enable-adb]: https://developer.android.com/studio/command-line/adb.html#Enabling

Auf manchen Geräten müssen zudem [weitere Optionen][control] aktiv sein um das Gerät mit Maus und Tastatur steuern zu können.

[control]: https://github.com/Genymobile/scrcpy/issues/70#issuecomment-373286323


## Installation der App

<a href="https://repology.org/project/scrcpy/versions"><img src="https://repology.org/badge/vertical-allrepos/scrcpy.svg" alt="Packaging status" align="right"></a>

### Zusammenfassung

 - Linux: `apt install scrcpy`
 - Windows: [download (siehe README)](README.md#windows)
 - macOS: `brew install scrcpy`

Direkt von der Source bauen: [BUILD] ([vereinfachter Prozess (englisch)][BUILD_simple])

[BUILD]: BUILD.md
[BUILD_simple]: BUILD.md#simple


### Linux

Auf Debian und Ubuntu:

```
apt install scrcpy
```

Auf Arch Linux:

```
pacman -S scrcpy
```

Ein [Snap] package ist verfügbar: [`scrcpy`][snap-link].

[snap-link]: https://snapstats.org/snaps/scrcpy

[snap]: https://en.wikipedia.org/wiki/Snappy_(package_manager)

Für Fedora ist ein [COPR] package verfügbar: [`scrcpy`][copr-link].

[COPR]: https://fedoraproject.org/wiki/Category:Copr
[copr-link]: https://copr.fedorainfracloud.org/coprs/zeno/scrcpy/


Für Gentoo ist ein [Ebuild] verfügbar: [`scrcpy/`][ebuild-link].

[Ebuild]: https://wiki.gentoo.org/wiki/Ebuild
[ebuild-link]: https://github.com/maggu2810/maggu2810-overlay/tree/master/app-mobilephone/scrcpy

Die App kann zudem [manuell gebaut werden][BUILD] ([vereinfachter Prozess (englisch)][BUILD_simple]).


### Windows

Für Windows ist der Einfachheit halber ein vorgebautes Archiv mit allen Abhängigkeiten (inklusive `adb`) vorhanden.

 - [README](README.md#windows)

Es ist zudem in [Chocolatey] vorhanden:

[Chocolatey]: https://chocolatey.org/

```bash
choco install scrcpy
choco install adb    # falls noch nicht vorhanden
```

Und in [Scoop]:

```bash
scoop install scrcpy
scoop install adb    # falls noch nicht vorhanden
```

[Scoop]: https://scoop.sh

Die App kann zudem [manuell gebaut werden][BUILD].


### macOS

Die Anwendung ist in [Homebrew] verfügbar. Installation:

[Homebrew]: https://brew.sh/

```bash
brew install scrcpy
```

Es wird `adb` benötigt, auf welches über `PATH` zugegriffen werden kann. Falls noch nicht vorhanden:

```bash
brew install android-platform-tools
```

Es ist außerdem in [MacPorts] vorhanden, welches adb bereits aufsetzt:

```bash
sudo port install scrcpy
```

[MacPorts]: https://www.macports.org/


Die Anwendung kann zudem [manuell gebaut werden][BUILD].


## Ausführen

Ein Android-Gerät anschließen und diese Befehle ausführen:

```bash
scrcpy
```

Dabei werden Kommandozeilenargumente akzeptiert, aufgelistet per:

```bash
scrcpy --help
```

## Funktionalitäten

### Aufnahmekonfiguration

#### Größe reduzieren

Manchmal ist es sinnvoll, das Android-Gerät mit einer geringeren Auflösung zu spiegeln, um die Leistung zu erhöhen.

Um die Höhe und Breite auf einen Wert zu limitieren (z.B. 1024):

```bash
scrcpy --max-size 1024
scrcpy -m 1024  # short version
```

Die andere Größe wird dabei so berechnet, dass das Seitenverhältnis des Gerätes erhalten bleibt.
In diesem Fall wird ein Gerät mit einer 1920×1080-Auflösung mit 1024×576 gespiegelt.


#### Ändern der Bit-Rate

Die Standard-Bitrate ist 8 Mbps. Um die Bitrate zu ändern (z.B. zu 2 Mbps):

```bash
scrcpy --bit-rate 2M
scrcpy -b 2M  # Kurzversion
```

#### Limitieren der Bildwiederholrate

Die Aufnahme-Bildwiederholrate kann begrenzt werden:

```bash
scrcpy --max-fps 15
```

Dies wird offiziell seit Android 10 unterstützt, kann jedoch bereits auf früheren Versionen funktionieren.

#### Zuschneiden

Der Geräte-Bildschirm kann zugeschnitten werden, sodass nur ein Teil gespiegelt wird.

Dies ist beispielsweise nützlich, um nur ein Auge der Oculus Go zu spiegeln:

```bash
scrcpy --crop 1224:1440:0:0   # 1224x1440 am Versatz (0,0)
```

Falls `--max-size` auch festgelegt ist, wird das Ändern der Größe nach dem Zuschneiden angewandt.


#### Feststellen der Videoorientierung


Um die Orientierung während dem Spiegeln festzustellen:

```bash
scrcpy --lock-video-orientation     # ursprüngliche (momentane) Orientierung
scrcpy --lock-video-orientation=0   # normale Orientierung
scrcpy --lock-video-orientation=1   # 90° gegen den Uhrzeigersinn
scrcpy --lock-video-orientation=2   # 180°
scrcpy --lock-video-orientation=3   # 90° mit dem Uhrzeigersinn
```

Dies beeinflusst die Aufnahmeausrichtung.

Das [Fenster kann auch unabhängig rotiert](#Rotation) werden.


#### Encoder

Manche Geräte besitzen mehr als einen Encoder. Manche dieser Encoder können dabei sogar zu Problemen oder Abstürzen führen.
Die Auswahl eines anderen Encoders ist möglich:

```bash
scrcpy --encoder OMX.qcom.video.encoder.avc
```

Um eine Liste aller verfügbaren Encoder zu erhalten (eine Fehlermeldung gibt alle verfügbaren Encoder aus):

```bash
scrcpy --encoder _
```

### Aufnahme

#### Aufnehmen von Videos

Es ist möglich, das Display während des Spiegelns aufzunehmen:

```bash
scrcpy --record file.mp4
scrcpy -r file.mkv
```

Um das Spiegeln während des Aufnehmens zu deaktivieren:

```bash
scrcpy --no-display --record file.mp4
scrcpy -Nr file.mkv
# Unterbrechen der Aufnahme mit Strg+C
```

"Übersprungene Bilder" werden aufgenommen, selbst wenn sie in Echtzeit (aufgrund von Performancegründen) nicht dargestellt werden. Die Einzelbilder sind mit _Zeitstempeln_ des Gerätes versehen are, sodass eine [Paketverzögerungsvariation] nicht die Aufnahmedatei beeinträchtigt.

[Paketverzögerungsvariation]: https://www.wikide.wiki/wiki/en/Packet_delay_variation


#### v4l2loopback

Auf Linux ist es möglich, den Video-Stream zu einem v4l2 loopback Gerät zu senden, sodass das Android-Gerät von jedem v4l2-fähigen Tool wie eine Webcam verwendet werden kann.

Das Modul `v4l2loopback` muss dazu installiert werden:

```bash
sudo apt install v4l2loopback-dkms
```

Um ein v4l2 Gerät zu erzeugen:

```bash
sudo modprobe v4l2loopback
```

Dies erzeugt ein neues Video-Gerät in `/dev/videoN`, wobei `N` ein Integer ist (mehr [Optionen](https://github.com/umlaeute/v4l2loopback#options) sind verfügbar um mehrere Geräte oder Geräte mit spezifischen Nummern zu erzeugen).

Um die aktivierten Geräte aufzulisten:

```bash
# benötigt das v4l-utils package
v4l2-ctl --list-devices

# simpel, kann aber ausreichend
ls /dev/video*
```

Um scrcpy mithilfe eines v4l2 sink zu starten:

```bash
scrcpy --v4l2-sink=/dev/videoN
scrcpy --v4l2-sink=/dev/videoN --no-display  # Fenster mit Spiegelung ausschalten
scrcpy --v4l2-sink=/dev/videoN -N            # kurze Version
```

(`N` muss mit der Geräte-ID ersetzt werden, welche mit `ls /dev/video*` überprüft werden kann)

Einmal aktiv, kann der Stream mit einem v4l2-fähigen Tool verwendet werden:

```bash
ffplay -i /dev/videoN
vlc v4l2:///dev/videoN   # VLC kann eine gewisse  Bufferverzögerung herbeiführen
```

Beispielsweise kann das Video mithilfe von [OBS] aufgenommen werden.

[OBS]: https://obsproject.com/


#### Buffering

Es ist möglich, Buffering hinzuzufügen. Dies erhöht die Latenz, reduziert aber etwaigen Jitter (see [#2464]).

[#2464]: https://github.com/Genymobile/scrcpy/issues/2464

Diese Option ist sowohl für Video-Buffering:

```bash
scrcpy --display-buffer=50  # fügt 50ms Buffering zum Display hinzu
```

als auch V4L2 sink verfügbar:

```bash
scrcpy --v4l2-buffer=500    # fügt 500ms Buffering für v4l2 sink hinzu
```


### Verbindung

#### TCP/IP Kabellos

_Scrcpy_ verwendet `adb`, um mit dem Gerät zu kommunizieren. `adb` kann sich per TCP/IP mit einem Gerät [verbinden]. Das Gerät muss dabei mit demselben Netzwerk wie der Computer verbunden sein.

##### Automatisch

Die Option `--tcpip` erlaubt es, die Verbindung automatisch zu konfigurieren. Dabei gibt es zwei Varianten.

Falls das Gerät (verfügbar unter 192.168.1.1 in diesem Beispiel) bereit an einem Port (typically 5555) nach einkommenden adb-Verbindungen hört, dann führe diesen Befehl aus:

```bash
scrcpy --tcpip=192.168.1.1       # Standard-Port ist 5555
scrcpy --tcpip=192.168.1.1:5555
```

Falls adb TCP/IP auf dem Gerät deaktiviert ist (oder falls die IP-Adresse des Gerätes nicht bekannt ist): Gerät per USB verbinden, anschließend diesen Befehl ausführen:

```bash
scrcpy --tcpip    # ohne weitere Argumente
```

Dies finden automatisch das Gerät und aktiviert den TCP/IP-Modus. Anschließend verbindet sich der Befehl mit dem Gerät bevor die Verbindung startet.

##### Manuell

Alternativ kann die TCP/IP-Verbindung auch manuell per `adb` aktiviert werden:

1. Gerät mit demselben Wi-Fi wie den Computer verbinden.
2. IP-Adresse des Gerätes herausfinden, entweder über Einstellungen → Über das Telefon → Status, oder indem dieser Befehl ausgeführt wird:

    ```bash
    adb shell ip route | awk '{print $9}'
    ```

3. Aktivieren von adb über TCP/IP auf dem Gerät: `adb tcpip 5555`.
4. Ausstecken des Geräts.
5. Verbinden zum Gerät: `adb connect DEVICE_IP:5555` _(`DEVICE_IP` ersetzen)_.
6. `scrcpy` wie normal ausführen.

Es kann sinnvoll sein, die Bit-Rate sowie dei Auflösung zu reduzieren:

```bash
scrcpy --bit-rate 2M --max-size 800
scrcpy -b2M -m800  # kurze Version
```

[verbinden]: https://developer.android.com/studio/command-line/adb.html#wireless


#### Mehrere Geräte

Falls mehrere Geräte unter `adb devices` aufgelistet werden, muss die _Seriennummer_ angegeben werden:

```bash
scrcpy --serial 0123456789abcdef
scrcpy -s 0123456789abcdef  # kurze Version
```

Falls das Gerät über TCP/IP verbunden ist:

```bash
scrcpy --serial 192.168.0.1:5555
scrcpy -s 192.168.0.1:5555  # kurze Version
```

Es können mehrere Instanzen von _scrcpy_ für mehrere Geräte gestartet werden.

#### Autostart beim Verbinden eines Gerätes

Hierfür kann [AutoAdb] verwendet werden:

```bash
autoadb scrcpy -s '{}'
```

[AutoAdb]: https://github.com/rom1v/autoadb

#### Tunnel

Um sich zu einem entfernten Gerät zu verbinden, kann der `adb` Client mit einem remote-`adb`-Server verbunden werden (Voraussetzung: Gleiche Version des `adb`-Protokolls).

##### Remote ADB Server

Um sich zu einem Remote-`adb`-Server zu verbinden: Der Server muss auf allen Ports hören

```bash
adb kill-server
adb -a nodaemon server start
# Diesen Dialog offen halten
```

**Warnung: Die gesamte Kommunikation zwischen adb und den Geräten ist unverschlüsselt.**

Angenommen, der Server ist unter 192.168.1.2 verfügbar. Dann kann von einer anderen Kommandozeile scrcpy aufgeführt werden:

```bash
export ADB_SERVER_SOCKET=tcp:192.168.1.2:5037
scrcpy --tunnel-host=192.168.1.2
```

Standardmäßig verwendet scrcpy den lokalen Port für die Einrichtung des `adb forward`-Tunnels  (typischerweise `27183`, siehe `--port`).
Es ist zudem möglich, einen anderen Tunnel-Port zuzuweisen (sinnvoll in Situationen, bei welchen viele Weiterleitungen erfolgen):

```
scrcpy --tunnel-port=1234
```


##### SSH Tunnel

Um mit einem Remote-`adb`-Server sicher zu kommunizieren, wird ein SSH-Tunnel empfohlen.

Sicherstellen, dass der Remote-`adb`-Server läuft:

```bash
adb start-server
```

Erzeugung eines SSH-Tunnels:

```bash
# local  5038 --> remote  5037
# local 27183 <-- remote 27183
ssh -CN -L5038:localhost:5037 -R27183:localhost:27183 your_remote_computer
# Diesen Dialog geöffnet halten
```

Von einer anderen Kommandozeile aus scrcpy ausführen:

```bash
export ADB_SERVER_SOCKET=tcp:localhost:5038
scrcpy
```

Um das Aktivieren von Remote-Weiterleitung zu verhindern, kann eine Vorwärts-Verbindung verwendet werden (`-L` anstatt von `-R`):

```bash
# local  5038 --> remote  5037
# local 27183 --> remote 27183
ssh -CN -L5038:localhost:5037 -L27183:localhost:27183 your_remote_computer
# Diesen Dialog geöffnet halten
```

Von einer anderen Kommandozeile aus scrcpy ausführen:

```bash
export ADB_SERVER_SOCKET=tcp:localhost:5038
scrcpy --force-adb-forward
```


Wie für kabellose Verbindungen kann es sinnvoll sein, die Qualität zu reduzieren:

```
scrcpy -b2M -m800 --max-fps 15
```

### Fensterkonfiguration

#### Titel

Standardmäßig ist der Fenstertitel das Gerätemodell. Der Titel kann jedoch geändert werden:

```bash
scrcpy --window-title 'Mein Gerät'
```

#### Position und Größe

Die anfängliche Fensterposition und Größe können festgelegt werden:

```bash
scrcpy --window-x 100 --window-y 100 --window-width 800 --window-height 600
```

#### Rahmenlos

Um den Rahmen des Fensters zu deaktivieren:

```bash
scrcpy --window-borderless
```

#### Immer im Vordergrund

Um das Fenster immer im Vordergrund zu halten:

```bash
scrcpy --always-on-top
```

#### Vollbild

Die Anwendung kann direkt im Vollbildmodus gestartet werden:

```bash
scrcpy --fullscreen
scrcpy -f  # kurze Version
```

Das Vollbild kann dynamisch mit <kbd>MOD</kbd>+<kbd>f</kbd> gewechselt werden.

#### Rotation

Das Fenster kann rotiert werden:

```bash
scrcpy --rotation 1
```

Mögliche Werte sind:
 - `0`: keine Rotation
 - `1`: 90 grad gegen den Uhrzeigersinn
 - `2`: 180 grad
 - `3`: 90 grad mit dem Uhrzeigersinn

die Rotation kann zudem dynamisch mit <kbd>MOD</kbd>+<kbd>←</kbd>
_(links)_ and <kbd>MOD</kbd>+<kbd>→</kbd> _(rechts)_ angepasst werden.

_scrcpy_ schafft 3 verschiedene Rotationen:
 - <kbd>MOD</kbd>+<kbd>r</kbd> erfordert von Gerät den Wechsel zwischen Hochformat und Querformat (die momentane App kann dies verweigern, wenn die geforderte Ausrichtung nicht unterstützt wird).
 - [`--lock-video-orientation`](#feststellen-der-videoorientierung) ändert die Ausrichtung der Spiegelung (die Ausrichtung des an den Computer gesendeten Videos). Dies beeinflusst eventuelle Aufnahmen.
 - `--rotation` (or <kbd>MOD</kbd>+<kbd>←</kbd>/<kbd>MOD</kbd>+<kbd>→</kbd>) rotiert nur das Fenster, eventuelle Aufnahmen sind hiervon nicht beeinflusst.


### Andere Spiegel-Optionen

#### Lesezugriff

Um die Steuerung (alles, was mit dem Gerät interagieren kann: Tasten, Mausklicks, Drag-and-drop von Dateien) zu deaktivieren:

```bash
scrcpy --no-control
scrcpy -n
```

#### Anzeige

Falls mehrere Displays vorhanden sind, kann das zu spiegelnde Display gewählt werden:

```bash
scrcpy --display 1
```

Die Liste an verfügbaren Displays kann mit diesem Befehl ausgegeben werden:

```bash
adb shell dumpsys display   # Nach "mDisplayId=" in der Ausgabe suchen
```

Das zweite Display kann nur gesteuert werden, wenn das Gerät Android 10 oder höher besitzt. Ansonsten wird das Display nur mit Lesezugriff gespiegelt.


#### Wach bleiben

Um zu verhindern, dass das Gerät nach einer Weile in den Ruhezustand übergeht (solange es eingesteckt ist):

```bash
scrcpy --stay-awake
scrcpy -w
```

Der ursprüngliche Zustand wird beim Schließen von scrcpy wiederhergestellt.


#### Bildschirm ausschalten

Es ist möglich, beim Starten des Spiegelns mithilfe eines Kommandozeilenarguments den Bildschirm des Gerätes auszuschalten:

```bash
scrcpy --turn-screen-off
scrcpy -S
```

Oder durch das Drücken von <kbd>MOD</kbd>+<kbd>o</kbd> jederzeit.

Um das Display wieder einzuschalten muss <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>o</kbd> gedrückt werden.

Auf Android aktiviert der `POWER` Knopf das Display immer.
Für den Komfort wird, wenn `POWER` via scrcpy gesendet wird (über Rechtsklick oder <kbd>MOD</kbd>+<kbd>p</kbd>), wird versucht, das Display nach einer kurzen Zeit wieder auszuschalten (falls es möglich ist).
Der physische `POWER` Button aktiviert das Display jedoch immer.

Dies kann zudem nützlich sein, um das Gerät vom Ruhezustand abzuhalten:

```bash
scrcpy --turn-screen-off --stay-awake
scrcpy -Sw
```

#### Ausschalten beim Schließen

Um den Gerätebildschirm abzuschalten, wenn scrcpy geschlossen wird:

```bash
scrcpy --power-off-on-close
```


#### Anzeigen von Berührungen

Für Präsentationen kann es sinnvoll sein, die physischen Berührungen anzuzeigen (auf dem physischen Gerät).

Android stellt dieses Feature in den _Entwickleroptionen_ zur Verfügung.

_Scrcpy_ stellt die Option zur Verfügung, dies beim Start zu aktivieren und beim Schließen auf den Ursprungszustand zurückzusetzen:

```bash
scrcpy --show-touches
scrcpy -t
```

Anmerkung: Nur _physische Berührungen_ werden angezeigt (mit dem Finger auf dem Gerät).


#### Bildschirmschoner deaktivieren

Standardmäßig unterbindet scrcpy nicht den Bildschirmschoner des Computers.

Um den Bildschirmschoner zu unterbinden:

```bash
scrcpy --disable-screensaver
```


### Eingabesteuerung

#### Geräte-Bildschirm drehen

<kbd>MOD</kbd>+<kbd>r</kbd> drücken, um zwischen Hoch- und Querformat zu wechseln.

Anmerkung: Dis funktioniert nur, wenn die momentan geöffnete App beide Rotationen unterstützt.

#### Copy-paste

Immer, wenn sich die Zwischenablage von Android ändert wird dies mit dem Computer synchronisiert.

Jedes <kbd>Strg</kbd> wird an das Gerät weitergegeben. Insbesonders:
 - <kbd>Strg</kbd>+<kbd>c</kbd> kopiert typischerweise
 - <kbd>Strg</kbd>+<kbd>x</kbd> schneidet typischerweise aus
 - <kbd>Strg</kbd>+<kbd>v</kbd> fügt typischerweise ein (nach der Computer-zu-Gerät-Synchronisation)

Dies funktioniert typischerweise wie erwartet.

Die wirkliche Funktionsweise hängt jedoch von der jeweiligen Anwendung ab. Beispielhaft sendet _Termux_ SIGINT bei <kbd>Strg</kbd>+<kbd>c</kbd>, und _K-9 Mail_ erzeugt eine neue Nachricht.

Um kopieren, ausschneiden und einfügen in diesen Fällen zu verwenden (nur bei Android >= 7 unterstützt):
 - <kbd>MOD</kbd>+<kbd>c</kbd> gibt `COPY` ein
 - <kbd>MOD</kbd>+<kbd>x</kbd> gibt `CUT` ein
 - <kbd>MOD</kbd>+<kbd>v</kbd> gibt `PASTE` ein (nach der Computer-zu-Gerät-Synchronisation)

Zusätzlich erlaubt es <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd> den momentanen Inhalt der Zwischenablage als eine Serie von Tastenevents einzugeben.
Dies ist nützlich, fall die Applikation kein Einfügen unterstützt (z.B. _Termux_). Jedoch kann nicht-ASCII-Inhalt dabei zerstört werden.

**WARNUNG:** Das Einfügen der Computer-Zwischenablage in das Gerät (entweder mit <kbd>Strg</kbd>+<kbd>v</kbd> oder <kbd>MOD</kbd>+<kbd>v</kbd>) kopiert den Inhalt in die Zwischenablage des Gerätes.
Als Konsequenz kann somit jede Android-Applikation diesen Inhalt lesen. Das Einfügen von sensiblen Informationen wie Passwörtern sollte aus diesem Grund vermieden werden.

Mache Geräte verhalten sich nicht wie erwartet, wenn die Zwischenablage per Programm verändert wird.
Die Option `--legacy-paste` wird bereitgestellt, welche das Verhalten von <kbd>Strg</kbd>+<kbd>v</kbd> und <kbd>MOD</kbd>+<kbd>v</kbd> so ändert, dass die Zwischenablage wie bei <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd> als eine Serie von Tastenevents ausgeführt wird.

Um die automatische Synchronisierung der Zwischenablage zu deaktivieren:
`--no-clipboard-autosync`.

#### Ziehen zum Zoomen

Um "Ziehen-zum-Zoomen" zu simulieren: <kbd>Strg</kbd>+_klicken-und-bewegen_.

Genauer: <kbd>Strg</kbd> halten, während Linksklick gehalten wird. Solange Linksklick gehalten wird, skalieren und rotieren die Mausbewegungen den Inhalt (soweit von der jeweiligen App unterstützt).

Konkret erzeugt scrcpy einen am Mittelpunkt des Displays gespiegelten, "virtuellen" Finger.

#### Simulation einer physischen Tastatur mit HID

Standardmäßig verwendet scrcpy Android-Tasten oder Textinjektion. Dies funktioniert zwar immer, jedoch nur mit ASCII.

Auf Linux kann scrcpy mithilfe von HID eine physische Tastatur simulieren, um eine bessere Eingabeerfahrung zu gewährleisten (dies nutzt [USB HID over AOAv2][hid-aoav2]): Die virtuelle Tastatur wird deaktiviert, es funktioniert für alle Zeichen und mit IME.

[hid-aoav2]: https://source.android.com/devices/accessories/aoa2#hid-support

Dies funktioniert jedoch nur, wenn das Gerät über USB verbunden ist. Zudem wird dies momentan nur unter Linux unterstützt.

Um diesen Modus zu aktivieren:

```bash
scrcpy --hid-keyboard
scrcpy -K  # kurze Version
```

Falls dies auf gewissen Gründen fehlschlägt (z.B. Gerät ist nicht über USB verbunden), so fällt scrcpy auf den Standardmodus zurück (mit einer Ausgabe in der Konsole).
Dies erlaubt es, dieselben Kommandozeilenargumente zu verwenden, egal ob das Gerät per USB oder TCP/IP verbunden ist.

In diesem Modus werden rohe Tastenevents (scancodes) an das Gerät gesendet.
Aus diesem Grund muss ein nicht passenden Tastaturformat in den Einstellungen des Android-Gerätes unter Einstellungen → System → Sprache und Eingabe → [Physical keyboard] konfiguriert werden.

Diese Einstellungsseite kann direkt mit diesem Befehl geöffnet werden:

```bash
adb shell am start -a android.settings.HARD_KEYBOARD_SETTINGS
```

Diese Option ist jedoch nur verfügbar, wenn eine HID-Tastatur oder eine physische Tastatur verbunden sind.

[Physical keyboard]: https://github.com/Genymobile/scrcpy/pull/2632#issuecomment-923756915

#### Simulation einer physischen Maus mit HID

Ähnlich zu einer Tastatur kann auch eine Maus mithilfe von HID simuliert werden.
Wie zuvor funktioniert dies jedoch nur, wenn das Gerät über USB verbunden ist. Zudem wird dies momentan nur unter Linux unterstützt.

Standardmäßig verwendet scrcpy Android Maus Injektionen mit absoluten Koordinaten.
Durch die Simulation einer physischen Maus erscheint auf dem Display des Geräts ein Mauszeiger, zu welchem die Bewegungen, Klicks und Scrollbewegungen relativ eingegeben werden.

Um diesen Modus zu aktivieren:

```bash
scrcpy --hid-mouse
scrcpy -M  # kurze Version
```

Es kann zudem`--forward-all-clicks` übergeben werden, um [alle Mausklicks an das Gerät weiterzugeben](#rechtsklick-und-mittelklick).

Wenn dieser Modus aktiv ist, ist der Mauszeiger des Computers auf dem Fenster gefangen (Zeiger verschwindet von Computer und erscheint auf dem Android-Gerät).

Spezielle Tasteneingaben wie <kbd>Alt</kbd> oder <kbd>Super</kbd> ändern den Zustand des Mauszeigers (geben diesen wieder frei/fangen ihn wieder ein).
Eine dieser Tasten kann verwendet werden, um die Kontrolle der Maus wieder zurück an den Computer zu geben.


#### OTG

Es ist möglich, _scrcpy_ so auszuführen, dass nur Maus und Tastatur, wie wenn diese direkt über ein OTG-Kabel verbunden wären, simuliert werden.

In diesem Modus ist _adb_ nicht nötig, ebenso ist das Spiegeln der Anzeige deaktiviert.

Um den OTG-Modus zu aktivieren:

```bash
scrcpy --otg
# Seriennummer übergeben, falls mehrere Geräte vorhanden sind
scrcpy --otg -s 0123456789abcdef
```

Es ist möglich, nur HID-Tastatur oder HID-Maus zu aktivieren:

```bash
scrcpy --otg --hid-keyboard              # nur Tastatur
scrcpy --otg --hid-mouse                 # nur Maus
scrcpy --otg --hid-keyboard --hid-mouse  # Tastatur und Maus
# Der Einfachheit halber sind standardmäßig beide aktiv
scrcpy --otg                             # Tastatur und Maus
```

Wie `--hid-keyboard` und `--hid-mouse` funktioniert dies nur, wenn das Gerät per USB verbunden ist.
Zudem wird dies momentan nur unter Linux unterstützt.


#### Textinjektions-Vorliebe

Beim Tippen von Text werden zwei verschiedene [Events][textevents] generiert:
 - _key events_, welche signalisieren, ob eine Taste gedrückt oder losgelassen wurde;
 - _text events_, welche signalisieren, dass Text eingegeben wurde.

Standardmäßig werden key events verwendet, da sich bei diesen die Tastatur in Spielen wie erwartet verhält (typischerweise für WASD).

Dies kann jedoch [Probleme verursachen][prefertext]. Trifft man auf ein solches Problem, so kann dies mit diesem Befehl umgangen werden:

```bash
scrcpy --prefer-text
```

Dies kann jedoch das Tastaturverhalten in Spielen beeinträchtigen/zerstören.

Auf der anderen Seite kann jedoch auch die Nutzung von key events erzwungen werden:

```bash
scrcpy --raw-key-events
```

Diese Optionen haben jedoch keinen Einfluss auf eine etwaige HID-Tastatur, da in diesem modus alle key events als scancodes gesendet werden.

[textevents]: https://blog.rom1v.com/2018/03/introducing-scrcpy/#handle-text-input
[prefertext]: https://github.com/Genymobile/scrcpy/issues/650#issuecomment-512945343


#### Wiederholen von Tasten

Standardmäßig löst das gedrückt halten einer Taste das jeweilige Event mehrfach aus. Dies kann jedoch zu Performanceproblemen in manchen Spielen führen.

Um das Weitergeben von sich wiederholenden Tasteneingaben zu verhindern:

```bash
scrcpy --no-key-repeat
```

This option has no effect on HID keyboard (key repeat is handled by Android
directly in this mode).


#### Rechtsklick und Mittelklick

Standardmäßig löst Rechtsklick BACK (wenn Bildschirm aus: POWER) und Mittelklick BACK aus. Um diese Kürzel abzuschalten und stattdessen die Eingaben direkt an das Gerät weiterzugeben:

```bash
scrcpy --forward-all-clicks
```


### Dateien ablegen

#### APK installieren

Um eine AKP zu installieren, kann diese per Drag-and-drop auf das _scrcpy_-Fenster gezogen werden.

Dabei erfolgt kein visuelles Feedback, ein Log wird in die Konsole ausgegeben.


#### Datei auf Gerät schieben

Um eine Datei nach `/sdcard/Download/` auf dem Gerät zu schieben, Drag-and-drop die (nicht-APK)-Datei auf das _scrcpy_-Fenster.

Dabei erfolgt kein visuelles Feedback, ein Log wird in die Konsole ausgegeben.

Das Zielverzeichnis kann beim Start geändert werden:

```bash
scrcpy --push-target=/sdcard/Movies/
```


### Audioweitergabe

Audio wird von _scrcpy_ nicht übertragen. Hierfür kann [sndcpy] verwendet werden.

Siehe zudem [issue #14].

[sndcpy]: https://github.com/rom1v/sndcpy
[issue #14]: https://github.com/Genymobile/scrcpy/issues/14


## Tastenkürzel

In der folgenden Liste ist <kbd>MOD</kbd> der Kürzel-Auslöser. Standardmäßig ist dies (links) <kbd>Alt</kbd> oder (links) <kbd>Super</kbd>.

Dies kann mithilfe von `--shortcut-mod` geändert werden. Mögliche Tasten sind `lstrg`, `rstrg`,
`lalt`, `ralt`, `lsuper` und `rsuper`. Beispielhaft:

```bash
# Nutze rStrg als Auslöser
scrcpy --shortcut-mod=rctrl

# Nutze entweder LStrg+LAlt oder LSuper für Tastenkürzel
scrcpy --shortcut-mod=lctrl+lalt,lsuper
```

_<kbd>[Super]</kbd> ist typischerweise die <kbd>Windows</kbd> oder <kbd>Cmd</kbd> Taste._

[Super]: https://en.wikipedia.org/wiki/Super_key_(keyboard_button)

| Aktion                                                 | Tastenkürzel                                              |                          |
|--------------------------------------------------------|-----------------------------------------------------------|:-------------------------|
| Vollbild wechseln                                      | <kbd>MOD</kbd>+<kbd>f</kbd>                               |                          |
| Display nach links rotieren                            | <kbd>MOD</kbd>+<kbd>←</kbd> _(links)_                     |                          |
| Display nach links rotieren                            | <kbd>MOD</kbd>+<kbd>→</kbd> _(rechts)_                    |                          |
| Fenstergröße 1:1 replizieren (pixel-perfect)           | <kbd>MOD</kbd>+<kbd>g</kbd>                               |                          |
| Fenstergröße zum entfernen der schwarzen Balken ändern | <kbd>MOD</kbd>+<kbd>w</kbd>                               | _Doppel-Linksklick¹_     |
| Klick auf `HOME`                                       | <kbd>MOD</kbd>+<kbd>h</kbd>                               | _Mittelklick_            |
| Klick auf `BACK`                                       | <kbd>MOD</kbd>+<kbd>b</kbd>                               | _Rechtsklick²_           |
| Klick auf `APP_SWITCH`                                 | <kbd>MOD</kbd>+<kbd>s</kbd>                               | _4.-Taste-Klick³_        |
| Klick auf `MENU` (Bildschirm entsperren)⁴              | <kbd>MOD</kbd>+<kbd>m</kbd>                               |                          |
| Klick auf `VOLUME_UP`                                  | <kbd>MOD</kbd>+<kbd>↑</kbd> _(hoch)_                      |                          |
| CKlick auf `VOLUME_DOWN`                               | <kbd>MOD</kbd>+<kbd>↓</kbd> _(runter)_                    |                          |
| Klick auf `POWER`                                      | <kbd>MOD</kbd>+<kbd>p</kbd>                               |                          |
| Power an                                               | _Rechtsklick²_                                            |                          |
| Gerätebildschirm ausschalten (weiterhin spiegeln)      | <kbd>MOD</kbd>+<kbd>o</kbd>                               |                          |
| Gerätebildschirm einschalten                           | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>o</kbd>              |                          |
| Gerätebildschirm drehen                                | <kbd>MOD</kbd>+<kbd>r</kbd>                               |                          |
| Benachrichtigungs-Bereich anzeigen                     | <kbd>MOD</kbd>+<kbd>n</kbd>                               | _5.-Taste-Klick³_        |
| Erweitertes Einstellungs-Menü anzeigen                 | <kbd>MOD</kbd>+<kbd>n</kbd>+<kbd>n</kbd>                  | _Doppel-5.-Taste-Klick³_ |
| Bedienfelder einklappen                                | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>n</kbd>              |                          |
| In die Zwischenablage kopieren⁵                        | <kbd>MOD</kbd>+<kbd>c</kbd>                               |                          |
| In die Zwischenablage kopieren⁵                        | <kbd>MOD</kbd>+<kbd>x</kbd>                               |                          |
| Zwischenablage synchronisieren und einfügen⁵           | <kbd>MOD</kbd>+<kbd>v</kbd>                               |                          |
| Computer-Zwischenablage einfügen (per Tastenevents)    | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd>              |                          |
| FPS-Zähler aktivieren/deaktivieren (ing stdout)        | <kbd>MOD</kbd>+<kbd>i</kbd>                               |                          |
| Ziehen zum Zoomen                                      | <kbd>Strg</kbd>+_Klicken-und-Bewegen_                     |                          |
| Drag-and-drop mit APK-Datei                            | APK von Computer installieren                             |                          |
| Drag-and-drop mit Nicht-APK Datei                      | [Datei auf das Gerät schieben](#datei-auf-gerät-schieben) |                          |


_¹Doppelklick auf die schwarzen Balken, um diese zu entfernen._  
_²Rechtsklick aktiviert den Bildschirm, falls dieser aus war, ansonsten ZURÜCK._  
_³4. und 5. Maustasten, wenn diese an der jeweiligen Maus vorhanden sind._  
_⁴Für react-native Applikationen in Entwicklung, `MENU` öffnet das Entwickler-Menü._  
_⁵Nur für Android >= 7._

Abkürzungen mit mehreren Tastenanschlägen werden durch das Loslassen und erneute Drücken der Taste erreicht.
Beispielhaft, um "Erweitere das Einstellungs-Menü" auszuführen:

 1. Drücke und halte <kbd>MOD</kbd>.
 2. Doppelklicke <kbd>n</kbd>.
 3. Lasse <kbd>MOD</kbd> los.

Alle <kbd>Strg</kbd>+_Taste_ Tastenkürzel werden an das Gerät übergeben, sodass sie von der jeweiligen Applikation ausgeführt werden können.


## Personalisierte Pfade

Um eine spezifische _adb_ Binary zu verwenden, muss deren Pfad als Umgebungsvariable `ADB` deklariert werden:

```bash
ADB=/path/to/adb scrcpy
```

Um den Pfad der `scrcpy-server` Datei zu bearbeiten, muss deren Pfad in `SCRCPY_SERVER_PATH` bearbeitet werden.

Um das Icon von scrcpy zu ändern, muss `SCRCPY_ICON_PATH` geändert werden.


## Warum _scrcpy_?

Ein Kollege hat mich dazu herausgefordert, einen Namen so unaussprechbar wie [gnirehtet] zu finden.

[`strcpy`] kopiert einen **str**ing; `scrcpy` kopiert einen **scr**een.

[gnirehtet]: https://github.com/Genymobile/gnirehtet
[`strcpy`]: http://man7.org/linux/man-pages/man3/strcpy.3.html


## Selbst bauen?

Siehe [BUILD].


## Typische Fehler

Siehe [FAQ](FAQ.md).


## Entwickler

[Entwicklerseite](DEVELOP.md).


## Licence

    Copyright (C) 2018 Genymobile
    Copyright (C) 2018-2022 Romain Vimont

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.

## Artikel (auf Englisch)

- [Introducing scrcpy][article-intro]
- [Scrcpy now works wirelessly][article-tcpip]

[article-intro]: https://blog.rom1v.com/2018/03/introducing-scrcpy/
[article-tcpip]: https://www.genymotion.com/blog/open-source-project-scrcpy-now-works-wirelessly/
