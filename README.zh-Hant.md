_Only the original [README](README.md) is guaranteed to be up-to-date._

_只有原版的 [README](README.md)是保證最新的。_


本文件翻譯時點: [521f2fe](https://github.com/Genymobile/scrcpy/commit/521f2fe994019065e938aa1a54b56b4f10a4ac4a#diff-04c6e90faac2675aa89e2176d2eec7d8)


# scrcpy (v1.15)

Scrcpy 可以透過 USB、或是 [TCP/IP][article-tcpip] 來顯示或控制 Android 裝置。且 scrcpy 不需要 _root_ 權限。

Scrcpy 目前支援 _GNU/Linux_、_Windows_ 和 _macOS_。

![screenshot](assets/screenshot-debian-600.jpg)

特色:

 - **輕量** (只顯示裝置螢幕)
 - **效能** (30~60fps)
 - **品質** (1920×1080 或更高)
 - **低延遲** ([35~70ms][lowlatency])
 - **快速啟動** (~1 秒就可以顯示第一個畫面)
 - **非侵入性** (不安裝、留下任何東西在裝置上)

[lowlatency]: https://github.com/Genymobile/scrcpy/pull/646


## 需求

Android 裝置必須是 API 21+ (Android 5.0+)。

請確認裝置上的 [adb 偵錯 (通常位於開發者模式內)][enable-adb] 已啟用。

[enable-adb]: https://developer.android.com/studio/command-line/adb.html#Enabling


在部分的裝置上，你也必須啟用[特定的額外選項][control]才能使用鍵盤和滑鼠控制。

[control]: https://github.com/Genymobile/scrcpy/issues/70#issuecomment-373286323


## 下載/獲取軟體


### Linux

Debian (目前支援 _testing_ 和 _sid_) 和 Ubuntu (20.04):

```
apt install scrcpy
```

[Snap] 上也可以下載: [`scrcpy`][snap-link].

[snap-link]: https://snapstats.org/snaps/scrcpy

[snap]: https://en.wikipedia.org/wiki/Snappy_(package_manager)

在 Fedora 上也可以使用 [COPR] 下載: [`scrcpy`][copr-link].

[COPR]: https://fedoraproject.org/wiki/Category:Copr
[copr-link]: https://copr.fedorainfracloud.org/coprs/zeno/scrcpy/

在 Arch Linux 上也可以使用 [AUR] 下載: [`scrcpy`][aur-link].

[AUR]: https://wiki.archlinux.org/index.php/Arch_User_Repository
[aur-link]: https://aur.archlinux.org/packages/scrcpy/

在 Gentoo 上也可以使用 [Ebuild] 下載: [`scrcpy/`][ebuild-link].

[Ebuild]: https://wiki.gentoo.org/wiki/Ebuild
[ebuild-link]: https://github.com/maggu2810/maggu2810-overlay/tree/master/app-mobilephone/scrcpy

你也可以自己[編譯 _Scrcpy_][BUILD]。別擔心，並沒有想像中的難。



### Windows

為了保持簡單，Windows 用戶可以下載一個包含所有必需軟體 (包含 `adb`) 的壓縮包:

 - [README](README.md#windows)

[Chocolatey] 上也可以下載:

[Chocolatey]: https://chocolatey.org/

```bash
choco install scrcpy
choco install adb    # 如果你還沒有安裝的話
```

[Scoop] 上也可以下載:

```bash
scoop install scrcpy
scoop install adb    # 如果你還沒有安裝的話
```

[Scoop]: https://scoop.sh

你也可以自己[編譯 _Scrcpy_][BUILD]。


### macOS

_Scrcpy_ 可以在 [Homebrew] 上直接安裝:

[Homebrew]: https://brew.sh/

```bash
brew install scrcpy
```

由於執行期間需要可以藉由 `PATH` 存取 `adb` 。如果還沒有安裝 `adb` 可以使用下列方式安裝:

```bash
brew cask install android-platform-tools
```

你也可以自己[編譯 _Scrcpy_][BUILD]。


## 執行

將電腦和你的 Android 裝置連線，然後執行:

```bash
scrcpy
```

_Scrcpy_ 可以接受命令列參數。輸入下列指令就可以瀏覽可以使用的命令列參數:

```bash
scrcpy --help
```


## 功能

> 以下說明中，有關快捷鍵的說明可能會出現 <kbd>MOD</kbd> 按鈕。相關說明請參見[快捷鍵]內的說明。

[快捷鍵]: #快捷鍵

### 畫面擷取

#### 縮小尺寸

使用比較低的解析度來投放 Android 裝置在某些情況可以提升效能。

限制寬和高的最大值(例如: 1024):

```bash
scrcpy --max-size 1024
scrcpy -m 1024  # 縮短版本
```

比較小的參數會根據螢幕比例重新計算。
根據上面的範例，1920x1080 會被縮小成 1024x576。


#### 更改 bit-rate

預設的 bit-rate 是 8 Mbps。如果要更改 bit-rate (例如: 2 Mbps):

```bash
scrcpy --bit-rate 2M
scrcpy -b 2M  # 縮短版本
```

#### 限制 FPS

限制畫面最高的 FPS:

```bash
scrcpy --max-fps 15
```

僅在 Android 10 後正式支援，不過也有可能可以在 Android 10 以前的版本使用。

#### 裁切

裝置的螢幕可以裁切。如此一來，鏡像出來的螢幕就只會是原本的一部份。

假如只要鏡像 Oculus Go 的其中一隻眼睛:

```bash
scrcpy --crop 1224:1440:0:0   # 位於 (0,0)，大小1224x1440
```

如果 `--max-size` 也有指定的話，裁切後才會縮放。


#### 鎖定影像方向


如果要鎖定鏡像影像方向:

```bash
scrcpy --lock-video-orientation 0   # 原本的方向
scrcpy --lock-video-orientation 1   # 逆轉 90°
scrcpy --lock-video-orientation 2   # 180°
scrcpy --lock-video-orientation 3   # 順轉 90°
```

這會影響錄影結果的影像方向。


### 錄影

鏡像投放螢幕的同時也可以錄影:

```bash
scrcpy --record file.mp4
scrcpy -r file.mkv
```

如果只要錄影，不要投放螢幕鏡像的話:

```bash
scrcpy --no-display --record file.mp4
scrcpy -Nr file.mkv
# 用 Ctrl+C 停止錄影
```

就算有些幀為了效能而被跳過，它們還是一樣會被錄製。

裝置上的每一幀都有時間戳記，所以 [封包延遲 (Packet Delay Variation, PDV)][packet delay variation] 並不會影響錄影的檔案。

[packet delay variation]: https://en.wikipedia.org/wiki/Packet_delay_variation


### 連線

#### 無線

_Scrcpy_ 利用 `adb` 和裝置通訊，而 `adb` 可以[透過 TCP/IP 連結][connect]:

1. 讓電腦和裝置連到同一個 Wi-Fi。
2. 獲取手機的 IP 位址(設定 → 關於手機 → 狀態).
3. 啟用裝置上的 `adb over TCP/IP`: `adb tcpip 5555`.
4. 拔掉裝置上的線。
5. 透過 TCP/IP 連接裝置: `adb connect DEVICE_IP:5555` _(把 `DEVICE_IP` 換成裝置的IP位址)_.
6. 和平常一樣執行 `scrcpy`。

如果效能太差，可以降低 bit-rate 和解析度:

```bash
scrcpy --bit-rate 2M --max-size 800
scrcpy -b2M -m800  # 縮短版本
```

[connect]: https://developer.android.com/studio/command-line/adb.html#wireless


#### 多裝置

如果 `adb devices` 內有多個裝置，則必須附上 _serial_:

```bash
scrcpy --serial 0123456789abcdef
scrcpy -s 0123456789abcdef  # 縮短版本
```

如果裝置是透過 TCP/IP 連線:

```bash
scrcpy --serial 192.168.0.1:5555
scrcpy -s 192.168.0.1:5555  # 縮短版本
```

你可以啟用復數個對應不同裝置的 _scrcpy_。

#### 裝置連結後自動啟動

你可以使用 [AutoAdb]:

```bash
autoadb scrcpy -s '{}'
```

[AutoAdb]: https://github.com/rom1v/autoadb

#### SSH tunnel

本地的 `adb` 可以連接到遠端的 `adb` 伺服器(假設兩者使用相同版本的 _adb_ 通訊協定)，以連接到遠端裝置:

```bash
adb kill-server    # 停止在 Port 5037 的本地 adb 伺服
ssh -CN -L5037:localhost:5037 -R27183:localhost:27183 your_remote_computer
# 保持開啟
```

從另外一個終端機:

```bash
scrcpy
```

如果要避免啟用 remote port forwarding，你可以強制它使用 forward connection (注意 `-L` 和 `-R` 的差別):

```bash
adb kill-server    # 停止在 Port 5037 的本地 adb 伺服
ssh -CN -L5037:localhost:5037 -L27183:localhost:27183 your_remote_computer
# 保持開啟
```

從另外一個終端機:

```bash
scrcpy --force-adb-forward
```


和無線連接一樣，有時候降低品質會比較好:

```
scrcpy -b2M -m800 --max-fps 15
```

### 視窗調整

#### 標題

預設標題是裝置的型號，不過可以透過以下方式修改:

```bash
scrcpy --window-title 'My device'
```

#### 位置 & 大小

初始的視窗位置和大小也可以指定:

```bash
scrcpy --window-x 100 --window-y 100 --window-width 800 --window-height 600
```

#### 無邊框

如果要停用視窗裝飾:

```bash
scrcpy --window-borderless
```

#### 保持最上層

如果要保持 `scrcpy` 的視窗在最上層:

```bash
scrcpy --always-on-top
```

#### 全螢幕

這個軟體可以直接在全螢幕模式下起動:

```bash
scrcpy --fullscreen
scrcpy -f  # 縮短版本
```

全螢幕可以使用 <kbd>MOD</kbd>+<kbd>f</kbd> 開關。

#### 旋轉

視窗可以旋轉:

```bash
scrcpy --rotation 1
```

可用的數值:
 - `0`: 不旋轉
 - `1`: 90 度**逆**轉
 - `2`: 180 度
 - `3`: 90 度**順**轉

旋轉方向也可以使用 <kbd>MOD</kbd>+<kbd>←</kbd> _(左方向鍵)_ 和 <kbd>MOD</kbd>+<kbd>→</kbd> _(右方向鍵)_ 調整。

_scrcpy_ 有 3 種不同的旋轉:
 - <kbd>MOD</kbd>+<kbd>r</kbd> 要求裝置在垂直、水平之間旋轉 (目前運行中的 App 有可能會因為不支援而拒絕)。
 - `--lock-video-orientation` 修改鏡像的方向 (裝置傳給電腦的影像)。這會影響錄影結果的影像方向。
 - `--rotation` (或是 <kbd>MOD</kbd>+<kbd>←</kbd> / <kbd>MOD</kbd>+<kbd>→</kbd>) 只旋轉視窗的內容。這只會影響鏡像結果，不會影響錄影結果。


### 其他鏡像選項

#### 唯讀

停用所有控制，包含鍵盤輸入、滑鼠事件、拖放檔案:

```bash
scrcpy --no-control
scrcpy -n
```

#### 顯示螢幕

如果裝置有複數個螢幕，可以指定要鏡像哪個螢幕:

```bash
scrcpy --display 1
```

可以透過下列指令獲取螢幕 ID:

```
adb shell dumpsys display   # 找輸出結果中的 "mDisplayId="
```

第二螢幕只有在 Android 10+ 時可以控制。如果不是 Android 10+，螢幕就會在唯讀狀態下投放。


#### 保持清醒

如果要避免裝置在連接狀態下進入睡眠:

```bash
scrcpy --stay-awake
scrcpy -w
```

_scrcpy_ 關閉後就會回復成原本的設定。


#### 關閉螢幕

鏡像開始時，可以要求裝置關閉螢幕:

```bash
scrcpy --turn-screen-off
scrcpy -S
```

或是在任何時候輸入 <kbd>MOD</kbd>+<kbd>o</kbd>。

如果要開啟螢幕，輸入 <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>o</kbd>。

在 Android 上，`POWER` 按鈕總是開啟螢幕。

為了方便，如果 `POWER` 是透過 scrcpy 轉送 (右鍵 或 <kbd>MOD</kbd>+<kbd>p</kbd>)的話，螢幕將會在短暫的延遲後關閉。

實際在手機上的 `POWER` 還是會開啟螢幕。

防止裝置進入睡眠狀態:

```bash
scrcpy --turn-screen-off --stay-awake
scrcpy -Sw
```


#### 顯示過期的幀

為了降低延遲， _scrcpy_ 預設只會顯示最後解碼的幀，並且拋棄所有在這之前的幀。

如果要強制顯示所有的幀 (有可能會拉高延遲)，輸入:

```bash
scrcpy --render-expired-frames
```

#### 顯示觸控點

對於要報告的人來說，顯示裝置上的實際觸控點有時候是有幫助的。

Android 在_開發者選項_中有提供這個功能。

_Scrcpy_ 可以在啟動時啟用這個功能，並且在停止後恢復成原本的設定:

```bash
scrcpy --show-touches
scrcpy -t
```

這個選項只會顯示**實際觸碰在裝置上的觸碰點**。


### 輸入控制


#### 旋轉裝置螢幕

輸入 <kbd>MOD</kbd>+<kbd>r</kbd> 以在垂直、水平之間切換。

如果使用中的程式不支援，則不會切換。


#### 複製/貼上

如果 Android 剪貼簿上的內容有任何更動，電腦的剪貼簿也會一起更動。

任何與 <kbd>Ctrl</kbd> 相關的快捷鍵事件都會轉送到裝置上。特別來說:
 - <kbd>Ctrl</kbd>+<kbd>c</kbd> 通常是複製
 - <kbd>Ctrl</kbd>+<kbd>x</kbd> 通常是剪下
 - <kbd>Ctrl</kbd>+<kbd>v</kbd> 通常是貼上 (在電腦的剪貼簿與裝置上的剪貼簿同步之後)

這些跟你通常預期的行為一樣。

但是，實際上的行為是根據目前運行中的應用程式而定。

舉例來說， _Termux_ 在收到 <kbd>Ctrl</kbd>+<kbd>c</kbd> 後，會傳送 SIGINT；而 _K-9 Mail_ 則是建立新訊息。

如果在這情況下，要剪下、複製或貼上 (只有在Android 7+時才支援):
 - <kbd>MOD</kbd>+<kbd>c</kbd> 注入 `複製`
 - <kbd>MOD</kbd>+<kbd>x</kbd> 注入 `剪下`
 - <kbd>MOD</kbd>+<kbd>v</kbd> 注入 `貼上` (在電腦的剪貼簿與裝置上的剪貼簿同步之後)

另外，<kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd> 則是以一連串的按鍵事件貼上電腦剪貼簿中的內容。當元件不允許文字貼上 (例如 _Termux_) 時，這就很有用。不過，這在非 ASCII 內容上就無法使用。

**警告:** 貼上電腦的剪貼簿內容 (無論是從 <kbd>Ctrl</kbd>+<kbd>v</kbd> 或 <kbd>MOD</kbd>+<kbd>v</kbd>) 時，會複製剪貼簿中的內容至裝置的剪貼簿上。這會讓所有 Android 程式讀取剪貼簿的內容。請避免貼上任何敏感內容 (像是密碼)。


#### 文字輸入偏好

輸入文字時，有兩種[事件][textevents]會被觸發:
 - _鍵盤事件 (key events)_，代表有一個按鍵被按下或放開
 - _文字事件 (text events)_，代表有一個文字被輸入

預設上，文字是被以鍵盤事件 (key events) 輸入的，所以鍵盤和遊戲內所預期的一樣 (通常是指 WASD)。

但是這可能造成[一些問題][prefertext]。如果在這輸入這方面遇到了問題，你可以試試:

```bash
scrcpy --prefer-text
```

(不過遊戲內鍵盤就會不可用)

[textevents]: https://blog.rom1v.com/2018/03/introducing-scrcpy/#handle-text-input
[prefertext]: https://github.com/Genymobile/scrcpy/issues/650#issuecomment-512945343


#### 重複輸入

通常來說，長時間按住一個按鍵會重複觸發按鍵事件。這會在一些遊戲中造成效能問題，而且這個重複的按鍵事件是沒有意義的。

如果不要轉送這些重複的按鍵事件:

```bash
scrcpy --no-key-repeat
```


### 檔案

#### 安裝 APK

如果要安裝 APK ，拖放一個 APK 檔案 (以 `.apk` 為副檔名) 到 _scrcpy_ 的視窗上。

視窗上不會有任何反饋；結果會顯示在命令列中。


#### 推送檔案至裝置

如果要推送檔案到裝置上的 `/sdcard/` ，拖放一個非 APK 檔案 (**不**以 `.apk` 為副檔名) 到 _scrcpy_ 的視窗上。

視窗上不會有任何反饋；結果會顯示在命令列中。

推送檔案的目標路徑可以在啟動時指定:

```bash
scrcpy --push-target /sdcard/foo/bar/
```


### 音訊轉送

_scrcpy_ **不**轉送音訊。請使用 [sndcpy]。另外，參見 [issue #14]。

[sndcpy]: https://github.com/rom1v/sndcpy
[issue #14]: https://github.com/Genymobile/scrcpy/issues/14


## 快捷鍵

在以下的清單中，<kbd>MOD</kbd> 是快捷鍵的特殊按鍵。通常來說，這個按鍵是 (左) <kbd>Alt</kbd> 或是 (左) <kbd>Super</kbd>。

這個是可以使用 `--shortcut-mod` 更改的。可以用的選項有:
- `lctrl`: 左邊的 <kbd>Ctrl</kbd>
- `rctrl`: 右邊的 <kbd>Ctrl</kbd>
- `lalt`: 左邊的 <kbd>Alt</kbd>
- `ralt`: 右邊的 <kbd>Alt</kbd>
- `lsuper`: 左邊的 <kbd>Super</kbd>
- `rsuper`: 右邊的 <kbd>Super</kbd>

```bash
# 以 右邊的 Ctrl 為快捷鍵特殊按鍵
scrcpy --shortcut-mod=rctrl

# 以 左邊的 Ctrl 和左邊的 Alt 或是 左邊的 Super 為快捷鍵特殊按鍵
scrcpy --shortcut-mod=lctrl+lalt,lsuper
```

_<kbd>[Super]</kbd> 通常是 <kbd>Windows</kbd> 或 <kbd>Cmd</kbd> 鍵。_

[Super]: https://en.wikipedia.org/wiki/Super_key_(keyboard_button)

 | Action                                            |   Shortcut
 | ------------------------------------------------- |:-----------------------------
 | 切換至全螢幕                                       | <kbd>MOD</kbd>+<kbd>f</kbd>
 | 左旋顯示螢幕                                       | <kbd>MOD</kbd>+<kbd>←</kbd> _(左)_
 | 右旋顯示螢幕                                       | <kbd>MOD</kbd>+<kbd>→</kbd> _(右)_
 | 縮放視窗成 1:1 (pixel-perfect)                     | <kbd>MOD</kbd>+<kbd>g</kbd>
 | 縮放視窗到沒有黑邊框為止                            | <kbd>MOD</kbd>+<kbd>w</kbd> \| _雙擊¹_
 | 按下 `首頁` 鍵                                     | <kbd>MOD</kbd>+<kbd>h</kbd> \| _中鍵_
 | 按下 `返回` 鍵                                     | <kbd>MOD</kbd>+<kbd>b</kbd> \| _右鍵²_
 | 按下 `切換 APP` 鍵                                 | <kbd>MOD</kbd>+<kbd>s</kbd>
 | 按下 `選單` 鍵 (或解鎖螢幕)                         | <kbd>MOD</kbd>+<kbd>m</kbd>
 | 按下 `音量+` 鍵                                    | <kbd>MOD</kbd>+<kbd>↑</kbd> _(上)_
 | 按下 `音量-` 鍵                                    | <kbd>MOD</kbd>+<kbd>↓</kbd> _(下)_
 | 按下 `電源` 鍵                                     | <kbd>MOD</kbd>+<kbd>p</kbd>
 | 開啟                                               | _右鍵²_
 | 關閉裝置螢幕(持續鏡像)                              | <kbd>MOD</kbd>+<kbd>o</kbd>
 | 開啟裝置螢幕                                        | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>o</kbd>
 | 旋轉裝置螢幕                                        | <kbd>MOD</kbd>+<kbd>r</kbd>
 | 開啟通知列                                          | <kbd>MOD</kbd>+<kbd>n</kbd>
 | 關閉通知列                                          | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>n</kbd>
 | 複製至剪貼簿³                                       | <kbd>MOD</kbd>+<kbd>c</kbd>
 | 剪下至剪貼簿³                                       | <kbd>MOD</kbd>+<kbd>x</kbd>
 | 同步剪貼簿並貼上³                                   | <kbd>MOD</kbd>+<kbd>v</kbd>
 | 複製電腦剪貼簿中的文字至裝置並貼上                    | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd>
 | 啟用/停用 FPS 計數器(顯示於 stdout - 通常是命令列)    | <kbd>MOD</kbd>+<kbd>i</kbd>

_¹在黑邊框上雙擊以移除它們。_  
_²右鍵會返回。如果螢幕是關閉狀態，則會打開螢幕。_  
_³只支援 Android 7+。_

所有 <kbd>Ctrl</kbd>+_按鍵_ 快捷鍵都會傳送到裝置上，所以它們是由目前運作的應用程式處理的。


## 自訂路徑

如果要使用特定的 _adb_ ，將它設定到環境變數中的 `ADB`:

    ADB=/path/to/adb scrcpy

如果要覆寫 `scrcpy-server` 檔案的路徑，則將路徑設定到環境變數中的 `SCRCPY_SERVER_PATH`。

[相關連結][useful]

[useful]: https://github.com/Genymobile/scrcpy/issues/278#issuecomment-429330345


## 為何叫 _scrcpy_ ？

有一個同事要我找一個跟 [gnirehtet] 一樣難念的名字。

[`strcpy`] 複製一個字串 (**str**ing)；`scrcpy` 複製一個螢幕 (**scr**een)。

[gnirehtet]: https://github.com/Genymobile/gnirehtet
[`strcpy`]: http://man7.org/linux/man-pages/man3/strcpy.3.html


## 如何編譯？

請看[這份文件 (英文)][BUILD]。

[BUILD]: BUILD.md


## 常見問題

請看[這份文件 (英文)][FAQ]。

[FAQ]: FAQ.md


## 開發者文件

請看[這個頁面 (英文)][developers page].

[developers page]: DEVELOP.md


## Licence

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

## 相關文章

- [Scrcpy 簡介 (英文)][article-intro]
- [Scrcpy 可以無線連線了 (英文)][article-tcpip]

[article-intro]: https://blog.rom1v.com/2018/03/introducing-scrcpy/
[article-tcpip]: https://www.genymotion.com/blog/open-source-project-scrcpy-now-works-wirelessly/
