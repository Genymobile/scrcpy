_Apri il [README](README.md) originale (in inglese) e sempre aggiornato._

<img src="app/data/icon.svg" width="128" height="128" alt="scrcpy" align="right" />

# scrcpy (v1.23)

_si pronuncia "**scr**een **c**o**py**"_

[Leggi in altre lingue](#traduzioni)

Questa applicazione fornisce la visualizzazione e il controllo di dispositivi Android collegati via USB (o [via TCP/IP](#tcpip-wireless)). Non richiede alcun accesso _root_.
Funziona su _GNU/Linux_, _Windows_ e _macOS_.

![screenshot](assets/screenshot-debian-600.jpg)

Si concentra su:

 - **leggerezza**: nativo, mostra solo lo schermo del dispositivo
 - **prestazioni**: 30~120fps, in funzione del dispositivo
 - **qualità**: 1920×1080 o superiore
 - **bassa latenza**: [35~70ms][lowlatency]
 - **tempo di avvio basso**: ~ 1secondo per visualizzare la prima immagine
 - **non invadenza**: nulla rimane installato sul dispositivo
 - **vantaggi per l'utente**: nessun account, nessuna pubblicità, non è richiesta alcuna connessione a internet
 - **libertà**: software libero e a codice aperto (_free and open source_)

[lowlatency]: https://github.com/Genymobile/scrcpy/pull/646

Le sue caratteristiche includono:
 - [registrazione](#registrazione)
 - mirroring con [schermo del dispositivo spento](#spegnere-lo-schermo)
 - [copia-incolla](#copia-incolla) in entrambe le direzioni
 - [qualità configurabile](#configurazione-di-acquisizione)
 - schermo del dispositivo [come webcam (V4L2)](#v4l2loopback) (solo per Linux)
 - [simulazione della tastiera fisica (HID)](#simulazione-della-tastiera-fisica-HID)
 - [simulazione mouse fisico (HID)](#simulazione-del-mouse-fisico-HID)
 - [modalità OTG](#otg)
 - e altro ancora...


## Requisiti

Il dispositivo Android richiede almeno le API 21 (Android 5.0).

Assiucurati di aver [attivato il debug usb][enable-adb] sul(/i) tuo(i) dispositivo(/i).

[enable-adb]: https://developer.android.com/studio/command-line/adb.html#Enabling

In alcuni dispositivi, devi anche abilitare [un'opzione aggiuntiva][control] per controllarli con tastiera e mouse.

[control]: https://github.com/Genymobile/scrcpy/issues/70#issuecomment-373286323

## Ottieni l'app

<a href="https://repology.org/project/scrcpy/versions"><img src="https://repology.org/badge/vertical-allrepos/scrcpy.svg" alt="Packaging status" align="right"></a>

### Sommario

 - Linux: `apt install scrcpy`
 - Windows: [download](README.md#windows)
 - macOS: `brew install scrcpy`

Compila dai sorgenti: [BUILD] (in inglese) ([procedimento semplificato][BUILD_simple] (in inglese))

[BUILD]: BUILD.md
[BUILD_simple]: BUILD.md#simple


### Linux

Su Debian e Ubuntu:

```
apt install scrcpy
```

Su Arch Linux:

```
pacman -S scrcpy
```

È disponibile anche un pacchetto [Snap]: [`scrcpy`][snap-link].

[snap-link]: https://snapstats.org/snaps/scrcpy

[snap]: https://it.wikipedia.org/wiki/Snappy_(gestore_pacchetti)

Per Fedora, è disponibile un pacchetto [COPR]: [`scrcpy`][copr-link].

[COPR]: https://fedoraproject.org/wiki/Category:Copr
[copr-link]: https://copr.fedorainfracloud.org/coprs/zeno/scrcpy/


Per Gentoo, è disponibile una [Ebuild]: [`scrcpy/`][ebuild-link].

[Ebuild]: https://wiki.gentoo.org/wiki/Ebuild
[ebuild-link]: https://github.com/maggu2810/maggu2810-overlay/tree/master/app-mobilephone/scrcpy

Puoi anche [compilare l'app manualmente][BUILD] (in inglese) ([procedimento semplificato][BUILD_simple] (in inglese)).


### Windows

Per Windows, per semplicità è disponibile un archivio precompilato con tutte le dipendenze (incluso `adb`):

 - [README](README.md#windows) (Link al README originale per l'ultima versione)

È anche disponibile in [Chocolatey]:

[Chocolatey]: https://chocolatey.org/

```bash
choco install scrcpy
choco install adb    # se non lo hai già
```

E in [Scoop]:

```bash
scoop install scrcpy
scoop install adb    # se non lo hai già
```

[Scoop]: https://scoop.sh

Puoi anche [compilare l'app manualmente][BUILD] (in inglese).


### macOS

L'applicazione è disponibile in [Homebrew]. Basta installarlo:

[Homebrew]: https://brew.sh/

```bash
brew install scrcpy
```

Serve che `adb` sia accessibile dal tuo `PATH`. Se non lo hai già:

```bash
brew install android-platform-tools
```

È anche disponibile in [MacPorts], che imposta adb per te:

```bash
sudo port install scrcpy
```

[MacPorts]: https://www.macports.org/


Puoi anche [compilare l'app manualmente][BUILD] (in inglese).


## Esecuzione

Collega un dispositivo Android ed esegui:

```bash
scrcpy
```

Scrcpy accetta argomenti da riga di comando, elencati con:

```bash
scrcpy --help
```

## Funzionalità

### Configurazione di acquisizione

#### Riduci dimensione

Qualche volta è utile trasmettere un dispositvo Android ad una definizione inferiore per aumentare le prestazioni.

Per limitare sia larghezza che altezza ad un certo valore (ad es. 1024):

```bash
scrcpy --max-size 1024
scrcpy -m 1024  # versione breve
```

L'altra dimensione è calcolata in modo tale che il rapporto di forma del dispositivo sia preservato.
In questo esempio un dispositivo in 1920x1080 viene trasmesso a 1024x576.


#### Cambia bit-rate (velocità di trasmissione)

Il bit-rate predefinito è 8 Mbps. Per cambiare il bitrate video (ad es. a 2 Mbps):

```bash
scrcpy --bit-rate 2M
scrcpy -b 2M  # versione breve
```

#### Limitare il frame rate (frequenza di fotogrammi)

Il frame rate di acquisizione può essere limitato:

```bash
scrcpy --max-fps 15
```

Questo è supportato ufficialmente a partire da Android 10, ma potrebbe funzionare in versioni precedenti.

L'attuale frame rate di acquisizione può essere stampato sulla console:

```
scrcpy --print-fps
```

Può anche essere abilitato o disabilitato in qualsiasi momento con <kbd>MOD</kbd>+<kbd>i</kbd>.

#### Ritaglio

Lo schermo del dispositivo può essere ritagliato per visualizzare solo parte di esso.

Questo può essere utile, per esempio, per trasmettere solo un occhio dell'Oculus Go:

```bash
scrcpy --crop 1224:1440:0:0   # 1224x1440 at offset (0,0)
```

Se anche `--max-size` è specificata, il ridimensionamento è applicato dopo il ritaglio.


#### Blocca orientamento del video


Per bloccare l'orientamento della trasmissione:

```bash
scrcpy --lock-video-orientation     # orientamento iniziale (corrente)
scrcpy --lock-video-orientation=0   # orientamento naturale
scrcpy --lock-video-orientation=1   # 90° antiorario
scrcpy --lock-video-orientation=2   # 180°
scrcpy --lock-video-orientation=3   # 90° orario
```

Questo influisce sull'orientamento della registrazione.


La [finestra può anche essere ruotata](#rotazione) indipendentemente.


#### Codificatore

Alcuni dispositivi hanno più di un codificatore e alcuni di questi possono provocare problemi o crash. È possibile selezionare un encoder diverso:

```bash
scrcpy --encoder OMX.qcom.video.encoder.avc
```

Per elencare i codificatori disponibili puoi immettere un nome di codificatore non valido e l'errore mostrerà i codificatori disponibili:

```bash
scrcpy --encoder _
```

### Cattura

#### Registrazione

È possibile registrare lo schermo durante la trasmissione:

```bash
scrcpy --record file.mp4
scrcpy -r file.mkv
```

Per disabilitare la trasmissione durante la registrazione:

```bash
scrcpy --no-display --record file.mp4
scrcpy -Nr file.mkv
# interrompere la registrazione con Ctrl+C
```

I "fotogrammi saltati" sono registrati nonostante non siano mostrati in tempo reale (per motivi di prestazioni). I fotogrammi sono _datati_ sul dispositivo, così una [variazione di latenza dei pacchetti][packet delay variation] non impatta il file registrato.

[packet delay variation]: https://en.wikipedia.org/wiki/Packet_delay_variation


#### v4l2loopback

Su Linux è possibile inviare il flusso video ad un dispositivo v4l2 loopback, cosicché un dispositivo Android possa essere aperto come una webcam da qualsiasi strumento compatibile con v4l2.

Il modulo `v4l2loopback` deve essere installato:

```bash
sudo apt install v4l2loopback-dkms
```

Per creare un dispositvo v4l2:

```bash
sudo modprobe v4l2loopback
```

Questo creerà un nuovo dispositivo video in `/dev/videoN` dove `N` è un intero (più [opzioni](https://github.com/umlaeute/v4l2loopback#options) sono disponibili per crere più dispositivi o dispositivi con ID specifici).

Per elencare i dispositvi attivati:

```bash
# necessita del pacchetto v4l-utils
v4l2-ctl --list-devices

# semplice ma potrebbe essere sufficiente
ls /dev/video*
```

Per avviare scrcpy utilizzando un v4l2 sink:

```bash
scrcpy --v4l2-sink=/dev/videoN
scrcpy --v4l2-sink=/dev/videoN --no-display  # disabilita la finestra di trasmissione
scrcpy --v4l2-sink=/dev/videoN -N            # versione corta
```

(sostituisci `N` con l'ID del dispositivo, controlla con `ls /dev/video*`)

Una volta abilitato, puoi aprire il tuo flusso video con uno strumento compatibile con v4l2:

```bash
ffplay -i /dev/videoN
vlc v4l2:///dev/videoN   # VLC potrebbe aggiungere del ritardo per il buffer
```

Per esempio potresti catturare il video in [OBS].

[OBS]: https://obsproject.com/


#### Buffering

È possibile aggiungere del buffer. Questo aumenta la latenza ma riduce il jitter (vedi [#2464]).

[#2464]: https://github.com/Genymobile/scrcpy/issues/2464

L'opzione è disponibile per il buffer della visualizzazione:

```bash
scrcpy --display-buffer=50  # aggiungi 50 ms di buffer per la visualizzazione
```

e per il V4L2 sink:

```bash
scrcpy --v4l2-buffer=500    #  aggiungi 500 ms di buffer per il v4l2 sink
```


### Connessione

#### TCP/IP (wireless)

_Scrcpy_ usa `adb` per comunicare col dispositivo e `adb` può [connettersi][connect] a un dispositivo mediante TCP/IP. Il dispositivo deve essere collegato alla stessa rete del computer.

##### Automatico

Un'opzione `--tcpip` permette di configurare automaticamente la connessione. Ci sono due varianti.

Se il dispositivo (accessibile a 192.168.1.1 in questo esempio) ascolta già su una porta (tipicamente 5555) per le connessioni adb in entrata, allora esegui:

```bash
scrcpy --tcpip=192.168.1.1       # la porta predefinita è 5555
scrcpy --tcpip=192.168.1.1:5555
```

Se la modalità TCP/IP di adb è disabilitata sul dispositivo (o se non si conosce l'indirizzo IP indirizzo), collegare il dispositivo tramite USB, quindi eseguire:

```bash
scrcpy --tcpip # senza argomenti
```

Il comando troverà automaticamente l'indirizzo IP del dispositivo, abiliterà la modalità TCP/IP, quindi connettersi al dispositivo prima di iniziare.

##### Manuale

In alternativa, è possibile abilitare la connessione TCP/IP manualmente usando `adb`:

1. Inserisci il dispositivo in una porta USB del tuo computer.
2. Connetti il dispositivo alla stessa rete Wi-Fi del tuo computer.
3. Ottieni l'indirizzo IP del tuo dispositivo, in Impostazioni → Informazioni sul telefono → Stato, o
   eseguendo questo comando:

    ```bash
    adb shell ip route | awk '{print $9}'
    ```

4. Abilita adb via TCP/IP sul tuo dispositivo: `adb tcpip 5555`.
5. Scollega il tuo dispositivo.
6. Connettiti al tuo dispositivo: `adb connect DEVICE_IP:5555` _(sostituisci `DEVICE_IP`
con l'indirizzo IP del dispositivo che hai trovato)_.
7. Esegui `scrcpy` come al solito.

Da Android 11, una [opzione di debug wireless][adb-wireless] permette di evitare di dover collegare fisicamente il dispositivo direttamente al computer.

[adb-wireless]: https://developer.android.com/studio/command-line/adb#connect-to-a-device-over-wi-fi-android-11+

Se la connessione cade casualmente, esegui il comando `scrcpy` per riconnetterti. Se il comando dice che non ci sono dispositivi/emulatori trovati, prova ad eseguire `adb connect DEVICE_IP:5555` di nuovo, e poi `scrcpy` come al solito. Se dice ancora che non ne ha trovato nessuno, prova ad eseguire `adb disconnect` e poi esegui di nuovo questi due comandi.

Potrebbe essere utile diminuire il bit-rate e la definizione:

```bash
scrcpy --bit-rate 2M --max-size 800
scrcpy -b2M -m800 # versione breve
```

[connect]: https://developer.android.com/studio/command-line/adb.html#wireless

#### Multi dispositivo

Se in `adb devices` sono elencati più dispositivi, è necessario specificare il _seriale_:

```bash
scrcpy --serial 0123456789abcdef
scrcpy -s 0123456789abcdef  # versione breve
```

Se il dispositivo è collegato mediante TCP/IP:

```bash
scrcpy --serial 192.168.0.1:5555
scrcpy -s 192.168.0.1:5555  # versione breve
```

Se solo un dispositivo è collegato via USB o TCP/IP, è possibile selezionarlo automaticamente:

```bash
# Select the only device connected via USB
scrcpy -d             # like adb -d
scrcpy --select-usb   # long version

# Select the only device connected via TCP/IP
scrcpy -e             # like adb -e
scrcpy --select-tcpip # long version
```

Puoi avviare più istanze di _scrcpy_ per diversi dispositivi.


#### Avvio automativo alla connessione del dispositivo

Potresti usare [AutoAdb]:

```bash
autoadb scrcpy -s '{}'
```

[AutoAdb]: https://github.com/rom1v/autoadb

#### Tunnels

Per connettersi a un dispositivo remoto, è possibile collegare un client `adb` locale a un server remoto `adb` (purché usino la stessa versione del protocollo _adb_). ).

##### Server ADB remoto

Per connettersi a un server ADB remoto, fate ascoltare il server su tutte le interfacce:

```bash
adb kill-server
adb -a nodaemon server start
# tienilo aperto
```

**Attenzione: tutte le comunicazioni tra i client e il server ADB non sono criptate.**

Supponi che questo server sia accessibile a 192.168.1.2. Poi, da un altro terminale, esegui scrcpy:

```bash
export ADB_SERVER_SOCKET=tcp:192.168.1.2:5037
scrcpy --tunnel-host=192.168.1.2
```

Per impostazione predefinita, scrcpy utilizza la porta locale utilizzata per il tunnel `adb forward` (tipicamente `27183`, vedi `--port`). È anche possibile forzare una diversa porta del tunnel (può essere utile in situazioni più complesse, quando sono coinvolti più reindirizzamenti):

```
scrcpy --tunnel-port=1234
```

##### SSH tunnel

Per comunicare con un server ADB remoto in modo sicuro, è preferibile utilizzare un tunnel SSH.

Per prima cosa, assicurati che il server ADB sia in esecuzione sul computer remoto:

```bash
adb start-server
```

Poi, crea un tunnel SSH:

```bash
# local  5038 --> remote  5037
# local 27183 <-- remote 27183
ssh -CN -L5038:localhost:5037 -R27183:localhost:27183 your_remote_computer
# keep this open
```

Da un altro terminale, esegui scrcpy:

```bash
export ADB_SERVER_SOCKET=tcp:localhost:5038
scrcpy
```

Per evitare l'abilitazione dell'apertura porte remota potresti invece forzare una "forward connection" (notare il `-L` invece di `-R`)

```bash
# local  5038 --> remote  5037
# local 27183 --> remote 27183
ssh -CN -L5038:localhost:5037 -L27183:localhost:27183 your_remote_computer
# tieni questo aperto
```

Da un altro terminale, esegui scrcpy:

```bash
export ADB_SERVER_SOCKET=tcp:localhost:5038
scrcpy --force-adb-forward
```

Come per le connessioni wireless potrebbe essere utile ridurre la qualità:

```
scrcpy -b2M -m800 --max-fps 15
```

### Configurazione della finestra

#### Titolo

Il titolo della finestra è il modello del dispositivo per impostazione predefinita. Esso può essere cambiato:

```bash
scrcpy --window-title 'My device'
```

#### Posizione e dimensione

La posizione e la dimensione iniziale della finestra può essere specificata:

```bash
scrcpy --window-x 100 --window-y 100 --window-width 800 --window-height 600
```

#### Senza bordi

Per disabilitare le decorazioni della finestra:

```bash
scrcpy --window-borderless
```

#### Sempre in primo piano

Per tenere scrcpy sempre in primo piano:

```bash
scrcpy --always-on-top
```

#### Schermo intero

L'app può essere avviata direttamente a schermo intero:

```bash
scrcpy --fullscreen
scrcpy -f  # versione breve
```

Lo schermo intero può anche essere attivato/disattivato con <kbd>MOD</kbd>+<kbd>f</kbd>.

#### Rotazione

La finestra può essere ruotata:

```bash
scrcpy --rotation 1
```

I valori possibili sono:
 - `0`: nessuna rotazione
 - `1`: 90 gradi antiorari
 - `2`: 180 gradi
 - `3`: 90 gradi orari

La rotazione può anche essere cambiata dinamicamente con <kbd>MOD</kbd>+<kbd>←</kbd>
_(sinistra)_ e <kbd>MOD</kbd>+<kbd>→</kbd> _(destra)_.

Notare che _scrcpy_ gestisce 3 diversi tipi di rotazione:
 - <kbd>MOD</kbd>+<kbd>r</kbd> richiede al dispositvo di cambiare tra orientamento verticale (portrait) e orizzontale (landscape) (l'app in uso potrebbe rifiutarsi se non supporta l'orientamento richiesto).
 - [`--lock-video-orientation`](#blocca-orientamento-del-video) cambia l'orientamento della trasmissione (l'orientamento del video inviato dal dispositivo al computer). Questo influenza la registrazione.
 - `--rotation` (o <kbd>MOD</kbd>+<kbd>←</kbd>/<kbd>MOD</kbd>+<kbd>→</kbd>) ruota solo il contenuto della finestra. Questo influenza solo la visualizzazione, non la registrazione.


### Altre opzioni di trasmissione

#### "Sola lettura"

Per disabilitare i controlli (tutto ciò che può interagire col dispositivo: tasti di input, eventi del mouse, trascina e rilascia (drag&drop) file):

```bash
scrcpy --no-control
scrcpy -n
```

#### Schermo

Se sono disponibili più schermi, è possibile selezionare lo schermo da trasmettere:

```bash
scrcpy --display 1
```

La lista degli id schermo può essere ricavata da:

```bash
adb shell dumpsys display   # cerca "mDisplayId=" nell'output
```

Lo schermo secondario potrebbe essere possibile controllarlo solo se il dispositivo esegue almeno Android 10 (in caso contrario è trasmesso in modalità sola lettura).


#### Mantenere sbloccato

Per evitare che il dispositivo si blocchi dopo un po' che il dispositivo è collegato:

```bash
scrcpy --stay-awake
scrcpy -w
```

Lo stato iniziale è ripristinato quando scrcpy viene chiuso.


#### Spegnere lo schermo

È possibile spegnere lo schermo del dispositivo durante la trasmissione con un'opzione da riga di comando:

```bash
scrcpy --turn-screen-off
scrcpy -S
```

Oppure premendo <kbd>MOD</kbd>+<kbd>o</kbd> in qualsiasi momento.

Per riaccenderlo premere <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>o</kbd>.

In Android il pulsante `POWER` (tasto di accensione) accende sempre lo schermo. Per comodità, se `POWER` è inviato via scrcpy (con click destro o con <kbd>MOD</kbd>+<kbd>p</kbd>), si forza il dispositivo a spegnere lo schermo dopo un piccolo ritardo (appena possibile).
Il pulsante fisico `POWER` continuerà ad accendere lo schermo normalmente.

Può anche essere utile evitare il blocco del dispositivo:

```bash
scrcpy --turn-screen-off --stay-awake
scrcpy -Sw
```

#### Spegnimento alla chiusura

Per spegnere lo schermo del dispositivo quando si chiude scrcpy:

```bash
scrcpy --power-off-on-close
```


#### Mostrare i tocchi

Per le presentazioni può essere utile mostrare i tocchi fisici (sul dispositivo fisico).

Android fornisce questa funzionalità nelle _Opzioni sviluppatore_.

_Scrcpy_ fornisce un'opzione per abilitare questa funzionalità all'avvio e ripristinare il valore iniziale alla chiusura:

```bash
scrcpy --show-touches
scrcpy -t
```

Notare che mostra solo i tocchi _fisici_ (con le dita sul dispositivo).


#### Disabilitare il salvaschermo

In maniera predefinita scrcpy non previene l'attivazione del salvaschermo del computer.

Per disabilitarlo:

```bash
scrcpy --disable-screensaver
```


### Input di controlli

#### Rotazione dello schermo del dispostivo

Premere <kbd>MOD</kbd>+<kbd>r</kbd> per cambiare tra le modalità verticale (portrait) e orizzontale (landscape).

Notare che la rotazione avviene solo se l'applicazione in primo piano supporta l'orientamento richiesto.

#### Copia-incolla

Quando gli appunti di Android cambiano, essi vengono automaticamente sincronizzati con gli appunti del computer.

Qualsiasi scorciatoia <kbd>Ctrl</kbd> viene inoltrata al dispositivo. In particolare:
 - <kbd>Ctrl</kbd>+<kbd>c</kbd> copia
 - <kbd>Ctrl</kbd>+<kbd>x</kbd> taglia
 - <kbd>Ctrl</kbd>+<kbd>v</kbd> incolla (dopo la sincronizzazione degli appunti da computer a dispositivo)

Questo solitamente funziona come ci si aspetta.

Il comportamento reale, però, dipende dall'applicazione attiva. Per esempio _Termux_ invia SIGINT con <kbd>Ctrl</kbd>+<kbd>c</kbd>, e _K-9 Mail_ compone un nuovo messaggio.

Per copiare, tagliare e incollare in questi casi (ma è solo supportato in Android >= 7):
 - <kbd>MOD</kbd>+<kbd>c</kbd> invia `COPY`
 - <kbd>MOD</kbd>+<kbd>x</kbd> invia `CUT`
 - <kbd>MOD</kbd>+<kbd>v</kbd> invia `PASTE` (dopo la sincronizzazione degli appunti da computer a dispositivo)

In aggiunta, <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd> permette l'invio del testo degli appunti del computer come una sequenza di eventi pressione dei tasti. Questo è utile quando il componente non accetta l'incollaggio di testo (per esempio in _Termux_), ma questo può compromettere il contenuto non ASCII.

**AVVISO:** Incollare gli appunti del computer nel dispositivo (sia con <kbd>Ctrl</kbd>+<kbd>v</kbd> che con <kbd>MOD</kbd>+<kbd>v</kbd>) copia il contenuto negli appunti del dispositivo. Come conseguenza, qualsiasi applicazione Android potrebbe leggere il suo contenuto. Dovresti evitare di incollare contenuti sensibili (come password) in questa maniera.

Alcuni dispositivi non si comportano come aspettato quando si modificano gli appunti del dispositivo a livello di codice. L'opzione `--legacy-paste` è fornita per cambiare il comportamento di <kbd>Ctrl</kbd>+<kbd>v</kbd> and <kbd>MOD</kbd>+<kbd>v</kbd> in modo tale che anch'essi inviino il testo degli appunti del computer come una sequenza di eventi di pressione dei tasti (nella stessa maniera di <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd>).

Per disabilitare la sincronizzazione automatica degli appunti, usa `--no-clipboard-autosync`.

#### Pizzica per zoomare (pinch-to-zoom)

Per simulare il "pizzica per zoomare": <kbd>Ctrl</kbd>+_click e trascina_.

Più precisamente, tieni premuto <kbd>Ctrl</kbd> mentre premi il pulsante sinistro. Finchè il pulsante non sarà rilasciato, tutti i movimenti del mouse ridimensioneranno e ruoteranno il contenuto (se supportato dall'applicazione) relativamente al centro dello schermo.

Concretamente, scrcpy genera degli eventi di tocco addizionali di un "dito virtuale" nella posizione simmetricamente opposta rispetto al centro dello schermo.

#### Simulazione della tastiera fisica (HID)

Per impostazione predefinita, scrcpy utilizza l'invio dei tasti o del testo di Android: funziona ovunque, ma è limitato all'ASCII.

In alternativa scrcpy può simulare una tastiera fisica USB su Android per fornire una migliore esperienza di input (utilizzando [USB HID over AOAv2][hid-aoav2]): la tastiera virtuale è disabilitata e funziona per tutti i caratteri e IME.

[hid-aoav2]: https://source.android.com/devices/accessories/aoa2#hid-support

Tuttavia, funziona solo se il dispositivo è collegato via USB.

Nota: su Windows, può funzionare solo in [odalità OTG](#otg), non durante il mirroring (non è possibile aprire un dispositivo USB se è già aperto da un altro processo come il daemon adb).

Per abilitare questa modalità:

```bash
scrcpy --hid-keyboard
scrcpy -K # versione breve
```

Se fallisce per qualche motivo (per esempio perché il dispositivo non è connesso via USB), ritorna automaticamente alla modalità predefinita (con un log nella console). Questo permette di usare le stesse opzioni della linea di comando quando si è connessi via USB e TCP/IP.

In questa modalità, gli eventi i pressione originali (scancodes) sono inviati al dispositivo, indipendentemente dalla mappatura dei tasti dell'host. Pertanto, se il layout della tua tastiera non corrisponde, deve essere configurato sul dispositivo Android, in Impostazioni → Sistema → Lingue e input → [Tastiera fisica] (in inglese).

Questa pagina di impostazioni può essere avviata direttamente:

```bash
adb shell am start -a android.settings.HARD_KEYBOARD_SETTINGS
```

Tuttavia, l'opzione è disponibile solo quando la tastiera HID è abilitata (o quando una tastiera fisica è collegata).

[Tastiera fisica]: https://github.com/Genymobile/scrcpy/pull/2632#issuecomment-923756915

#### Simulazione del mouse fisico (HID)

In modo simile alla simulazione della tastiera fisica, è possibile simulare un mouse fisico. Allo stesso modo funziona solo se il dispositivo è connesso via USB.

Per impostazione predefinita, scrcpy utilizza l'invio degli eventi del mouse di Android, utilizzando coordinate assolute. Simulando un mouse fisico, un puntatore del mouse appare sul dispositivo Android e vengono inviati i movimenti relativi del mouse, i click e gli scorrimenti.

Per abilitare questa modalità:

```bash
scrcpy --hid-mouse
scrcpy -M # versione breve
```

Si potrebbe anche aggiungere `--forward-all-clicks` a [inoltra tutti i pulsanti del mouse][forward_all_clicks].

[forward_all_clicks]: #click-destro-e-click-centrale


Quando questa modalità è attivata, il mouse del computer viene "catturato" (il puntatore del mouse scompare dal computer e appare invece sul dispositivo Android).

I tasti speciali di cattura, <kbd>Alt</kbd> o <kbd>Super</kbd>, commutano (disabilitano o abilitano) la cattura del mouse. Usa uno di essi per ridare il controllo del mouse al computer.


#### OTG

È possibile eseguire _scrcpy_ con la sola simulazione della tastiera fisica e del mouse (HID), come se la tastiera e il mouse del computer fossero collegati direttamente al dispositivo tramite un cavo OTG.

In questa modalità, _adb_ (debug USB) non è necessario e il mirroring è disabilitato.

Per attivare la modallità OTG:

```bash
scrcpy --otg
# Passa la seriale se sono disponibili diversi dispositivi USB
scrcpy --otg -s 0123456789abcdef
```

È possibile abilitare solo la tastiera HID o il mouse HID:

```bash
scrcpy --otg --hid-keyboard # solo la tastiera
scrcpy --otg --hid-mouse # solo mouse
scrcpy --otg --hid-keyboard --hid-mouse # tastiera e mouse
# per comodità, abilita entrambi per default
scrcpy --otg # tastiera e mouse
```

Come `--hid-keyboard` e `--hid-mouse`, funziona solo se il dispositivo è collegato via USB.


#### Preferenze di invio del testo

Ci sono due tipi di [eventi][textevents] generati quando si scrive testo:
 - _eventi di pressione_, segnalano che tasto è stato premuto o rilasciato;
 - _eventi di testo_, segnalano che del testo è stato inserito.

In maniera predefinita le lettere sono inviate usando gli eventi di pressione, in maniera tale che la tastiera si comporti come aspettato nei giochi (come accade solitamente per i tasti WASD).

Questo, però, può [causare problemi][prefertext]. Se incontri un problema del genere, puoi evitarlo con:

```bash
scrcpy --prefer-text
```

(ma questo romperà il normale funzionamento della tastiera nei giochi)

Al contrario, si potrebbe forzare per inviare sempre eventi di pressione grezzi:

```bash
scrcpy --raw-key-events
```

Queste opzioni non hanno effetto sulla tastiera HID (tutti gli eventi di pressione sono inviati come scancodes in questa modalità).

[textevents]: https://blog.rom1v.com/2018/03/introducing-scrcpy/#handle-text-input
[prefertext]: https://github.com/Genymobile/scrcpy/issues/650#issuecomment-512945343


#### Ripetizione di tasti

In maniera predefinita, tenere premuto un tasto genera una ripetizione degli eventi di pressione di tale tasto. Questo può creare problemi di performance in alcuni giochi, dove questi eventi sono inutilizzati.

Per prevenire l'inoltro ripetuto degli eventi di pressione:

```bash
scrcpy --no-key-repeat
```

Questa opzione non ha effetto sulla tastiera HID (la ripetizione dei tasti è gestita da Android direttamente in questa modalità).


#### Click destro e click centrale

In maniera predefinita, click destro aziona BACK (indietro) o POWER on (accensione) e il click centrale aziona HOME. Per disabilitare queste scorciatoie e, invece, inviare i click al dispositivo:

```bash
scrcpy --forward-all-clicks
```


### Rilascio di file

#### Installare APK

Per installare un APK, trascina e rilascia un file APK (finisce con `.apk`) nella finestra di _scrcpy_.

Non c'è alcuna risposta visiva, un log è stampato nella console.


#### Trasferimento di file verso il dispositivo

Per trasferire un file in `/sdcard/Download` del dispositivo trascina e rilascia un file (non APK) nella finestra di _scrcpy_.

Non c'è alcuna risposta visiva, un log è stampato nella console.

La cartella di destinazione può essere cambiata all'avvio:

```bash
scrcpy --push-target=/sdcard/Movies/
```


### Inoltro dell'audio

L'audio non è inoltrato da _scrcpy_. Usa [sndcpy].

Vedi anche la [issue #14].

[sndcpy]: https://github.com/rom1v/sndcpy
[issue #14]: https://github.com/Genymobile/scrcpy/issues/14


## Scociatoie

Nella lista seguente, <kbd>MOD</kbd> è il modificatore delle scorciatoie. In maniera predefinita è <kbd>Alt</kbd> (sinistro) o <kbd>Super</kbd> (sinistro).

Può essere cambiato usando `--shortcut-mod`. I tasti possibili sono `lctrl`, `rctrl`, `lalt`, `ralt`, `lsuper` and `rsuper` (`l` significa sinistro e `r` significa destro). Per esempio:

```bash
# usa ctrl destro per le scorciatoie
scrcpy --shortcut-mod=rctrl

# use sia "ctrl sinistro"+"alt sinistro" che "super sinistro" per le scorciatoie
scrcpy --shortcut-mod=lctrl+lalt,lsuper
```

_<kbd>[Super]</kbd> è solitamente il pulsante <kbd>Windows</kbd> o <kbd>Cmd</kbd>._

[Super]: https://it.wikipedia.org/wiki/Tasto_Windows
<!-- https://en.wikipedia.org/wiki/Super_key_(keyboard_button) è la pagina originale di Wikipedia inglese, l'ho sostituita con una simile in quello italiano -->

 | Azione                                      |   Scorciatoia
 | ------------------------------------------- |:-----------------------------
 | Schermo intero                      | <kbd>MOD</kbd>+<kbd>f</kbd>
 | Rotazione schermo a sinistra                         | <kbd>MOD</kbd>+<kbd>←</kbd> _(sinistra)_
 | Rotazione schermo a destra                        | <kbd>MOD</kbd>+<kbd>→</kbd> _(destra)_
 | Ridimensiona finestra a 1:1 (pixel-perfect)        | <kbd>MOD</kbd>+<kbd>g</kbd>
 | Ridimensiona la finestra per rimuovere i bordi neri       | <kbd>MOD</kbd>+<kbd>w</kbd> \| _Doppio click sinistro¹_
 | Premi il tasto `HOME`                             | <kbd>MOD</kbd>+<kbd>h</kbd> \| _Click centrale_
 | Premi il tasto `BACK`                             | <kbd>MOD</kbd>+<kbd>b</kbd> \| _Click destro²_
 | Premi il tasto `APP_SWITCH`                       | <kbd>MOD</kbd>+<kbd>s</kbd> \| _4° click³_
 | Premi il tasto `MENU` (sblocca lo schermo)⁴           | <kbd>MOD</kbd>+<kbd>m</kbd>
 | Premi il tasto `VOLUME_UP`                        | <kbd>MOD</kbd>+<kbd>↑</kbd> _(su)_
 | Premi il tasto `VOLUME_DOWN`                      | <kbd>MOD</kbd>+<kbd>↓</kbd> _(giù)_
 | Premi il tasto `POWER`                            | <kbd>MOD</kbd>+<kbd>p</kbd>
 | Accendi                                    | _Click destro²_
 | Spegni lo schermo del dispositivo (continua a trasmettere)     | <kbd>MOD</kbd>+<kbd>o</kbd>
 | Accendi lo schermo del dispositivo                       | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>o</kbd>
 | Ruota lo schermo del dispositivo                        | <kbd>MOD</kbd>+<kbd>r</kbd>
 | Espandi il pannello delle notifiche                   | <kbd>MOD</kbd>+<kbd>n</kbd> \| _5° click³_
 | Espandi il pannello delle impostazioni                   | <kbd>MOD</kbd>+<kbd>n</kbd>+<kbd>n</kbd> \| _Doppio 5° click³_
 | Chiudi pannelli                 | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>n</kbd>
 | Copia negli appunti⁵                          | <kbd>MOD</kbd>+<kbd>c</kbd>
 | Taglia negli appunti⁵                           | <kbd>MOD</kbd>+<kbd>x</kbd>
 | Sincronizza gli appunti e incolla⁵           | <kbd>MOD</kbd>+<kbd>v</kbd>
 | Invia il testo degli appunti del computer              | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd>
 | Abilita/Disabilita il contatore FPS (su stdout)      | <kbd>MOD</kbd>+<kbd>i</kbd>
 | Pizzica per zoomare                               | <kbd>Ctrl</kbd>+_click e trascina_
 | Trascina file APK                        | Installa APK dal computer
 | Trascina file non-APK                    | [Trasferisci file verso il dispositivo](#push-file-to-device)

_¹Doppio click sui bordi neri per rimuoverli._  
_²Il tasto destro accende lo schermo se era spento, preme BACK in caso contrario._  
_³4° e 5° pulsante del mouse, se il tuo mouse ne dispone._  
_⁴Per le app native react in sviluppo, `MENU` attiva il menu di sviluppo._  
_⁵Solo in Android >= 7._

Le scorciatoie con pulsanti ripetuti sono eseguite rilasciando e premendo il pulsante una seconda volta. Per esempio, per eseguire "Espandi il pannello delle impostazioni":

1. Premi e tieni premuto <kbd>MOD</kbd>.
2. Poi premi due volte <kbd>n</kbd>.
3. Infine rilascia <kbd>MOD</kbd>.

Tutte le scorciatoie <kbd>Ctrl</kbd>+_tasto_ sono inoltrate al dispositivo, così sono gestite dall'applicazione attiva.

## Path personalizzati

Per utilizzare dei binari _adb_ specifici, configura il suo path nella variabile d'ambente `ADB`:

```bash
ADB=/percorso/per/adb scrcpy
```

Per sovrascrivere il percorso del file `scrcpy-server`, configura il percorso in `SCRCPY_SERVER_PATH`.

## Perchè _scrcpy_?

Un collega mi ha sfidato a trovare un nome tanto impronunciabile quanto [gnirehtet].

[`strcpy`] copia una **str**ing (stringa); `scrcpy` copia uno **scr**een (schermo).

[gnirehtet]: https://github.com/Genymobile/gnirehtet
[`strcpy`]: http://man7.org/linux/man-pages/man3/strcpy.3.html

## Come compilare?

Vedi [BUILD] (in inglese).


## Problemi comuni

Vedi le [FAQ](FAQ.it.md).


## Sviluppatori

Leggi la [pagina per sviluppatori].

[pagina per sviluppatori]: DEVELOP.md


## Licenza (in inglese)

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

## Articoli (in inglese)

- [Introducendo scrcpy][article-intro]
- [Scrcpy ora funziona wireless][article-tcpip]

[article-intro]: https://blog.rom1v.com/2018/03/introducing-scrcpy/
[article-tcpip]: https://www.genymotion.com/blog/open-source-project-scrcpy-now-works-wirelessly/

## Contatti

Se incontri un bug, per favore leggi prima le [FAQ](FAQ.it.md), poi apri una [issue].

[issue]: https://github.com/Genymobile/scrcpy/issues

Per domande generali o discussioni, puoi anche usare:

 - Reddit: [`r/scrcpy`](https://www.reddit.com/r/scrcpy)
 - Twitter: [`@scrcpy_app`](https://twitter.com/scrcpy_app)
