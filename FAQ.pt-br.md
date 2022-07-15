_Apenas o [FAQ](FAQ.md) original é garantido estar atualizado._

# Perguntas Frequentes

Aqui ficam os problemas mais comuns reportados e seus estados.

Se você encontrar algum erro, o primeiro passo é atualizar para a última versão.

## Problemas com `adb`

`scrcpy` executa comandos `adb` para iniciar a conexão com o dispositivo.
Se o `adb` falhar, então scrpcpy não irá funcionar.

Isso normalmente não é um bug no _scrcpy_, mas um problema em seu ambiente.

### `adb` não encontrado

Você precisa do `adb` acessível de seu `PATH`.

No Windows, o diretório atual está em seu `PATH`, e `adb.exe` está 
incluso no release, então deve funcionar sem esforço.

### Dispositivo não detectado

>     ERRO: Não foi possível encontrar nenhum dispositivo ADB 

Certifique-se que você habilitou corretamente a [Depuração USB][enable-adb].

Seu dispositivo deve ser detectado por `adb`:

```
adb devices
```

Se seu dispositivo não foi detectado, você pode precisar de alguns [drivers] (on Windows). Há um [Driver USB para dispositivos Google] [google-usb-driver].

[enable-adb]: https://developer.android.com/studio/command-line/adb.html#Enabling
[drivers]: https://developer.android.com/studio/run/oem-usb.html
[google-usb-driver]: https://developer.android.com/studio/run/win-usb


### Dispositivo não autorizado

>    ERRO: Dispositivo não autorizado:
>    ERROR:     -->   (usb)  0123456789abcdef          não autorizado
>    ERROR: Um popup deve abrir no dispositivo para pedir a autorização.

Quando estiver conectando, um popup deve abrir no dispositivo. Você deve autorizar a depuração USB.

Se não abrir, verifique [stackoverflow][device-unauthorized]

[device-unauthorized]: https://stackoverflow.com/questions/23081263/adb-android-device-unauthorized


### Diversos dispositivos conectados

Se houver diversos dispositivos conectados, você vai receber esse erro:

ERRO: Múltiplos (2) dispositivos ADB:
ERRO:     -->   (usb)  0123456789abcdef            dispositivo  Nexus_5
ERRO:     --> (tcpip)  192.168.1.5:5555            dispositivo  GM1913
ERRO: Selecione um dispositivo via -s (--serial), -d (--select-usb) ou -e (--select-tcpip) 

Neste caso, você pode tanto fornecer o identificador do dispositivo que quer espelhar:

```bash
scrcpy -s 0123456789abcdef
```

Ou solicitar o único dispositivo USB (ou TCP/IP):

```bash
scrcpy -d  # dispositivo USB
scrcpy -e  # dispositivo TCP/IP
```

Note que se seu dispositivo estiver conectado por TCP/IP, você pode receber essa mensagem:

>     adb: erro: mais de um dispositivo/emulador
>     ERRO: "adb reverse" retornou com valor 1
>     AVISO: 'adb reverse' falhou, voltando para 'adb forward'

Isso é esperado (devido a um bug em versões antigas do Android, veja [#5]), mas nesse caso, 
scrcpy tenta um método diferente, que deve funcionar.

[#5]: https://github.com/Genymobile/scrcpy/issues/5


### Conflitos entre versões do adb

>     versão do servidor adb (41) é diferente do cliente (39); finalizando...

Esse erro ocorre quando você usa diversas versões do `adb` simultaneamente. 
Você deve encontrar o programa que está utilizando uma versão diferente do `adb`, e utilizar a mesma para tudo.

Você pode subscrever o binário do `adb` no outro programa, ou informar ao _scrcpy_ para utilizar um binário específico do `adb`, ao configurar a variável do ambiente:

```bash
# no bash
export ADB=/caminho/para/o/adb
scrcpy
```

```cmd
:: no cmd
set ADB=C:\caminho\para\o\adb.exe
scrcpy
```

```powershell
# no PowerShell
$env:ADB = 'C:\caminho\para\o\adb.exe'
scrcpy
```


### Dispositivo desconectado

Se o _scrcpy_ parar com um aviso "Dispositivo desconectado", então a conexão com o `adb` foi finalizada.

Tente outro cabo USB ou conecte em outra porta USB. Veja [#281] e
[#283].

[#281]: https://github.com/Genymobile/scrcpy/issues/281
[#283]: https://github.com/Genymobile/scrcpy/issues/283



## Problema com os controles

### Mouse e teclado não funcionam

Em alguns dispositivos, você pode precisar habilitar uma opção para permitir [simular entradas][simulating_input].
Nas opções de desenvolvedor, habilite:

> **Depuração USB (Config. de segurança)**
> _Conceder permissões de acesso e simulação de entrada via depuração USB_

[simulating_input]: https://github.com/Genymobile/scrcpy/issues/70#issuecomment-373286323


### Caracteres especiais não funcionam

O método padrão de injeção de texto é [limitado a caracteres ASCII][text-input].
Um truque permite injetar alguns [caracteres acentuados][accented-characters],
mas isso é tudo. Veja [#37].

Desde scrcpy v1.20 no Linux, é possível simular um [teclado 
físico][hid] (HID).

[text-input]: https://github.com/Genymobile/scrcpy/issues?q=is%3Aopen+is%3Aissue+label%3Aunicode
[accented-characters]: https://blog.rom1v.com/2018/03/introducing-scrcpy/#handle-accented-characters
[#37]: https://github.com/Genymobile/scrcpy/issues/37
[hid]: README.md#physical-keyboard-simulation-hid


## Problemas no cliente

### Está com baixa qualidade

Se a definição da janela do seu cliente é menor que a tela de seu dispositivo, 
então você pode ter baixa qualidade, especialmente visível em texto (veja [#40]).

[#40]: https://github.com/Genymobile/scrcpy/issues/40

Esse problema deve ser resolvido no scrcpy v1.22: **atualize para a última versão**.

Em versões anteriores, você deve configurar o [comportamento de escala]:

> `scrcpy.exe` > Propriedades > Compatibilidade > Alterar configurações de DPI alto >
> Substituir o ajuste de DPI > Ajuste executado por: _Aplicativo_.

[scaling behavior]: https://github.com/Genymobile/scrcpy/issues/40#issuecomment-424466723

Também, para melhorar a qualidade do downscaling, o filtro trilinear é habilitado
automaticamente se o renderizador for OpenGL e se ele suporta mipmapping. 

No Windows, você pode querer forçar o OpenGL a habilitar mipmapping:

```
scrcpy --render-driver=opengl
```


### Problemas com Wayland

Por padrão, o SDL utiliza x11 no Linux. O [driver de vídeo][video-driver] pode ser alterado pela
variável de ambiente `SDL_VIDEODRIVER`:

[video-driver]: https://wiki.libsdl.org/FAQUsingSDL#how_do_i_choose_a_specific_video_driver

```bash
export SDL_VIDEODRIVER=wayland
scrcpy
```

Em algumas distribuições (pelo menos em Fedora), o pacote `libdecor` precisa ser
instalado manualmente.

Veja os problemas [#2554] e [#2559].

[#2554]: https://github.com/Genymobile/scrcpy/issues/2554
[#2559]: https://github.com/Genymobile/scrcpy/issues/2559


### Compositor KWin crashando

No Plasma Desktop, o compositor é desabilitado enquanto _scrcpy_ estiver rodando.

Como gambiarra, [desabilite "Composição de blocos"][kwin].

[kwin]: https://github.com/Genymobile/scrcpy/issues/114#issuecomment-378778613


## Crash

### Exceção

Pode haver diversas razões. A causa mais comum é de que o codificador de hardware 
de seu dispositivo não está conseguindo codificar dada a seguinte definição:

> ```
> ERROR: Exception on thread Thread[main,5,main]
> android.media.MediaCodec$CodecException: Error 0xfffffc0e
> ...
> Exit due to uncaughtException in main thread:
> ERROR: Could not open video stream
> INFO: Initial texture: 1080x2336
> ```

ou

> ```
> ERROR: Exception on thread Thread[main,5,main]
> java.lang.IllegalStateException
>         at android.media.MediaCodec.native_dequeueOutputBuffer(Native Method)
> ```

Tente com uma definição menor:

```
scrcpy -m 1920
scrcpy -m 1024
scrcpy -m 800
```

Desde o scrcpy v1.22, o scrcpy automaticamente tenta com uma menor definição
antes de falhar. Esse comportamento pode ser desabilitado com `--no-downsize-on-error`.

Você também pode tentar outro [codificador](README.md#encoder).

Se você encontrar essa exceção no Android 12, apenas atualize para o scrcpy >= 1.18 (veja [#2129]):

```
> ERROR: Exception on thread Thread[main,5,main]
java.lang.AssertionError: java.lang.reflect.InvocationTargetException
    at com.genymobile.scrcpy.wrappers.SurfaceControl.setDisplaySurface(SurfaceControl.java:75)
    ...
Caused by: java.lang.reflect.InvocationTargetException
    at java.lang.reflect.Method.invoke(Native Method)
    at com.genymobile.scrcpy.wrappers.SurfaceControl.setDisplaySurface(SurfaceControl.java:73)
    ... 7 more
Caused by: java.lang.IllegalArgumentException: displayToken must not be null
    at android.view.SurfaceControl$Transaction.setDisplaySurface(SurfaceControl.java:3067)
    at android.view.SurfaceControl.setDisplaySurface(SurfaceControl.java:2147)
    ... 9 more
```

[#2129]: https://github.com/Genymobile/scrcpy/issues/2129


## Linha de comando no Windows

Desde a v1.22, um "atalho" foi adicionado para abrir um terminal diretamente 
no diretório de seu scrcpy. Dê um duplo clique em `open_a_terminal_here.bat`, então digite seu 
comando. Por exemplo:

```
scrcpy --record file.mkv
```

Você também pode abrir o terminal e ir para a pasta do scrpcpy manualmente:

 1. Pressione <kbd>Windows</kbd>+<kbd>r</kbd>, isso abre a caixa de diálogo.
 2. Digite `cmd` e pressione <kbd>Enter</kbd>, isso abre um terminal.
 3. Vá para o diretório do _scrcpy_, digitando (altere o caminho):

    ```bat
    cd C:\Users\user\Downloads\scrcpy-win64-xxx
    ```

    e pressione <kbd>Enter</kbd>
 4. Digite seu comando. Por exemplo:

    ```bat
    scrcpy --record file.mkv
    ```

Se você planeja utilizar sempre os mesmos argumentos, crie um arquivo `myscrcpy.bat`
(habilite [Extensões de nome de arquivos][show-file-extensions] para evitar confusão) no diretório do `scrcpy`, contendo seu comando. Por exemplo:

```bat
scrcpy --prefer-text --turn-screen-off --stay-awake
```

E então dê um duplo clique no arquivo.

Você também pode editar (a cópia de) `scrcpy-console.bat` ou `scrcpy-noconsole.vbs` 
para adicionar alguns argumentos. 

[show-file-extensions]: https://www.howtogeek.com/205086/beginner-how-to-make-windows-show-file-extensions/