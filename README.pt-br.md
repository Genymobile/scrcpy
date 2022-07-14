_Apenas o [README](README.md) original é garantido estar atualizado._

# scrcpy (v1.24)

Esta aplicação fornece exibição e controle de dispositivos Android conectados via
USB (ou [via TCP/IP][article-tcpip]). Não requer nenhum acesso _root_.
Funciona em _GNU/Linux_, _Windows_ e _macOS_.

![screenshot](assets/screenshot-debian-600.jpg)

Foco em:

 - **leveza** (nativo, mostra apenas a tela do dispositivo)
 - **performance** (30~120fps, dependendo do dispositivo)
 - **qualidade** (1920×1080 ou acima)
 - **baixa latência** ([35~70ms][lowlatency])
 - **baixo tempo de inicialização** (~1 segundo para mostrar a primeira imagem)
 - **não intrusivo** (nada é deixado instalado no dispositivo)

[lowlatency]: https://github.com/Genymobile/scrcpy/pull/646


## Requisitos

O dispositivo Android requer pelo menos a API 21 (Android 5.0).

Tenha certeza de ter [ativado a depuração adb][enable-adb] no(s) seu(s) dispositivo(s).

[enable-adb]: https://developer.android.com/studio/command-line/adb.html#Enabling

Em alguns dispositivos, você também precisa ativar [uma opção adicional][control] para
controlá-lo usando teclado e mouse.

[control]: https://github.com/Genymobile/scrcpy/issues/70#issuecomment-373286323


## Obter o app

<a href="https://repology.org/project/scrcpy/versions"><img src="https://repology.org/badge/vertical-allrepos/scrcpy.svg" alt="Packaging status" align="right"></a>

### Sumário

 - Linux: `apt install scrcpy`
 - Windows: [baixar][direct-win64]
 - macOS: `brew install scrcpy`

  Compilar pelos arquivos fontes: [BUILD] ([processo simplificado][BUILD_simple])

[BUILD]: BUILD.md
[BUILD_simple]: BUILD.md#simple


### Linux

No Debian e Ubuntu:

```
apt install scrcpy
```

No Arch Linux:

```
pacman -S scrcpy
```

Um pacote [Snap] está disponível: [`scrcpy`][snap-link].

[snap-link]: https://snapstats.org/snaps/scrcpy

[snap]: https://en.wikipedia.org/wiki/Snappy_(package_manager)

Para Fedora, um pacote [COPR] está disponível: [`scrcpy`][copr-link].

[COPR]: https://fedoraproject.org/wiki/Category:Copr
[copr-link]: https://copr.fedorainfracloud.org/coprs/zeno/scrcpy/

Para Gentoo, uma [Ebuild] está disponível: [`scrcpy/`][ebuild-link].

[Ebuild]: https://wiki.gentoo.org/wiki/Ebuild
[ebuild-link]: https://github.com/maggu2810/maggu2810-overlay/tree/master/app-mobilephone/scrcpy

Você também pode [compilar o app manualmente][BUILD] ([processo simplificado][BUILD_simple]).


### Windows

Para Windows, por simplicidade, um arquivo pré-compilado com todas as dependências
(incluindo `adb`) está disponível:

 - [README](README.md#windows)

Também está disponível em [Chocolatey]:

[Chocolatey]: https://chocolatey.org/

```bash
choco install scrcpy
choco install adb    # se você ainda não o tem
```

E no [Scoop]:

```bash
scoop install scrcpy
scoop install adb    # se você ainda não o tem
```

[Scoop]: https://scoop.sh

Você também pode [compilar o app manualmente][BUILD].


### macOS

A aplicação está disponível em [Homebrew]. Apenas instale-a:

[Homebrew]: https://brew.sh/

```bash
brew install scrcpy
```

Você precisa do `adb`, acessível pelo seu `PATH`. Se você ainda não o tem:

```bash
brew install android-platform-tools
```

Está também disponivel em [MacPorts], que prepara o adb para você:

```bash
sudo port install scrcpy
```

[MacPorts]: https://www.macports.org/


Você também pode [compilar o app manualmente][BUILD].


## Executar

Conecte um dispositivo Android e execute:

```bash
scrcpy
```

Também aceita argumentos de linha de comando, listados por:

```bash
scrcpy --help
```

## Funcionalidades

### Configuração de captura

#### Reduzir tamanho

Algumas vezes, é útil espelhar um dispositivo Android em uma resolução menor para
aumentar a performance.

Para limitar ambos (largura e altura) para algum valor (ex: 1024):

```bash
scrcpy --max-size 1024
scrcpy -m 1024  # versão curta
```

A outra dimensão é calculada para que a proporção do dispositivo seja preservada.
Dessa forma, um dispositivo de 1920x1080 será espelhado em 1024x576.


#### Mudar bit-rate

O bit-rate padrão é 8 Mbps. Para mudar o bit-rate do vídeo (ex: para 2 Mbps):

```bash
scrcpy --bit-rate 2M
scrcpy -b 2M  # versão curta
```

#### Limitar frame rate

O frame rate de captura pode ser limitado:

```bash
scrcpy --max-fps 15
```

Isso é oficialmente suportado desde o Android 10, mas pode funcionar em versões anteriores.

O frame rate atual pode ser exibido no console:

```
scrcpy --print-fps
```

E pode ser desabilitado a qualquer momento com <kbd>MOD</kbd>+<kbd>i</kbd>.

#### Cortar

A tela do dispositivo pode ser cortada para espelhar apenas uma parte da tela.

Isso é útil por exemplo, para espelhar apenas um olho do Oculus Go:

```bash
scrcpy --crop 1224:1440:0:0   # 1224x1440 no deslocamento (0,0)
```

Se `--max-size` também for especificado, o redimensionamento é aplicado após o corte.


#### Travar orientação do vídeo


Para travar a orientação do espelhamento:

```bash
scrcpy --lock-video-orientation     # orientação inicial (Atual)
scrcpy --lock-video-orientation=0   # orientação natural
scrcpy --lock-video-orientation=1   # 90° sentido anti-horário
scrcpy --lock-video-orientation=2   # 180°
scrcpy --lock-video-orientation=3   # 90° sentido horário
```

Isso afeta a orientação de gravação.

A [janela também pode ser rotacionada](#rotação) independentemente.


#### Encoder

Alguns dispositivos têm mais de um encoder, e alguns deles podem causar problemas ou
travar. É possível selecionar um encoder diferente:

```bash
scrcpy --encoder OMX.qcom.video.encoder.avc
```

Para listar os encoders disponíveis, você pode passar um nome de encoder inválido, o
erro dará os encoders disponíveis:

```bash
scrcpy --encoder _
```

### Captura

#### Gravando

É possível gravar a tela enquanto ocorre o espelhamento:

```bash
scrcpy --record file.mp4
scrcpy -r file.mkv
```

Para desativar o espelhamento durante a gravação:

```bash
scrcpy --no-display --record file.mp4
scrcpy -Nr file.mkv
# interrompa a gravação com Ctrl+C
```

"Frames pulados" são gravados, mesmo que não sejam exibidos em tempo real (por
motivos de performance). Frames têm seu _horário carimbado_ no dispositivo, então [variação de atraso nos pacotes] não impacta o arquivo gravado.

[packet delay variation]: https://en.wikipedia.org/wiki/Packet_delay_variation


#### v4l2loopback

Em Linux, é possível enviar a transmissão do video para um disposiivo v4l2 loopback, assim
o dispositivo Android pode ser aberto como uma webcam por qualquer ferramenta capaz de v4l2

The module `v4l2loopback` must be installed:

```bash
sudo apt install v4l2loopback-dkms
```

Para criar um dispositivo v4l2:

```bash
sudo modprobe v4l2loopback
```

Isso criara um novo dispositivo de vídeo em `/dev/videoN`, onde `N` é uma integer
(mais [opções](https://github.com/umlaeute/v4l2loopback#options) estão disponiveis
para criar varios dispositivos ou dispositivos com IDs específicas).

Para listar os dispositivos disponíveis:

```bash
# requer o pacote v4l-utils
v4l2-ctl --list-devices

# simples, mas pode ser suficiente
ls /dev/video*
```

Para iniciar o `scrcpy` usando o coletor v4l2 (sink):

```bash
scrcpy --v4l2-sink=/dev/videoN
scrcpy --v4l2-sink=/dev/videoN --no-display  # desativa a janela espelhada
scrcpy --v4l2-sink=/dev/videoN -N            # versão curta
```

(troque `N` pelo ID do dipositivo, verifique com `ls /dev/video*`)

Uma vez ativado, você pode abrir suas trasmissões de videos com uma ferramenta capaz de v4l2:

```bash
ffplay -i /dev/videoN
vlc v4l2:///dev/videoN   # VLC pode adicionar um pouco de atraso de buffering
```

Por exemplo, você pode capturar o video dentro do [OBS].

[OBS]: https://obsproject.com/


#### Buffering

É possivel adicionar buffering. Isso aumenta a latência, mas reduz a tenção (jitter) (veja
[#2464]).

[#2464]: https://github.com/Genymobile/scrcpy/issues/2464

A opção éta disponivel para buffering de exibição:

```bash
scrcpy --display-buffer=50  # adiciona 50 ms de buffering para a exibição
```

e coletor V4L2:

```bash
scrcpy --v4l2-buffer=500    # adiciona 500 ms de buffering para coletor V4L2
```

,
### Conexão

#### Sem fio

_Scrcpy_ usa `adb` para se comunicar com o dispositivo, e `adb` pode [conectar-se][connect] a um
dispositivo via TCP/IP. O dispositivo precisa estar conectado na mesma rede 
que o computador.

#### Automático

A opção `--tcpip` permite configurar a conexão automaticamente. Há duas 
variantes.

Se o dispositivo (acessível em 192.168.1.1 neste exemplo) escuta em uma porta
(geralmente 5555) para conexões _adb_ de entrada, então execute:

```bash
scrcpy --tcpip=192.168.1.1       # porta padrão é 5555
scrcpy --tcpip=192.168.1.1:5555
```

Se o modo TCP/IP do _adb_ estiver desabilitado no dispositivo (ou se você não 
souber o endereço de IP), conecte-o via USB e execute:

```bash
scrcpy --tcpip    # sem argumentos
```

Ele vai encontrar o endereço de IP do dispositivo automaticamente, habilitar 
o modo TCP/IP, e então, conectar-se ao dispositivo antes de iniciar.

#### Manual

1. Conecte o dispositivo em uma porta USB do seu computador.
2. Conecte o dispositivo no mesmo Wi-Fi do seu computador.
3. Pegue o endereço IP do seu dispositivo, em Configurações → Sobre o telefone → Status, ou
   executando este comando:

    ```bash
    adb shell ip route | awk '{print $9}'
    ```

4. Ative o adb via TCP/IP no seu dispositivo: `adb tcpip 5555`.
5. Desconecte seu dispositivo.
6. Conecte-se ao seu dispositivo: `adb connect DEVICE_IP:5555` _(substitua `DEVICE_IP`)_.
7. Execute `scrcpy` como de costume.

Desde o Android 11, a opção [Depuração por Wi-Fi][adb-wireless]

[adb-wireless]: https://developer.android.com/studio/command-line/adb#connect-to-a-device-over-wi-fi-android-11+

Se a conexão cair aleatoriamente, execute o comando `scrcpy` para reconectar. Se disser 
que não encontrou dispositivos/emuladores, tente executar `adb connect DEVICE_IP:5555` novamente, e então `scrcpy` como de costume. Se ainda disser 
que não encontrou nada, tente executar `adb disconnect`, e então execute os dois comandos novamente.

Pode ser útil diminuir o bit-rate e a resolução:

```bash
scrcpy --bit-rate 2M --max-size 800
scrcpy -b2M -m800  # versão curta
```

[connect]: https://developer.android.com/studio/command-line/adb.html#wireless


#### Múltiplos dispositivos

Se vários dispositivos são listados em `adb devices`, você deve especificar o _serial_:

```bash
scrcpy --serial 0123456789abcdef
scrcpy -s 0123456789abcdef  # versão curta
```

O serial também pode ser consultado pela variável de ambiente `ANDROID _SERIAL`
(também utilizado por `adb`)

Se o dispositivo está conectado via TCP/IP:

```bash
scrcpy --serial 192.168.0.1:5555
scrcpy -s 192.168.0.1:5555  # versão curta
```

Se somente um dispositivo está conectado via USB ou TCP/IP, é possível 
selecioná-lo automaticamente:

```bash
# Seleciona o único dispositivo conectado via USB
scrcpy -d             # assim como adb -d
scrcpy --select-usb   # método extenso

# Seleciona o único dispositivo conectado via TCP/IP`
scrcpy -e             # assim como adb -e
scrcpy --select-tcpip # método extenso

Você pode iniciar várias instâncias do _scrcpy_ para vários dispositivos.

#### Iniciar automaticamente quando dispositivo é conectado

Você pode usar [AutoAdb]:

```bash
autoadb scrcpy -s '{}'
```

[AutoAdb]: https://github.com/rom1v/autoadb

#### Túnel SSH

Para conectar-se a um dispositivo remoto, é possível conectar um cliente `adb` local a
um servidor `adb` remoto (contanto que eles usem a mesma versão do protocolo
_adb_)

#### Servidor ADB remoto

Para se conectar a um _adb server_ remoto, o servidor precisa escutar em todas as interfaces:

```bash
adb kill-server
adb -a nodaemon server start
# mantenha isso aberto
```

**Aviso: todas a comunicações entre clientes e o _adb server_ não são criptografadas.**

Supondo que o servidor esteja acessível em 192.168.1.2. Então, em outro 
terminal, execute `scrcpy`:

```bash
# no bash
export ADB_server_SOCKET=tcp:192.168.1.2:5037
scrcpy --tunnel-host=192.168.1.2
```

```cmd
:: in cmd
set ADB_SERVER_SOCKET=tcp:192.168.1.2:5037
scrcpy --tunnel-host=192.168.1.2
```

```powershell
# in PowerShell
$env:ADB_SERVER_SOCKET = 'tcp:192.168.1.2:5037'
scrcpy --tunnel-host=192.168.1.2
```

Por padrão, `scrcpy` usa a porta local utilizada por `adb forward` no estabelecimento de tunelagem (Tipicamente `27183`, veja `--port`). Também
é possível forçar uma porta de tunelagem diferente (pode ser útil em 
situações mais complexas, quando há mais redirecionamentos envolvidos):

```
scrcpy --tunnel-port=1234
```

#### Túnel SSH

Para se comunicar a um _adb server_ remoto de um jeito seguro, é 
preferível utilizar um túnel SSH.

Primeiro, tenha certeza de que o _adb server_ esteja rodando em um 
computador remoto:

```bash
adb start-server
```

Então, estabeleça um túnel SSH:

```bash
# local  5038 --> remoto  5037
# local 27183 <-- remoto 27183

ssg -CN -L5038:localhost:5037 -R27183:localhost:27183 seu_computador_remoto
# mantenha isso aberto
```

De outro terminal, execute `scrcpy`:

```bash
# in bash
export ADB_SERVER_SOCKET=tcp:localhost:5038
scrcpy
```

```cmd
:: in cmd
set ADB_SERVER_SOCKET=tcp:localhost:5038
scrcpy
```

```powershell
# in PowerShell
$env:ADB_SERVER_SOCKET = 'tcp:localhost:5038'
scrcpy
```

Para evitar ativar o encaminhamento de porta remota, você pode forçar uma conexão
de encaminhamento (note o `-L` em vez de `-R`):

```bash
# local  5038 --> remoto  5037
# local 27183 <-- remoto 27183
ssh -CN -L5037:localhost:5037 -L27183:localhost:27183 seu_computador_remoto
# mantenha isso aberto
```

De outro terminal, execute `scrcpy`:

```bash
# no bash
export ADB_SERVER_SOCKET=tcp:localhost:5038
scrcpy --force-adb-forward
```

```cmd
:: in cmd
set ADB_SERVER_SOCKET=tcp:localhost:5038
scrcpy --force-adb-forward
```

```powershell
# no PowerShell
$env:ADB_SERVER_SOCKET = 'tcp:localhost:5038'
scrcpy --force-adb-forward
```


Igual a conexões sem fio, pode ser útil reduzir a qualidade:

```
scrcpy -b2M -m800 --max-fps 15
```

### Configuração de janela

#### Título

Por padrão, o título da janela é o modelo do dispositivo. Isso pode ser mudado:

```bash
scrcpy --window-title 'Meu dispositivo'
```

#### Posição e tamanho

A posição e tamanho iniciais da janela podem ser especificados:

```bash
scrcpy --window-x 100 --window-y 100 --window-width 800 --window-height 600
```

#### Sem bordas

Para desativar decorações de janela:

```bash
scrcpy --window-borderless
```

#### Sempre no topo

Para manter a janela do scrcpy sempre no topo:

```bash
scrcpy --always-on-top
```

#### Tela cheia

A aplicação pode ser iniciada diretamente em tela cheia:

```bash
scrcpy --fullscreen
scrcpy -f  # versão curta
```

Tela cheia pode ser alternada dinamicamente com <kbd>MOD</kbd>+<kbd>f</kbd>.

#### Rotação

A janela pode ser rotacionada:

```bash
scrcpy --rotation 1
```

Valores possíveis são:
 - `0`: sem rotação
 - `1`: 90 graus sentido anti-horário
 - `2`: 180 graus
 - `3`: 90 graus sentido horário

A rotação também pode ser mudada dinamicamente com <kbd>MOD</kbd>+<kbd>←</kbd>
_(esquerda)_ e <kbd>MOD</kbd>+<kbd>→</kbd> _(direita)_.

Note que _scrcpy_ controla 3 rotações diferentes:
 - <kbd>MOD</kbd>+<kbd>r</kbd> requisita ao dispositivo para mudar entre retrato
   e paisagem (a aplicação em execução pode se recusar, se ela não suporta a
   orientação requisitada).
 - [`--lock-video-orientation`](#travar-orientação-do-vídeo) muda a orientação de
   espelhamento (a orientação do vídeo enviado pelo dispositivo para o
   computador). Isso afeta a gravação.
 - `--rotation` (ou <kbd>MOD</kbd>+<kbd>←</kbd>/<kbd>MOD</kbd>+<kbd>→</kbd>)
   rotaciona apenas o conteúdo da janela. Isso afeta apenas a exibição, não a
   gravação.


### Outras opções de espelhamento

#### Apenas leitura

Para desativar controles (tudo que possa interagir com o dispositivo: teclas de entrada,
eventos de mouse, arrastar e soltar arquivos):

```bash
scrcpy --no-control
scrcpy -n
```

#### Display

Se vários displays estão disponíveis, é possível selecionar o display para
espelhar:

```bash
scrcpy --display 1
```

A lista de IDs dos displays pode ser obtida por:

```bash
adb shell dumpsys display   # busca "mDisplayId=" na saída
```

O display secundário pode apenas ser controlado se o dispositivo roda pelo menos Android
10 (caso contrário é espelhado como apenas leitura).


#### Permanecer ativo

Para evitar que o dispositivo seja suspenso após um delay quando o dispositivo é conectado:

```bash
scrcpy --stay-awake
scrcpy -w
```

O estado inicial é restaurado quando o _scrcpy_ é fechado.


#### Desligar tela

É possível desligar a tela do dispositivo durante o início do espelhamento com uma
opção de linha de comando:

```bash
scrcpy --turn-screen-off
scrcpy -S
```

Ou apertando <kbd>MOD</kbd>+<kbd>o</kbd> a qualquer momento.

Para ligar novamente, pressione <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>o</kbd>.

No Android, o botão de `POWER` sempre liga a tela. Por conveniência, se
`POWER` é enviado via scrcpy (via clique-direito ou <kbd>MOD</kbd>+<kbd>p</kbd>), ele
forçará a desligar a tela após um delay pequeno (numa base de melhor esforço).
O botão `POWER` físico ainda causará a tela ser ligada.

Também pode ser útil evitar que o dispositivo seja suspenso:

```bash
scrcpy --turn-screen-off --stay-awake
scrcpy -Sw
```

#### Desligar ao fechar

Para desligar a tela do dispositivo ao fechar _scrcpy_:

```bash
scrcpy --power-off-on-close
```

#### Ligar ao iniciar

Por padrão, ao iniciar, o dispositivo é ligado.

Para prevenir esse comportamento:

```bash
scrcpy --no-power-on
```



#### Mostrar toques

Para apresentações, pode ser útil mostrar toques físicos (no dispositivo
físico).

Android fornece esta funcionalidade nas _Opções do desenvolvedor_.

_Scrcpy_ fornece esta opção de ativar esta funcionalidade no início e restaurar o
valor inicial no encerramento:

```bash
scrcpy --show-touches
scrcpy -t
```

Note que isto mostra apenas toques _físicos_ (com o dedo no dispositivo).


#### Desativar descanso de tela

Por padrão, _scrcpy_ não evita que o descanso de tela rode no computador.

Para desativá-lo:

```bash
scrcpy --disable-screensaver
```


### Controle de entrada

#### Rotacionar a tela do dispositivo

Pressione <kbd>MOD</kbd>+<kbd>r</kbd> para mudar entre os modos retrato e
paisagem.

Note que só será rotacionado se a aplicação em primeiro plano suportar a
orientação requisitada.

#### Copiar-colar

Sempre que a área de transferência do Android muda, é automaticamente sincronizada com a
área de transferência do computador.

Qualquer atalho com <kbd>Ctrl</kbd> é encaminhado para o dispositivo. Em particular:
 - <kbd>Ctrl</kbd>+<kbd>c</kbd> tipicamente copia
 - <kbd>Ctrl</kbd>+<kbd>x</kbd> tipicamente recorta
 - <kbd>Ctrl</kbd>+<kbd>v</kbd> tipicamente cola (após a sincronização de área de transferência
   computador-para-dispositivo)

Isso tipicamente funciona como esperado.

O comportamento de fato depende da aplicação ativa, no entanto. Por exemplo,
_Termux_ envia SIGINT com <kbd>Ctrl</kbd>+<kbd>c</kbd>, e _K-9 Mail_
compõe uma nova mensagem.

Para copiar, recortar e colar em tais casos (mas apenas suportado no Android >= 7):
 - <kbd>MOD</kbd>+<kbd>c</kbd> injeta `COPY`
 - <kbd>MOD</kbd>+<kbd>x</kbd> injeta `CUT`
 - <kbd>MOD</kbd>+<kbd>v</kbd> injeta `PASTE` (após a sincronização de área de transferência
   computador-para-dispositivo)

Em adição, <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd> permite injetar o
texto da área de transferência do computador como uma sequência de eventos de tecla. Isso é útil quando o
componente não aceita colar texto (por exemplo no _Termux_), mas pode
quebrar conteúdo não-ASCII.

**ADVERTÊNCIA:** Colar a área de transferência do computador para o dispositivo (tanto via
<kbd>Ctrl</kbd>+<kbd>v</kbd> quanto <kbd>MOD</kbd>+<kbd>v</kbd>) copia o conteúdo
para a área de transferência do dispositivo. Como consequência, qualquer aplicação Android pode ler
o seu conteúdo. Você deve evitar colar conteúdo sensível (como senhas) dessa
forma.

Alguns dispositivos não se comportam como esperado quando a área de transferência é definida
programaticamente. Uma opção `--legacy-paste` é fornecida para mudar o comportamento
de <kbd>Ctrl</kbd>+<kbd>v</kbd> e <kbd>MOD</kbd>+<kbd>v</kbd> para que eles
também injetem o texto da área de transferência do computador como uma sequência de eventos de tecla (da mesma
forma que <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd>).

Para desabilitar a sincronização automática da área de transferência, utilize
`--no-clipboard-autosync`

#### Pinçar para dar zoom

Para simular "pinçar para dar zoom": <kbd>Ctrl</kbd>+_clicar-e-mover_.

Mais precisamente, segure <kbd>Ctrl</kbd> enquanto pressiona o botão de clique-esquerdo. Até que
o botão de clique-esquerdo seja liberado, todos os movimentos do mouse ampliar e rotacionam o
conteúdo (se suportado pelo app) relativo ao centro da tela.

Concretamente, _scrcpy_ gera eventos adicionais de toque de um "dedo virtual" em
uma posição invertida em relação ao centro da tela.

#### Simulação de teclado físico (HID)

Por padrão, _scrcpy_ utiliza injeção de chave ou texto: funciona em qualquer lugar,
mas é limitado a ASCII.

Alternativamente, `scrcpy` pode simular um teclado USB físico no Android
para servir uma melhor experiência de entrada de teclas (utilizando [USB HID por AOAv2][hid-aoav2]): 
o teclado virtual é desabilitado e funciona para todos os caractéres e IME.

[hid-aoav2]: https://source.android.com/devices/accessories/aoa2#hid-support

Porém, só funciona se o dispositivo estiver conectado via USB.

Nota: No Windows, pode funcionar somente no [modo OTG](#otg), não esquanto espelhando (não 
é possível abrir um dispositivo USB se ele já estiver em uso por outro processo
como o  _adb daemon_).

Para habilitar esse modo:

```bash
scrcpy --hid-keyboard
scrcpy -K  # versão curta
```

Se falhar por algum motivo (por exemplo, se o dispositivo não estiver conectado via
USB), ele automaticamente retorna para o modo padrão (com um log no console). Isso permite utilizar as mesmas linhas de comando de quando conectado por
USB e TCP/IP.

Neste modo, eventos de tecla bruta (scancodes) são enviados ao dispositivo, independentemente
do mapeamento de teclas do hospedeiro. Portanto, se o seu layout de teclas não é igual, ele
precisará ser configurado no dispositivo Android, em Configurações → Sistema → Idiomas e entrada → [Teclado_físico].

Essa página de configurações pode ser aberta diretamente:

```bash
adb shell am start -a android.settings.HARD_KEYBOARD_SETTINGS
```

Porém, essa opção só está disponível quando o teclado HID está habilitado (ou quando
um teclado físico é conectado).

[Teclado_físico]: https://github.com/Genymobile/scrcpy/pull/2632#issuecomment-923756915

#### Simulação de mouse físico (HID)

Similar a simulação de teclado físico, é possível simular um mouse físico. Da mesma forma, só funciona se o dispositivo está conectado por USB.

Por padrão, _scrcpy_ utilizada injeção de eventos do mouse com coordenadas absolutas. Ao simular um mouse físico, o ponteiro de mouse aparece no
dispositivo Android, e o movimento relativo do mouse, cliques e scrolls são injetados.

Para habilitar esse modo:

```bash
scrcpy --hid-mouse
scrcpy -M  # versão curta
```

Você também pode adicionar `--forward-all-clicks` para [enviar todos os botões do mouse][forward_all_clicks].

[forward_all_clicks]: #clique-direito-e-clique-do-meio

Quando esse modo é habilitado, o mouse do computador é "capturado" (o ponteiro do mouse
desaparece do computador e aparece no dispositivo Android).

Teclas especiais de captura, como <kbd>Alt</kbd> ou <kbd>Super</kbd>, alterna
(desabilita e habilita) a captura do mouse. Utilize uma delas para dar o controle
do mouse de volta para o computador.


#### OTG

É possível executar _scrcpy_ somente com simulação de mouse e teclado físicos
(HID), como se o teclado e mouse do computador estivessem conectados diretamente ao dispositivo
via cabo OTG.

Nesse modo, `adb` (Depuração USB) não é necessária, e o espelhamento é desabilitado.

Para habilitar o modo OTG:

```bash
scrcpy --otg
# Passe o serial de houver diversos dispositivos USB disponíveis
scrcpy --otg -s 0123456789abcdef
```

É possível habilitar somente teclado HID ou mouse HID:

```bash
scrcpy --otg --hid-keyboard              # somente teclado
scrcpy --otg --hid-mouse                 # somente mouse
scrcpy --otg --hid-keyboard --hid-mouse  # teclado e mouse
# para conveniência, habilite ambos por padrão
scrcpy --otg                             # teclado e mouse
```

Como `--hid-keyboard` e `--hid-mouse`, só funciona se o dispositivo estiver conectado por USB.


#### Preferência de injeção de texto

Existem dois tipos de [eventos][textevents] gerados ao digitar um texto:
 - _eventos de tecla_, sinalizando que a tecla foi pressionada ou solta;
 - _eventos de texto_, sinalizando que o texto foi inserido.

Por padrão, letras são injetadas usando eventos de tecla, assim o teclado comporta-se
como esperado em jogos (normalmente para teclas WASD).

Mas isso pode [causar problemas][prefertext]. Se você encontrar tal problema, você
pode evitá-lo com:

```bash
scrcpy --prefer-text
```

(mas isso vai quebrar o comportamento do teclado em jogos)

Ao contrário, você pode forçar para sempre injetar eventos de teclas brutas:

```bash
scrcpy --raw-key-events
```

Essas opções não surgem efeito no teclado HID (todos os eventos de tecla são enviados como scancodes nesse modo).

[textevents]: https://blog.rom1v.com/2018/03/introducing-scrcpy/#handle-text-input
[prefertext]: https://github.com/Genymobile/scrcpy/issues/650#issuecomment-512945343


#### Repetir tecla

Por padrão, segurar uma tecla gera eventos de tecla repetidos. Isso pode causar
problemas de performance em alguns jogos, onde esses eventos são inúteis de qualquer forma.

Para evitar o encaminhamento eventos de tecla repetidos:

```bash
scrcpy --no-key-repeat
```

Essa opção não surge efeito no teclado HID (repetição de tecla é gerida diretamente pelo 
Android nesse modo).

#### Clique-direito e clique-do-meio

Por padrão, clique-direito dispara BACK (ou POWER) e clique-do-meio dispara
HOME. Para desabilitar esses atalhos e encaminhar os cliques para o dispositivo:

```bash
scrcpy --forward-all-clicks
```


### Soltar arquivo

#### Instalar APK

Para instalar um APK, arraste e solte o arquivo APK (com extensão `.apk`) na janela
_scrcpy_.

Não existe feedback visual, um log é imprimido no console.


#### Enviar arquivo para dispositivo

Para enviar um arquivo para `/sdcard/Download/` no dispositivo, arraste e solte um arquivo (não-APK) para a
janela do _scrcpy_.

Não existe feedback visual, um log é imprimido no console.

O diretório alvo pode ser mudado ao iniciar:

```bash
scrcpy --push-target=/sdcard/Movies/
```


### Encaminhamento de áudio

Áudio não é encaminhado pelo _scrcpy_. Use [sndcpy].

Também veja [issue_#14].

[sndcpy]: https://github.com/rom1v/sndcpy
[issue_#14]: https://github.com/Genymobile/scrcpy/issues/14


## Atalhos

Na lista a seguir, <kbd>MOD</kbd> é o modificador de atalho. Por padrão, é
<kbd>Alt</kbd> (esquerdo) ou <kbd>Super</kbd> (esquerdo).

Ele pode ser mudado usando `--shortcut-mod`. Possíveis teclas são `lctrl`, `rctrl`,
`lalt`, `ralt`, `lsuper` e `rsuper`. Por exemplo:

```bash
# usar RCtrl para atalhos
scrcpy --shortcut-mod=rctrl

# usar tanto LCtrl+LAlt quanto LSuper para atalhos
scrcpy --shortcut-mod=lctrl+lalt,lsuper
```

_<kbd>[Super]</kbd> é tipicamente a tecla <kbd>Windows</kbd> ou <kbd>Cmd</kbd>._

[Super]: https://en.wikipedia.org/wiki/Super_key_(keyboard_button)

 | Ação                                        |   Atalho
 | ------------------------------------------- |:-----------------------------
 | Mudar modo de tela cheia                    | <kbd>MOD</kbd>+<kbd>f</kbd>
 | Rotacionar display para esquerda            | <kbd>MOD</kbd>+<kbd>←</kbd> _(esquerda)_
 | Rotacionar display para direita             | <kbd>MOD</kbd>+<kbd>→</kbd> _(direita)_
 | Redimensionar janela para 1:1 (pixel-perfeito) | <kbd>MOD</kbd>+<kbd>g</kbd>
 | Redimensionar janela para remover bordas pretas | <kbd>MOD</kbd>+<kbd>w</kbd> \| _Clique-duplo-esquerdo¹_
 | Clicar em `HOME`                            | <kbd>MOD</kbd>+<kbd>h</kbd> \| _Clique-do-meio_
 | Clicar em `BACK`                            | <kbd>MOD</kbd>+<kbd>b</kbd> \| _Clique-direito²_
 | Clicar em `APP_SWITCH`                      | <kbd>MOD</kbd>+<kbd>s</kbd> \| _Clique-do-4.°³_
 | Clicar em `MENU` (desbloquear tela)         | <kbd>MOD</kbd>+<kbd>m</kbd>
 | Clicar em `VOLUME_UP`                       | <kbd>MOD</kbd>+<kbd>↑</kbd> _(cima)_
 | Clicar em `VOLUME_DOWN`                     | <kbd>MOD</kbd>+<kbd>↓</kbd> _(baixo)_
 | Clicar em `POWER`                           | <kbd>MOD</kbd>+<kbd>p</kbd>
 | Ligar                                       | _Clique-direito²_
 | Desligar tela do dispositivo (continuar espelhando) | <kbd>MOD</kbd>+<kbd>o</kbd>
 | Ligar tela do dispositivo                   | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>o</kbd>
 | Rotacionar tela do dispositivo              | <kbd>MOD</kbd>+<kbd>r</kbd>
 | Expandir painel de notificação              | <kbd>MOD</kbd>+<kbd>n</kbd> \| _Clique-do-5.°³_
 | Expandir painel de configurção              | <kbd>MOD</kbd>+<kbd>n</kbd>+<kbd>n</kbd> \| _Clique-duplo-do-5.°³_
 | Colapsar paineis                            | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>n</kbd>
 | Copiar para área de transferência⁴          | <kbd>MOD</kbd>+<kbd>c</kbd>
 | Recortar para área de transferência⁴        | <kbd>MOD</kbd>+<kbd>x</kbd>
 | Sincronizar áreas de transferência e colar⁴ | <kbd>MOD</kbd>+<kbd>v</kbd>
 | Injetar texto da área de transferência do computador | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd>
 | Ativar/desativar contador de FPS (em stdout) | <kbd>MOD</kbd>+<kbd>i</kbd>
 | Pinçar para dar zoom                        | <kbd>Ctrl</kbd>+_Clicar-e-mover_
 | Segure e arraste um arquivo APK                        | Instala APK pelo computador
 | Segure e arraste arquivo não-APK                        | [Enviar arquivo para o dispositivo](#push-file-to-device)

_¹Clique-duplo-esquerdo na borda preta para remove-la._  
_²Clique-direito liga a tela caso esteja desligada, pressione BACK caso contrário._  
_³4.° and 5.° botões do mouse, caso o mouse possua._
_⁴Para aplicativos react-native em desenvolvimento,`MENU` dispara o menu de desenvolvimento_
_⁵Apenas em Android >= 7._

Atalhos com teclas reptidas são executados soltando e precionando a tecla
uma segunda vez. Por exemplo, para executar "Expandir painel de Configurção":

 1. Mantenha pressionado <kbd>MOD</kbd>.
 2. Depois click duas vezes <kbd>n</kbd>.
 3. Finalmente, solte <kbd>MOD</kbd>.

Todos os atalhos <kbd>Ctrl</kbd>+_tecla_ são encaminhados para o dispositivo, para que eles sejam
tratados pela aplicação ativa.


## Caminhos personalizados

Para usar um binário _adb_ específico, configure seu caminho na variável de ambiente
`ADB`:

```bash
ADB=/caminho/para/adb scrcpy
```

Para sobrepor o caminho do arquivo `scrcpy-server`, configure seu caminho em
`SCRCPY_SERVER_PATH`.

Para sobrepor o ícone, configure seu caminho em `SCRCPY_ICON_PATH`. 


## Por quê _scrcpy_?

Um colega me desafiou a encontrar um nome tão impronunciável quanto [gnirehtet].

[`strcpy`] copia uma **str**ing; `scrcpy` copia uma **scr**een.

[gnirehtet]: https://github.com/Genymobile/gnirehtet
[`strcpy`]: http://man7.org/linux/man-pages/man3/strcpy.3.html


## Como compilar?

Veja [BUILD].


## Problemas comuns

Veja o [FAQ].

[FAQ:] FAQ.md


## Desenvolvedores

Leia a [página dos desenvolvedores][developers_page].

[developers_page]: DEVELOP.md


## Licença

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

## Artigos

- [Introducing scrcpy][article-intro]
- [Scrcpy now works wirelessly][article-tcpip]

[article-intro]: https://blog.rom1v.com/2018/03/introducing-scrcpy/
[article-tcpip]: https://www.genymotion.com/blog/open-source-project-scrcpy-now-works-wirelessly/

## Contato

Se você encontrar um bug, por favor leia o [FAQ] primeiro, depois abra um [issue].

[issue]: https://github.com/Genymobile/scrcpy/issues

Para perguntar em geral ou discussões, você também pode usar:

 - Reddit: [`r/scrcpy`](https://www.reddit.com/r/scrcpy)
 - Twitter: [`@scrcpy_app`](https://twitter.com/scrcpy_app)

## Traduções

Esse README está disponível em outros idiomas:

- [Deutsch (German, `de`) - v1.22](README.de.md)
- [Indonesian (Indonesia, `id`) - v1.16](README.id.md)
- [Italiano (Italiano, `it`) - v1.23](README.it.md)
- [日本語 (Japanese, `jp`) - v1.19](README.jp.md)
- [한국어 (Korean, `ko`) - v1.11](README.ko.md)
- [Español (Spanish, `sp`) - v1.21](README.sp.md)
- [简体中文 (Simplified Chinese, `zh-Hans`) - v1.22](README.zh-Hans.md)
- [繁體中文 (Traditional Chinese, `zh-Hant`) - v1.15](README.zh-Hant.md)
- [Turkish (Turkish, `tr`) - v1.18](README.tr.md)
