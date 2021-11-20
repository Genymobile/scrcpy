_Apri le [FAQ](FAQ.md) originali e sempre aggiornate._

# Domande Frequenti (FAQ)

Questi sono i problemi più comuni riportati e i loro stati.


## Problemi di `adb`

`scrcpy` esegue comandi `adb` per inizializzare la connessione con il dispositivo. Se `adb` fallisce, scrcpy non funzionerà.

In questo caso sarà stampato questo errore:

>     ERROR: "adb push" returned with value 1

Questo solitamente non è un bug di _scrcpy_, ma un problema del tuo ambiente.

Per trovare la causa, esegui:

```bash
adb devices
```

### `adb` not found (`adb` non trovato)

È necessario che `adb` sia accessibile dal tuo `PATH`.

In Windows, la cartella corrente è nel tuo `PATH` e `adb.exe` è incluso nella release, perciò dovrebbe già essere pronto all'uso.


### Device unauthorized (Dispositivo non autorizzato)

Controlla [stackoverflow][device-unauthorized] (in inglese).

[device-unauthorized]: https://stackoverflow.com/questions/23081263/adb-android-device-unauthorized


### Device not detected (Dispositivo non rilevato)

>     adb: error: failed to get feature set: no devices/emulators found

Controlla di aver abilitato correttamente il [debug con adb][enable-adb] (link in inglese).

Se il tuo dispositivo non è rilevato, potresti avere bisogno dei [driver][drivers] (link in inglese) (in Windows).

[enable-adb]: https://developer.android.com/studio/command-line/adb.html#Enabling
[drivers]: https://developer.android.com/studio/run/oem-usb.html


### Più dispositivi connessi

Se più dispositivi sono connessi, riscontrerai questo errore:

>     adb: error: failed to get feature set: more than one device/emulator

l'identificatore del tuo dispositivo deve essere fornito:

```bash
scrcpy -s 01234567890abcdef
```

Notare che se il tuo dispositivo è connesso mediante TCP/IP, riscontrerai questo messaggio:

>     adb: error: more than one device/emulator
>     ERROR: "adb reverse" returned with value 1
>     WARN: 'adb reverse' failed, fallback to 'adb forward'

Questo è un problema atteso (a causa di un bug di una vecchia versione di Android, vedi [#5] (link in inglese)), ma in quel caso scrcpy ripiega su un metodo differente, il quale dovrebbe funzionare.

[#5]: https://github.com/Genymobile/scrcpy/issues/5


### Conflitti tra versioni di adb

>     adb server version (41) doesn't match this client (39); killing...

L'errore compare quando usi più versioni di `adb` simultaneamente. Devi trovare il programma che sta utilizzando una versione differente di `adb` e utilizzare la stessa versione dappertutto.

Puoi sovrascrivere i binari di `adb` nell'altro programma, oppure chiedere a _scrcpy_ di usare un binario specifico di `adb`, impostando la variabile d'ambiente `ADB`:

```bash
set ADB=/path/to/your/adb
scrcpy
```


### Device disconnected (Dispositivo disconnesso)

Se _scrcpy_ si interrompe con l'avviso "Device disconnected", allora la connessione `adb` è stata chiusa.

Prova con un altro cavo USB o inseriscilo in un'altra porta USB. Vedi [#281] (in inglese) e [#283] (in inglese).

[#281]: https://github.com/Genymobile/scrcpy/issues/281
[#283]: https://github.com/Genymobile/scrcpy/issues/283



## Problemi di controllo

### Mouse e tastiera non funzionano

Su alcuni dispositivi potresti dover abilitare un opzione che permette l'[input simulato][simulating input] (link in inglese). Nelle opzioni sviluppatore, abilita:

> **Debug USB (Impostazioni di sicurezza)**
> _Permetti la concessione dei permessi e la simulazione degli input mediante il debug USB_
<!--- Ho tradotto personalmente il testo sopra, non conosco esattamente il testo reale --->

[simulating input]: https://github.com/Genymobile/scrcpy/issues/70#issuecomment-373286323


### I caratteri speciali non funzionano

Iniettare del testo in input è [limitato ai caratteri ASCII][text-input] (link in inglese). Un trucco permette di iniettare dei [caratteri accentati][accented-characters] (link in inglese), ma questo è tutto. Vedi [#37] (link in inglese).

[text-input]: https://github.com/Genymobile/scrcpy/issues?q=is%3Aopen+is%3Aissue+label%3Aunicode
[accented-characters]: https://blog.rom1v.com/2018/03/introducing-scrcpy/#handle-accented-characters
[#37]: https://github.com/Genymobile/scrcpy/issues/37


## Problemi del client

### La qualità è bassa

Se la definizione della finestra del tuo client è minore di quella del tuo dispositivo, allora potresti avere una bassa qualità di visualizzazione, specialmente individuabile nei testi (vedi [#40] (link in inglese)).

[#40]: https://github.com/Genymobile/scrcpy/issues/40

Per migliorare la qualità di ridimensionamento (downscaling), il filtro trilineare è applicato automaticamente se il renderizzatore è OpenGL e se supporta la creazione di mipmap.

In Windows, potresti voler forzare OpenGL:

```
scrcpy --render-driver=opengl
```

Potresti anche dover configurare il [comportamento di ridimensionamento][scaling behavior] (link in inglese):

> `scrcpy.exe` > Propietà > Compatibilità > Modifica impostazioni DPI elevati > Esegui l'override del comportamento di ridimensionamento DPI elevati >  Ridimensionamento eseguito per:  _Applicazione_.

[scaling behavior]: https://github.com/Genymobile/scrcpy/issues/40#issuecomment-424466723



### Crash del compositore KWin

In Plasma Desktop, il compositore è disabilitato mentre _scrcpy_ è in esecuzione.

Come soluzione alternativa, [disattiva la "composizione dei blocchi"][kwin] (link in inglese).
<!--- Non sono sicuro di aver tradotto correttamente la stringa di testo del pulsante --->

[kwin]: https://github.com/Genymobile/scrcpy/issues/114#issuecomment-378778613


## Crash

### Eccezione

Ci potrebbero essere molte ragioni. Una causa comune è che il codificatore hardware del tuo dispositivo non riesce a codificare alla definizione selezionata:

> ```
> ERROR: Exception on thread Thread[main,5,main]
> android.media.MediaCodec$CodecException: Error 0xfffffc0e
> ...
> Exit due to uncaughtException in main thread:
> ERROR: Could not open video stream
> INFO: Initial texture: 1080x2336
> ```

o

> ```
> ERROR: Exception on thread Thread[main,5,main]
> java.lang.IllegalStateException
>         at android.media.MediaCodec.native_dequeueOutputBuffer(Native Method)
> ```

Prova con una definizione inferiore:

```
scrcpy -m 1920
scrcpy -m 1024
scrcpy -m 800
```

Potresti anche provare un altro [codificatore](README.it.md#codificatore).


## Linea di comando in Windows

Alcuni utenti Windows non sono familiari con la riga di comando. Qui è descritto come aprire un terminale ed eseguire `scrcpy` con gli argomenti:

 1. Premi <kbd>Windows</kbd>+<kbd>r</kbd>, questo apre una finestra di dialogo.
 2. Scrivi `cmd` e premi <kbd>Enter</kbd>, questo apre un terminale.
 3. Vai nella tua cartella di _scrcpy_ scrivendo (adatta il percorso):

    ```bat
    cd C:\Users\user\Downloads\scrcpy-win64-xxx
    ```

    e premi <kbd>Enter</kbd>
 4. Scrivi il tuo comando. Per esempio:

    ```bat
    scrcpy --record file.mkv
    ```

Se pianifichi di utilizzare sempre gli stessi argomenti, crea un file `myscrcpy.bat` (abilita mostra [estensioni nomi file][show file extensions] per evitare di far confusione) contenente il tuo comando nella cartella di `scrcpy`. Per esempio:

```bat
scrcpy --prefer-text --turn-screen-off --stay-awake
```

Poi fai doppio click su quel file.

Potresti anche modificare (una copia di) `scrcpy-console.bat` o `scrcpy-noconsole.vbs` per aggiungere alcuni argomenti.

[show file extensions]: https://www.techpedia.it/14-windows/windows-10/171-visualizzare-le-estensioni-nomi-file-con-windows-10
