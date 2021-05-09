_Apenas o [README](README.md) original é garantido estar atualizado._

# scrcpy (v1.17)

Esta aplicação fornece exibição e controle de dispositivos Android conectados via
USB (ou [via TCP/IP][article-tcpip]). Não requer nenhum acesso _root_.
Funciona em _GNU/Linux_, _Windows_ e _macOS_.

![screenshot](assets/screenshot-debian-600.jpg)

Foco em:

 - **leveza** (nativo, mostra apenas a tela do dispositivo)
 - **performance** (30~60fps)
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

### Linux

No Debian (_testing_ e _sid_ por enquanto) e Ubuntu (20.04):

```
apt install scrcpy
```

Um pacote [Snap] está disponível: [`scrcpy`][snap-link].

[snap-link]: https://snapstats.org/snaps/scrcpy

[snap]: https://en.wikipedia.org/wiki/Snappy_(package_manager)

Para Fedora, um pacote [COPR] está disponível: [`scrcpy`][copr-link].

[COPR]: https://fedoraproject.org/wiki/Category:Copr
[copr-link]: https://copr.fedorainfracloud.org/coprs/zeno/scrcpy/

Para Arch Linux, um pacote [AUR] está disponível: [`scrcpy`][aur-link].

[AUR]: https://wiki.archlinux.org/index.php/Arch_User_Repository
[aur-link]: https://aur.archlinux.org/packages/scrcpy/

Para Gentoo, uma [Ebuild] está disponível: [`scrcpy/`][ebuild-link].

[Ebuild]: https://wiki.gentoo.org/wiki/Ebuild
[ebuild-link]: https://github.com/maggu2810/maggu2810-overlay/tree/master/app-mobilephone/scrcpy

Você também pode [compilar o app manualmente][BUILD] (não se preocupe, não é tão
difícil).



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
# Homebrew >= 2.6.0
brew install --cask android-platform-tools

# Homebrew < 2.6.0
brew cask install android-platform-tools
```

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
scrcpy --lock-video-orientation 0   # orientação natural
scrcpy --lock-video-orientation 1   # 90° sentido anti-horário
scrcpy --lock-video-orientation 2   # 180°
scrcpy --lock-video-orientation 3   # 90° sentido horário
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
# interrompa a gravação com Ctrl+C
```

"Frames pulados" são gravados, mesmo que não sejam exibidos em tempo real (por
motivos de performance). Frames têm seu _horário carimbado_ no dispositivo, então [variação de atraso nos
pacotes][packet delay variation] não impacta o arquivo gravado.

[packet delay variation]: https://en.wikipedia.org/wiki/Packet_delay_variation


### Conexão

#### Sem fio

_Scrcpy_ usa `adb` para se comunicar com o dispositivo, e `adb` pode [conectar-se][connect] a um
dispositivo via TCP/IP:

1. Conecte o dispositivo no mesmo Wi-Fi do seu computador.
2. Pegue o endereço IP do seu dispositivo, em Configurações → Sobre o telefone → Status, ou
   executando este comando:

    ```bash
    adb shell ip route | awk '{print $9}'
    ```

3. Ative o adb via TCP/IP no seu dispositivo: `adb tcpip 5555`.
4. Desconecte seu dispositivo.
5. Conecte-se ao seu dispositivo: `adb connect DEVICE_IP:5555` _(substitua `DEVICE_IP`)_.
6. Execute `scrcpy` como de costume.

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

Se o dispositivo está conectado via TCP/IP:

```bash
scrcpy --serial 192.168.0.1:5555
scrcpy -s 192.168.0.1:5555  # versão curta
```

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
_adb_):

```bash
adb kill-server    # encerra o servidor adb local em 5037
ssh -CN -L5037:localhost:5037 -R27183:localhost:27183 your_remote_computer
# mantenha isso aberto
```

De outro terminal:

```bash
scrcpy
```

Para evitar ativar o encaminhamento de porta remota, você pode forçar uma conexão
de encaminhamento (note o `-L` em vez de `-R`):

```bash
adb kill-server    # encerra o servidor adb local em 5037
ssh -CN -L5037:localhost:5037 -L27183:localhost:27183 your_remote_computer
# mantenha isso aberto
```

De outro terminal:

```bash
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

```
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

O estado inicial é restaurado quando o scrcpy é fechado.


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


#### Renderizar frames expirados

Por padrão, para minimizar a latência, _scrcpy_ sempre renderiza o último frame decodificado
disponível, e descarta o anterior.

Para forçar a renderização de todos os frames (com o custo de um possível aumento de
latência), use:

```bash
scrcpy --render-expired-frames
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

Por padrão, scrcpy não evita que o descanso de tela rode no computador.

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

#### Pinçar para dar zoom

Para simular "pinçar para dar zoom": <kbd>Ctrl</kbd>+_clicar-e-mover_.

Mais precisamente, segure <kbd>Ctrl</kbd> enquanto pressiona o botão de clique-esquerdo. Até que
o botão de clique-esquerdo seja liberado, todos os movimentos do mouse ampliar e rotacionam o
conteúdo (se suportado pelo app) relativo ao centro da tela.

Concretamente, scrcpy gera eventos adicionais de toque de um "dedo virtual" em
uma posição invertida em relação ao centro da tela.


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

[textevents]: https://blog.rom1v.com/2018/03/introducing-scrcpy/#handle-text-input
[prefertext]: https://github.com/Genymobile/scrcpy/issues/650#issuecomment-512945343


#### Repetir tecla

Por padrão, segurar uma tecla gera eventos de tecla repetidos. Isso pode causar
problemas de performance em alguns jogos, onde esses eventos são inúteis de qualquer forma.

Para evitar o encaminhamento eventos de tecla repetidos:

```bash
scrcpy --no-key-repeat
```


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

Para enviar um arquivo para `/sdcard/` no dispositivo, arraste e solte um arquivo (não-APK) para a
janela do _scrcpy_.

Não existe feedback visual, um log é imprimido no console.

O diretório alvo pode ser mudado ao iniciar:

```bash
scrcpy --push-target /sdcard/foo/bar/
```


### Encaminhamento de áudio

Áudio não é encaminhado pelo _scrcpy_. Use [sndcpy].

Também veja [issue #14].

[sndcpy]: https://github.com/rom1v/sndcpy
[issue #14]: https://github.com/Genymobile/scrcpy/issues/14


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
 | Redimensionar janela para 1:1 (pixel-perfect) | <kbd>MOD</kbd>+<kbd>g</kbd>
 | Redimensionar janela para remover bordas pretas | <kbd>MOD</kbd>+<kbd>w</kbd> \| _Clique-duplo¹_
 | Clicar em `HOME`                            | <kbd>MOD</kbd>+<kbd>h</kbd> \| _Clique-do-meio_
 | Clicar em `BACK`                            | <kbd>MOD</kbd>+<kbd>b</kbd> \| _Clique-direito²_
 | Clicar em `APP_SWITCH`                      | <kbd>MOD</kbd>+<kbd>s</kbd>
 | Clicar em `MENU` (desbloquear tela          | <kbd>MOD</kbd>+<kbd>m</kbd>
 | Clicar em `VOLUME_UP`                       | <kbd>MOD</kbd>+<kbd>↑</kbd> _(cima)_
 | Clicar em `VOLUME_DOWN`                     | <kbd>MOD</kbd>+<kbd>↓</kbd> _(baixo)_
 | Clicar em `POWER`                           | <kbd>MOD</kbd>+<kbd>p</kbd>
 | Ligar                                       | _Clique-direito²_
 | Desligar tela do dispositivo (continuar espelhando) | <kbd>MOD</kbd>+<kbd>o</kbd>
 | Ligar tela do dispositivo                   | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>o</kbd>
 | Rotacionar tela do dispositivo              | <kbd>MOD</kbd>+<kbd>r</kbd>
 | Expandir painel de notificação              | <kbd>MOD</kbd>+<kbd>n</kbd>
 | Colapsar painel de notificação              | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>n</kbd>
 | Copiar para área de transferência³          | <kbd>MOD</kbd>+<kbd>c</kbd>
 | Recortar para área de transferência³        | <kbd>MOD</kbd>+<kbd>x</kbd>
 | Sincronizar áreas de transferência e colar³ | <kbd>MOD</kbd>+<kbd>v</kbd>
 | Injetar texto da área de transferência do computador | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd>
 | Ativar/desativar contador de FPS (em stdout) | <kbd>MOD</kbd>+<kbd>i</kbd>
 | Pinçar para dar zoom                        | <kbd>Ctrl</kbd>+_clicar-e-mover_

_¹Clique-duplo em bordas pretas para removê-las._  
_²Clique-direito liga a tela se ela estiver desligada, pressiona BACK caso contrário._
_³Apenas em Android >= 7._

Todos os atalhos <kbd>Ctrl</kbd>+_tecla_ são encaminhados para o dispositivo, para que eles sejam
tratados pela aplicação ativa.


## Caminhos personalizados

Para usar um binário _adb_ específico, configure seu caminho na variável de ambiente
`ADB`:

    ADB=/caminho/para/adb scrcpy

Para sobrepor o caminho do arquivo `scrcpy-server`, configure seu caminho em
`SCRCPY_SERVER_PATH`.

[useful]: https://github.com/Genymobile/scrcpy/issues/278#issuecomment-429330345


## Por quê _scrcpy_?

Um colega me desafiou a encontrar um nome tão impronunciável quanto [gnirehtet].

[`strcpy`] copia uma **str**ing; `scrcpy` copia uma **scr**een.

[gnirehtet]: https://github.com/Genymobile/gnirehtet
[`strcpy`]: http://man7.org/linux/man-pages/man3/strcpy.3.html


## Como compilar?

Veja [BUILD].

[BUILD]: BUILD.md


## Problemas comuns

Veja o [FAQ](FAQ.md).


## Desenvolvedores

Leia a [página dos desenvolvedores][developers page].

[developers page]: DEVELOP.md


## Licença

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

## Artigos

- [Introducing scrcpy][article-intro]
- [Scrcpy now works wirelessly][article-tcpip]

[article-intro]: https://blog.rom1v.com/2018/03/introducing-scrcpy/
[article-tcpip]: https://www.genymotion.com/blog/open-source-project-scrcpy-now-works-wirelessly/
