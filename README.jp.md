_Only the original [README](README.md) is guaranteed to be up-to-date._

# scrcpy (v1.17)

このアプリケーションはUSB(もしくは[TCP/IP経由][article-tcpip])で接続されたAndroidデバイスの表示と制御を提供します。このアプリケーションは _root_ でのアクセスを必要としません。このアプリケーションは _GNU/Linux_ 、 _Windows_ そして _macOS_ 上で動作します。

![screenshot](assets/screenshot-debian-600.jpg)

以下に焦点を当てています:

 - **軽量** (ネイティブ、デバイス画面表示のみ)
 - **パフォーマンス**　(30~60fps)
 - **クオリティ**　(1920x1080以上)
 - **低遅延**　([35~70ms][lowlatency])
 - **短い起動時間**　(初回画像を1秒以内に表示)
 - **非侵入型**　(デバイスに何もインストールされていない状態になる)

[lowlatency]: https://github.com/Genymobile/scrcpy/pull/646


## 必要要件

AndroidデバイスはAPI21(Android 5.0)以上。

Androidデバイスで[adbデバッグが有効][enable-adb]であること。

[enable-adb]: https://developer.android.com/studio/command-line/adb.html#Enabling

一部のAndroidデバイスでは、キーボードとマウスを使用して制御する[追加オプション][control]を有効にする必要がある。

[control]: https://github.com/Genymobile/scrcpy/issues/70#issuecomment-373286323


## アプリの取得

<a href="https://repology.org/project/scrcpy/versions"><img src="https://repology.org/badge/vertical-allrepos/scrcpy.svg" alt="Packaging status" align="right"></a>

### Linux

Debian (_testing_ と _sid_) とUbuntu(20.04):

```
apt install scrcpy
```

[Snap]パッケージが利用可能: [`scrcpy`][snap-link]

[snap-link]: https://snapstats.org/snaps/scrcpy

[snap]: https://en.wikipedia.org/wiki/Snappy_(package_manager)

Fedora用[COPR]パッケージが利用可能: [`scrcpy`][copr-link]

[COPR]: https://fedoraproject.org/wiki/Category:Copr
[copr-link]: https://copr.fedorainfracloud.org/coprs/zeno/scrcpy/

Arch Linux用[AUR]パッケージが利用可能: [`scrcpy`][aur-link]

[AUR]: https://wiki.archlinux.org/index.php/Arch_User_Repository
[aur-link]: https://aur.archlinux.org/packages/scrcpy/

Gentoo用[Ebuild]が利用可能: [`scrcpy`][ebuild-link]

[Ebuild]: https://wiki.gentoo.org/wiki/Ebuild
[ebuild-link]: https://github.com/maggu2810/maggu2810-overlay/tree/master/app-mobilephone/scrcpy

[自分でビルド][BUILD]も可能（心配しないでください、それほど難しくはありません。）


### Windows

Windowsでは簡単に、（`adb`を含む）すべての依存関係を構築済みのアーカイブを利用可能です。

 - [README](README.md#windows)

[Chocolatey]でも利用可能です:

[Chocolatey]: https://chocolatey.org/

```bash
choco install scrcpy
choco install adb    # まだ入手していない場合
```

[Scoop]でも利用可能です:

```bash
scoop install scrcpy
scoop install adb    # まだ入手していない場合
```

[Scoop]: https://scoop.sh

また、[アプリケーションをビルド][BUILD]することも可能です。

### macOS

アプリケーションは[Homebrew]で利用可能です。ただインストールするだけです。

[Homebrew]: https://brew.sh/

```bash
brew install scrcpy
```

`PATH`から`adb`へのアクセスが必要です。もしまだ持っていない場合:

```bash
# Homebrew >= 2.6.0
brew install --cask android-platform-tools

# Homebrew < 2.6.0
brew cask install android-platform-tools
```

また、[アプリケーションをビルド][BUILD]することも可能です。


## 実行

Androidデバイスを接続し、実行:

```bash
scrcpy
```

次のコマンドでリストされるコマンドライン引数も受け付けます:

```bash
scrcpy --help
```

## 機能

### キャプチャ構成

#### サイズ削減

Androidデバイスを低解像度でミラーリングする場合、パフォーマンス向上に便利な場合があります。

幅と高さをある値(例：1024)に制限するには:

```bash
scrcpy --max-size 1024
scrcpy -m 1024  # 短縮版
```

一方のサイズはデバイスのアスペクト比が維持されるように計算されます。この方法では、1920x1080のデバイスでは1024x576にミラーリングされます。


#### ビットレート変更

ビットレートの初期値は8Mbpsです。ビットレートを変更するには(例:2Mbpsに変更):

```bash
scrcpy --bit-rate 2M
scrcpy -b 2M  # 短縮版
```

#### フレームレート制限

キャプチャするフレームレートを制限できます:

```bash
scrcpy --max-fps 15
```

この機能はAndroid 10からオフィシャルサポートとなっていますが、以前のバージョンでも動作する可能性があります。

#### トリミング

デバイスの画面は、画面の一部のみをミラーリングするようにトリミングできます。

これは、例えばOculus Goの片方の目をミラーリングする場合に便利です。:

```bash
scrcpy --crop 1224:1440:0:0   # オフセット位置(0,0)で1224x1440
```

もし`--max-size`も指定されている場合、トリミング後にサイズ変更が適用されます。

#### ビデオの向きをロックする

ミラーリングの向きをロックするには:

```bash
scrcpy --lock-video-orientation 0   # 自然な向き
scrcpy --lock-video-orientation 1   # 90°反時計回り
scrcpy --lock-video-orientation 2   # 180°
scrcpy --lock-video-orientation 3   # 90°時計回り
```

この設定は録画の向きに影響します。

[ウィンドウは独立して回転することもできます](#回転)。


#### エンコーダ

いくつかのデバイスでは一つ以上のエンコーダを持ちます。それらのいくつかは、問題やクラッシュを引き起こします。別のエンコーダを選択することが可能です:


```bash
scrcpy --encoder OMX.qcom.video.encoder.avc
```

利用可能なエンコーダをリストするために、無効なエンコーダ名を渡すことができます。エラー表示で利用可能なエンコーダを提供します。

```bash
scrcpy --encoder _
```

### 録画

ミラーリング中に画面の録画をすることが可能です:

```bash
scrcpy --record file.mp4
scrcpy -r file.mkv
```

録画中にミラーリングを無効にするには:

```bash
scrcpy --no-display --record file.mp4
scrcpy -Nr file.mkv
# Ctrl+Cで録画を中断する
```

"スキップされたフレーム"は(パフォーマンス上の理由で)リアルタイムで表示されなくても録画されます。

フレームはデバイス上で _タイムスタンプされる_ ため [パケット遅延のバリエーション] は録画されたファイルに影響を与えません。

[パケット遅延のバリエーション]: https://en.wikipedia.org/wiki/Packet_delay_variation


### 接続

#### ワイヤレス

_Scrcpy_ はデバイスとの通信に`adb`を使用します。そして`adb`はTCP/IPを介しデバイスに[接続]することができます:

1. あなたのコンピュータと同じWi-Fiに接続します。
2. あなたのIPアドレスを取得します。設定 → 端末情報 → ステータス情報、もしくは、このコマンドを実行します:

    ```bash
    adb shell ip route | awk '{print $9}'
    ```

3. あなたのデバイスでTCP/IPを介したadbを有効にします: `adb tcpip 5555`
4. あなたのデバイスの接続を外します。
5. あなたのデバイスに接続します:
 `adb connect DEVICE_IP:5555` _(`DEVICE_IP`は置き換える)_
6. 通常通り`scrcpy`を実行します。

この方法はビットレートと解像度を減らすのにおそらく有用です:

```bash
scrcpy --bit-rate 2M --max-size 800
scrcpy -b2M -m800  # 短縮版
```

[接続]: https://developer.android.com/studio/command-line/adb.html#wireless


#### マルチデバイス

もし`adb devices`でいくつかのデバイスがリストされる場合、 _シリアルナンバー_ を指定する必要があります:

```bash
scrcpy --serial 0123456789abcdef
scrcpy -s 0123456789abcdef  # 短縮版
```

デバイスがTCP/IPを介して接続されている場合:

```bash
scrcpy --serial 192.168.0.1:5555
scrcpy -s 192.168.0.1:5555  # 短縮版
```

複数のデバイスに対して、複数の _scrcpy_ インスタンスを開始することができます。

#### デバイス接続での自動起動

[AutoAdb]を使用可能です:

```bash
autoadb scrcpy -s '{}'
```

[AutoAdb]: https://github.com/rom1v/autoadb

#### SSHトンネル

リモートデバイスに接続するため、ローカル`adb`クライアントからリモート`adb`サーバーへ接続することが可能です(同じバージョンの _adb_ プロトコルを使用している場合):

```bash
adb kill-server    # 5037ポートのローカルadbサーバーを終了する
ssh -CN -L5037:localhost:5037 -R27183:localhost:27183 your_remote_computer
# オープンしたままにする
```

他の端末から:

```bash
scrcpy
```

リモートポート転送の有効化を回避するためには、代わりに転送接続を強制することができます(`-R`の代わりに`-L`を使用することに注意):

```bash
adb kill-server    # 5037ポートのローカルadbサーバーを終了する
ssh -CN -L5037:localhost:5037 -L27183:localhost:27183 your_remote_computer
# オープンしたままにする
```

他の端末から:

```bash
scrcpy --force-adb-forward
```


ワイヤレス接続と同様に、クオリティを下げると便利な場合があります:

```
scrcpy -b2M -m800 --max-fps 15
```

### ウィンドウ構成

#### タイトル

ウィンドウのタイトルはデバイスモデルが初期値です。これは変更できます:

```bash
scrcpy --window-title 'My device'
```

#### 位置とサイズ

ウィンドウの位置とサイズの初期値を指定できます:

```bash
scrcpy --window-x 100 --window-y 100 --window-width 800 --window-height 600
```

#### ボーダーレス

ウィンドウの装飾を無効化するには:

```bash
scrcpy --window-borderless
```

#### 常に画面のトップ

scrcpyの画面を常にトップにするには:

```bash
scrcpy --always-on-top
```

#### フルスクリーン

アプリケーションを直接フルスクリーンで開始できます:

```bash
scrcpy --fullscreen
scrcpy -f  # 短縮版
```

フルスクリーンは、次のコマンドで動的に切り替えることができます <kbd>MOD</kbd>+<kbd>f</kbd>


#### 回転

ウィンドウは回転することができます:

```bash
scrcpy --rotation 1
```

設定可能な値:
 - `0`: 回転なし
 - `1`: 90° 反時計回り
 - `2`: 180°
 - `3`: 90° 時計回り

回転は次のコマンドで動的に変更することができます。 <kbd>MOD</kbd>+<kbd>←</kbd>_(左)_ 、 <kbd>MOD</kbd>+<kbd>→</kbd>_(右)_

_scrcpy_ は3つの回転を管理することに注意:
 - <kbd>MOD</kbd>+<kbd>r</kbd>はデバイスに縦向きと横向きの切り替えを要求する(現在実行中のアプリで要求している向きをサポートしていない場合、拒否することがある)
 - [`--lock-video-orientation`](#ビデオの向きをロックする)は、ミラーリングする向きを変更する(デバイスからPCへ送信される向き)。録画に影響します。
 - `--rotation` (もしくは<kbd>MOD</kbd>+<kbd>←</kbd>/<kbd>MOD</kbd>+<kbd>→</kbd>)は、ウィンドウのコンテンツのみを回転します。これは表示にのみに影響し、録画には影響しません。

### 他のミラーリングオプション

#### Read-only　リードオンリー

制御を無効にするには(デバイスと対話する全てのもの:入力キー、マウスイベント、ファイルのドラッグ&ドロップ):

```bash
scrcpy --no-control
scrcpy -n
```

#### ディスプレイ

いくつか利用可能なディスプレイがある場合、ミラーリングするディスプレイを選択できます:

```bash
scrcpy --display 1
```

ディスプレイIDのリストは次の方法で取得できます:

```
adb shell dumpsys display   # search "mDisplayId=" in the output
```

セカンダリディスプレイは、デバイスが少なくともAndroid 10の場合にコントロール可能です。(それ以外ではリードオンリーでミラーリングされます)


#### 起動状態にする

デバイス接続時、少し遅れてからデバイスのスリープを防ぐには:

```bash
scrcpy --stay-awake
scrcpy -w
```

scrcpyが閉じられた時、初期状態に復元されます。

#### 画面OFF

コマンドラインオプションを使用することで、ミラーリングの開始時にデバイスの画面をOFFにすることができます:

```bash
scrcpy --turn-screen-off
scrcpy -S
```

もしくは、<kbd>MOD</kbd>+<kbd>o</kbd>を押すことでいつでもできます。

元に戻すには、<kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>o</kbd>を押します。

Androidでは、`POWER`ボタンはいつでも画面を表示します。便宜上、`POWER`がscrcpyを介して(右クリックもしくは<kbd>MOD</kbd>+<kbd>p</kbd>を介して)送信される場合、(ベストエフォートベースで)少し遅れて、強制的に画面を非表示にします。ただし、物理的な`POWER`ボタンを押した場合は、画面は表示されます。

このオプションはデバイスがスリープしないようにすることにも役立ちます:

```bash
scrcpy --turn-screen-off --stay-awake
scrcpy -Sw
```


#### 期限切れフレームをレンダリングする

初期状態では、待ち時間を最小限にするために、_scrcpy_ は最後にデコードされたフレームをレンダリングし、前のフレームを削除します。

全フレームのレンダリングを強制するには(待ち時間が長くなる可能性があります):

```bash
scrcpy --render-expired-frames
```

#### タッチを表示

プレゼンテーションの場合(物理デバイス上で)物理的なタッチを表示すると便利な場合があります。

Androidはこの機能を _開発者オプション_ で提供します。

_Scrcpy_　は開始時にこの機能を有効にし、終了時に初期値を復元するオプションを提供します:

```bash
scrcpy --show-touches
scrcpy -t
```

(デバイス上で指を使った) _物理的な_ タッチのみ表示されることに注意してください。


#### スクリーンセーバー無効

初期状態では、scrcpyはコンピュータ上でスクリーンセーバーが実行される事を妨げません。

これを無効にするには:

```bash
scrcpy --disable-screensaver
```


### 入力制御

#### デバイス画面の回転

<kbd>MOD</kbd>+<kbd>r</kbd>を押すことで、縦向きと横向きを切り替えます。

フォアグラウンドのアプリケーションが要求された向きをサポートしている場合のみ回転することに注意してください。

#### コピー-ペースト

Androidのクリップボードが変更される度に、コンピュータのクリップボードに自動的に同期されます。

<kbd>Ctrl</kbd>のショートカットは全てデバイスに転送されます。特に:
 - <kbd>Ctrl</kbd>+<kbd>c</kbd> 通常はコピーします
 - <kbd>Ctrl</kbd>+<kbd>x</kbd> 通常はカットします
 - <kbd>Ctrl</kbd>+<kbd>v</kbd> 通常はペーストします(コンピュータとデバイスのクリップボードが同期された後)

通常は期待通りに動作します。

しかしながら、実際の動作はアクティブなアプリケーションに依存します。例えば、_Termux_ は代わりに<kbd>Ctrl</kbd>+<kbd>c</kbd>でSIGINTを送信します、そして、_K-9 Mail_ は新しいメッセージを作成します。

このようなケースでコピー、カットそしてペーストをするには(Android 7以上でのサポートのみですが):
 - <kbd>MOD</kbd>+<kbd>c</kbd> `COPY`を挿入
 - <kbd>MOD</kbd>+<kbd>x</kbd> `CUT`を挿入
 - <kbd>MOD</kbd>+<kbd>v</kbd> `PASTE`を挿入(コンピュータとデバイスのクリップボードが同期された後)

加えて、<kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd>はコンピュータのクリップボードテキストにキーイベントのシーケンスとして挿入することを許可します。これはコンポーネントがテキストのペーストを許可しない場合(例えば _Termux_)に有用ですが、非ASCIIコンテンツを壊す可能性があります。

**警告:** デバイスにコンピュータのクリップボードを(<kbd>Ctrl</kbd>+<kbd>v</kbd>または<kbd>MOD</kbd>+<kbd>v</kbd>を介して)ペーストすることは、デバイスのクリップボードにコンテンツをコピーします。結果としてどのAndoridアプリケーションもそのコンテンツを読み取ることができます。機密性の高いコンテンツ(例えばパスワードなど)をこの方法でペーストすることは避けてください。

プログラムでデバイスのクリップボードを設定した場合、一部のデバイスは期待どおりに動作しません。`--legacy-paste`オプションは、コンピュータのクリップボードテキストをキーイベントのシーケンスとして挿入するため(<kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd>と同じ方法)、<kbd>Ctrl</kbd>+<kbd>v</kbd>と<kbd>MOD</kbd>+<kbd>v</kbd>の動作の変更を提供します。

#### ピンチしてズームする

"ピンチしてズームする"をシミュレートするには: <kbd>Ctrl</kbd>+_クリック&移動_

より正確にするには、左クリックボタンを押している間、<kbd>Ctrl</kbd>を押したままにします。左クリックボタンを離すまで、全てのマウスの動きは、(アプリでサポートされている場合)画面の中心を基準として、コンテンツを拡大縮小および回転します。

具体的には、scrcpyは画面の中央を反転した位置にある"バーチャルフィンガー"から追加のタッチイベントを生成します。


#### テキストインジェクション環境設定

テキストをタイプした時に生成される2種類の[イベント][textevents]があります:
 - _key events_　はキーを押したときと離したことを通知します。
 - _text events_　はテキストが入力されたことを通知します。

初期状態で、文字はキーイベントで挿入されるため、キーボードはゲームで期待通りに動作します(通常はWASDキー)。

しかし、これは[問題を引き起こす][prefertext]かもしれません。もしこのような問題が発生した場合は、この方法で回避できます:

```bash
scrcpy --prefer-text
```

(しかしこの方法はゲームのキーボードの動作を壊します)

[textevents]: https://blog.rom1v.com/2018/03/introducing-scrcpy/#handle-text-input
[prefertext]: https://github.com/Genymobile/scrcpy/issues/650#issuecomment-512945343


#### キーの繰り返し

初期状態では、キーの押しっぱなしは繰り返しのキーイベントを生成します。これらのイベントが使われない場合でも、この方法は一部のゲームでパフォーマンスの問題を引き起す可能性があります。

繰り返しのキーイベントの転送を回避するためには:

```bash
scrcpy --no-key-repeat
```


#### 右クリックと真ん中クリック

初期状態では、右クリックはバックの動作(もしくはパワーオン)を起こし、真ん中クリックではホーム画面へ戻ります。このショートカットを無効にし、代わりにデバイスへクリックを転送するには:

```bash
scrcpy --forward-all-clicks
```


### ファイルのドロップ

#### APKのインストール

APKをインストールするには、(`.apk`で終わる)APKファイルを _scrcpy_ の画面にドラッグ&ドロップします。

見た目のフィードバックはありません。コンソールにログが出力されます。


#### デバイスにファイルを送る

デバイスの`/sdcard/`ディレクトリにファイルを送るには、(APKではない)ファイルを _scrcpy_ の画面にドラッグ&ドロップします。

見た目のフィードバックはありません。コンソールにログが出力されます。

転送先ディレクトリを起動時に変更することができます:

```bash
scrcpy --push-target /sdcard/foo/bar/
```


### 音声転送

音声は _scrcpy_ では転送されません。[sndcpy]を使用します。

[issue #14]も参照ください。

[sndcpy]: https://github.com/rom1v/sndcpy
[issue #14]: https://github.com/Genymobile/scrcpy/issues/14


## ショートカット

次のリストでは、<kbd>MOD</kbd>でショートカット変更します。初期状態では、(left)<kbd>Alt</kbd>または(left)<kbd>Super</kbd>です。

これは`--shortcut-mod`で変更することができます。可能なキーは`lctrl`、`rctrl`、`lalt`、 `ralt`、 `lsuper`そして`rsuper`です。例えば:

```bash
# RCtrlをショートカットとして使用します
scrcpy --shortcut-mod=rctrl

# ショートカットにLCtrl+LAltまたはLSuperのいずれかを使用します
scrcpy --shortcut-mod=lctrl+lalt,lsuper
```

_<kbd>[Super]</kbd>は通常<kbd>Windows</kbd>もしくは<kbd>Cmd</kbd>キーです。_

[Super]: https://en.wikipedia.org/wiki/Super_key_(keyboard_button)

 | アクション                                   |   ショートカット
 | ------------------------------------------- |:-----------------------------
 | フルスクリーンモードへの切り替え               | <kbd>MOD</kbd>+<kbd>f</kbd>
 | ディスプレイを左に回転                        | <kbd>MOD</kbd>+<kbd>←</kbd> _(左)_
 | ディスプレイを右に回転                        | <kbd>MOD</kbd>+<kbd>→</kbd> _(右)_
 | ウィンドウサイズを変更して1:1に変更(ピクセルパーフェクト)  | <kbd>MOD</kbd>+<kbd>g</kbd>
 | ウィンドウサイズを変更して黒い境界線を削除      | <kbd>MOD</kbd>+<kbd>w</kbd> \| _ダブルクリック¹_
 | `HOME`をクリック                             | <kbd>MOD</kbd>+<kbd>h</kbd> \| _真ん中クリック_
 | `BACK`をクリック                             | <kbd>MOD</kbd>+<kbd>b</kbd> \| _右クリック²_
 | `APP_SWITCH`をクリック                       | <kbd>MOD</kbd>+<kbd>s</kbd>
 | `MENU` (画面のアンロック)をクリック            | <kbd>MOD</kbd>+<kbd>m</kbd>
 | `VOLUME_UP`をクリック                        | <kbd>MOD</kbd>+<kbd>↑</kbd> _(上)_
 | `VOLUME_DOWN`をクリック                      | <kbd>MOD</kbd>+<kbd>↓</kbd> _(下)_
 | `POWER`をクリック                            | <kbd>MOD</kbd>+<kbd>p</kbd>
 | 電源オン                                     | _右クリック²_
 | デバイス画面をオフにする(ミラーリングしたまま)  | <kbd>MOD</kbd>+<kbd>o</kbd>
 | デバイス画面をオンにする                      | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>o</kbd>
 | デバイス画面を回転する                        | <kbd>MOD</kbd>+<kbd>r</kbd>
 | 通知パネルを展開する                          | <kbd>MOD</kbd>+<kbd>n</kbd>
 | 通知パネルを折りたたむ                        | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>n</kbd>
 | クリップボードへのコピー³                     | <kbd>MOD</kbd>+<kbd>c</kbd>
 | クリップボードへのカット³                     | <kbd>MOD</kbd>+<kbd>x</kbd>
 | クリップボードの同期とペースト³                | <kbd>MOD</kbd>+<kbd>v</kbd>
 | コンピュータのクリップボードテキストの挿入      | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd>
 | FPSカウンタ有効/無効(標準入出力上)             | <kbd>MOD</kbd>+<kbd>i</kbd>
 | ピンチしてズームする                          | <kbd>Ctrl</kbd>+_クリック&移動_

_¹黒い境界線を削除するため、境界線上でダブルクリック_  
_²もしスクリーンがオフの場合、右クリックでスクリーンをオンする。それ以外の場合はBackを押します._  
_³Android 7以上のみ._

全ての<kbd>Ctrl</kbd>+_キー_ ショートカットはデバイスに転送されます、そのためアクティブなアプリケーションによって処理されます。


## カスタムパス

特定の _adb_ バイナリを使用する場合、そのパスを環境変数`ADB`で構成します:

    ADB=/path/to/adb scrcpy

`scrcpy-server`ファイルのパスを上書きするには、`SCRCPY_SERVER_PATH`でそのパスを構成します。

[useful]: https://github.com/Genymobile/scrcpy/issues/278#issuecomment-429330345


## なぜ _scrcpy_?

同僚が私に、[gnirehtet]のように発音できない名前を見つけるように要求しました。

[`strcpy`]は**str**ingをコピーします。`scrcpy`は**scr**eenをコピーします。

[gnirehtet]: https://github.com/Genymobile/gnirehtet
[`strcpy`]: http://man7.org/linux/man-pages/man3/strcpy.3.html


## ビルド方法は?

[BUILD]を参照してください。

[BUILD]: BUILD.md


## よくある質問

[FAQ](FAQ.md)を参照してください。


## 開発者

[開発者のページ]を読んでください。

[開発者のページ]: DEVELOP.md


## ライセンス

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

## 記事

- [Introducing scrcpy][article-intro]
- [Scrcpy now works wirelessly][article-tcpip]

[article-intro]: https://blog.rom1v.com/2018/03/introducing-scrcpy/
[article-tcpip]: https://www.genymotion.com/blog/open-source-project-scrcpy-now-works-wirelessly/
