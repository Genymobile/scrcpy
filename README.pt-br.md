_Only the original [README](README.md) is guaranteed to be up-to-date._

# scrcpy (v1.12.1)

Esta aplicação fornece visualização e controle de dispositivos Android conectados via
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


Você também pode [compilar a aplicação manualmente][BUILD] (não se preocupe, não é tão difícil).


### Windows

Para Windows, para simplicidade, um arquivo pré-compilado com todas as dependências
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

E no [Scoop]:

```bash
scoop install scrcpy
scoop install adb    # se você ainda não o tem
```

[Scoop]: https://scoop.sh

Você também pode [compilar a aplicação manualmente][BUILD].


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

Você também pode [compilar a aplicação manualmente][BUILD].


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

_Scrcpy_ usa `adb` para se comunicar com o dispositivo, e `adb` pode [conectar-se] à um dispositivo via TCP/IP:

1. Conecte o dispositivo a mesma rede Wi-Fi do seu computador.
2. Pegue o endereço de IP do seu dispositivo (Em Configurações → Sobre o Telefone → Status).
3. Ative o adb via TCP/IP no seu dispositivo: `adb tcpip 5555`.
4. Desplugue seu dispositivo.
5. Conecte-se ao seu dispositivo: `adb connect DEVICE_IP:5555` _(substitua o `DEVICE_IP`)_.
6. Execute `scrcpy` como de costume.

Pode ser útil diminuir o bit-rate e a resolução:

```bash
scrcpy --bit-rate 2M --max-size 800
scrcpy -b2M -m800  # versão reduzida
```

[conectar-se]: https://developer.android.com/studio/command-line/adb.html#wireless


#### N-dispositivos

Se alguns dispositivos estão listados em `adb devices`, você precisa especificar o _serial_:

```bash
scrcpy --serial 0123456789abcdef
scrcpy -s 0123456789abcdef  # versão reduzida
```

Se o dispositivo está conectado via TCP/IP:

```bash
scrcpy --serial 192.168.0.1:5555
scrcpy -s 192.168.0.1:5555  # versão reduzida
```

Você pode iniciar algumas instâncias do _scrcpy_ para alguns dispositivos.

#### Conexão via SSH

Para conectar-se à um dispositivo remoto, é possível se conectar um cliente local `adb`  à um servidor `adb` remoto (contanto que eles usem a mesma versão do protocolo _adb_):

```bash
adb kill-server    # encerra o servidor local na 5037
ssh -CN -L5037:localhost:5037 -R27183:localhost:27183 your_remote_computer
# mantém isso aberto
```

De outro terminal:

```bash
scrcpy
```

Igual para conexões sem fio, pode ser útil reduzir a qualidade:

```
scrcpy -b2M -m800 --max-fps 15
```

### Configurações de Janela

#### Título

Por padrão, o título da janela é o modelo do dispositivo. Isto pode ser mudado:

```bash
scrcpy --window-title 'Meu dispositivo'
```

#### Posição e tamanho

A posição e tamanho iniciais da janela podem ser especificados:

```bash
scrcpy --window-x 100 --window-y 100 --window-width 800 --window-height 600
```

#### Sem bordas

Para desativar decorações da janela:

```bash
scrcpy --window-borderless
```

#### Sempre visível

Para manter a janela do scrcpy sempre visível:

```bash
scrcpy --always-on-top
```

#### Tela cheia

A aplicação pode ser iniciada diretamente em tela cheia:

```bash
scrcpy --fullscreen
scrcpy -f  # versão reduzida
```

Tela cheia pode ser alternada dinamicamente com `Ctrl`+`f`.


### Outras opções de espelhamento

#### Apenas leitura

Para desativar controles (tudo que possa interagir com o dispositivo: teclas de entrada, eventos de mouse, arrastar e soltar arquivos):

```bash
scrcpy --no-control
scrcpy -n
```

#### Desligar a tela

É possível desligar a tela do dispositivo durante o início do espelhamento com uma opção de linha de comando:

```bash
scrcpy --turn-screen-off
scrcpy -S
```

Ou apertando `Ctrl`+`o` durante qualquer momento.

Para ligar novamente, pressione `POWER` (ou `Ctrl`+`p`).

#### Frames expirados de renderização

Por padrão, para minimizar a latência, _scrcpy_ sempre renderiza o último frame decodificado disponível e descarta o anterior.

Para forçar a renderização de todos os frames ( com o custo de aumento de latência), use:

```bash
scrcpy --render-expired-frames
```

#### Mostrar toques

Para apresentações, pode ser útil mostrar toques físicos(dispositivo físico).

Android fornece esta funcionalidade nas _Opções do Desenvolvedor_.

_Scrcpy_ fornece esta opção de ativar esta funcionalidade no início e desativar no encerramento:

```bash
scrcpy --show-touches
scrcpy -t
```

Note que isto mostra apenas toques _físicos_ (com o dedo no dispositivo).


### Controle de entrada

#### Rotacionar a tela do dispositivo

Pressione `Ctrl`+`r` para mudar entre os modos Retrato e Paisagem.

Note que só será rotacionado se a aplicação em primeiro plano tiver suporte para o modo requisitado.

#### Copiar-Colar

É possível sincronizar áreas de transferência entre computador e o dispositivo,
para ambas direções:

 - `Ctrl`+`c` copia a área de transferência do dispositivo para a área de trasferência do computador;
 - `Ctrl`+`Shift`+`v` copia a área de transferência do computador para a área de transferência do dispositivo;
 - `Ctrl`+`v` _cola_ a área de transferência do computador como uma sequência de eventos de texto (mas
   quebra caracteres não-ASCII).

#### Preferências de injeção de texto

Existe dois tipos de [eventos][textevents] gerados ao digitar um texto:
 - _eventos de teclas_, sinalizando que a tecla foi pressionada ou solta;
 - _eventos de texto_, sinalizando que o texto foi inserido.

Por padrão, letras são injetadas usando eventos de teclas, assim teclados comportam-se
como esperado em jogos (normalmente para tecladas WASD)

Mas isto pode [causar problemas][prefertext]. Se você encontrar tal problema,
pode evitá-lo usando:

```bash
scrcpy --prefer-text
```

(mas isto vai quebrar o comportamento do teclado em jogos)

[textevents]: https://blog.rom1v.com/2018/03/introducing-scrcpy/#handle-text-input
[prefertext]: https://github.com/Genymobile/scrcpy/issues/650#issuecomment-512945343


### Transferência de arquivo

#### Instalar APK

Para instalar um APK, arraste e solte o arquivo APK(com extensão `.apk`) na janela _scrcpy_.

Não existe feedback visual, um log é imprimido no console.


#### Enviar arquivo para o dispositivo

Para enviar um arquivo para o diretório `/sdcard/` no dispositivo, arraste e solte um arquivo não APK para a janela do
_scrcpy_.

Não existe feedback visual, um log é imprimido no console.

O diretório alvo pode ser mudado ao iniciar:

```bash
scrcpy --push-target /sdcard/foo/bar/
```


### Encaminhamento de áudio

Áudio não é encaminhando pelo _scrcpy_. Use [USBaudio] (Apenas linux).

Também veja [issue #14].

[USBaudio]: https://github.com/rom1v/usbaudio
[issue #14]: https://github.com/Genymobile/scrcpy/issues/14


## Atalhos

 | Ação                                                          |   Atalho                        |   Atalho (macOS)
 | ------------------------------------------------------------- |:------------------------------- |:-----------------------------
 | Alternar para modo de tela cheia                              | `Ctrl`+`f`                      | `Cmd`+`f`
 | Redimensionar janela para pixel-perfect(Escala 1:1)           | `Ctrl`+`g`                      | `Cmd`+`g`
 | Redimensionar janela para tirar as bordas pretas              | `Ctrl`+`x` \| _Clique-duplo¹_   | `Cmd`+`x`  \| _Clique-duplo¹_
 | Clicar em `HOME`                                              | `Ctrl`+`h` \| _Clique-central_  | `Ctrl`+`h` \| _Clique-central_
 | Clicar em `BACK`                                              | `Ctrl`+`b` \| _Clique-direito²_ | `Cmd`+`b`  \| _Clique-direito²_
 | Clicar em `APP_SWITCH`                                        | `Ctrl`+`s`                      | `Cmd`+`s`
 | Clicar em `MENU`                                              | `Ctrl`+`m`                      | `Ctrl`+`m`
 | Clicar em `VOLUME_UP`                                         | `Ctrl`+`↑` _(cima)_             | `Cmd`+`↑` _(cima)_
 | Clicar em `VOLUME_DOWN`                                       | `Ctrl`+`↓` _(baixo)_            | `Cmd`+`↓` _(baixo)_
 | Clicar em `POWER`                                             | `Ctrl`+`p`                      | `Cmd`+`p`
 | Ligar                                                         | _Clique-direito²_               | _Clique-direito²_
 | Desligar a tela do dispositivo                                | `Ctrl`+`o`                      | `Cmd`+`o`
 | Rotacionar tela do dispositivo                                | `Ctrl`+`r`                      | `Cmd`+`r`
 | Expandir painel de notificação                                | `Ctrl`+`n`                      | `Cmd`+`n`
 | Esconder painel de notificação                                | `Ctrl`+`Shift`+`n`              | `Cmd`+`Shift`+`n`
 | Copiar área de transferência do dispositivo para o computador | `Ctrl`+`c`                      | `Cmd`+`c`
 | Colar área de transferência do computador para o dispositivo  | `Ctrl`+`v`                      | `Cmd`+`v`
 | Copiar área de transferência do computador para dispositivo   | `Ctrl`+`Shift`+`v`              | `Cmd`+`Shift`+`v`
 | Ativar/desativar contador de FPS(Frames por segundo)          | `Ctrl`+`i`                      | `Cmd`+`i`

_¹Clique-duplo em bordas pretas para removê-las._  
_²Botão direito liga a tela se ela estiver desligada, clique BACK para o contrário._


## Caminhos personalizados

Para usar um binário específico _adb_, configure seu caminho na variável de ambiente `ADB`:

    ADB=/caminho/para/adb scrcpy

Para sobrepor o caminho do arquivo `scrcpy-server`, configure seu caminho em
`SCRCPY_SERVER_PATH`.

[useful]: https://github.com/Genymobile/scrcpy/issues/278#issuecomment-429330345


## Por quê _scrcpy_?

Um colega me desafiou a encontrar um nome impronunciável como [gnirehtet].

[`strcpy`] copia uma **str**ing; `scrcpy` copia uma **scr**een.

[gnirehtet]: https://github.com/Genymobile/gnirehtet
[`strcpy`]: http://man7.org/linux/man-pages/man3/strcpy.3.html


## Como compilar?

Veja [BUILD].

[BUILD]: BUILD.md


## Problemas comuns

Veja [FAQ](FAQ.md).


## Desenvolvedores

Leia a  [developers page].

[developers page]: DEVELOP.md


## Licença

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

## Artigos

- [Introducing scrcpy][article-intro]
- [Scrcpy now works wirelessly][article-tcpip]

[article-intro]: https://blog.rom1v.com/2018/03/introducing-scrcpy/
[article-tcpip]: https://www.genymotion.com/blog/open-source-project-scrcpy-now-works-wirelessly/
