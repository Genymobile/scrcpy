# scrcpy (v1.11)

This document will be updated frequently along with the original Readme file
이 문서는 원어 리드미 파일의 업데이트에 따라 종종 업데이트 될 것입니다

 이 어플리케이션은 UBS ( 혹은 [TCP/IP][article-tcpip] ) 로 연결된 Android 디바이스를 화면에 보여주고 관리하는 것을 제공합니다.
 _GNU/Linux_, _Windows_ 와 _macOS_ 상에서 작동합니다.
 (아래 설명에서 디바이스는 안드로이드 핸드폰을 의미합니다.)

[article-tcpip]:https://www.genymotion.com/blog/open-source-project-scrcpy-now-works-wirelessly/

![screenshot](https://github.com/Genymobile/scrcpy/blob/master/assets/screenshot-debian-600.jpg?raw=true)

주요 기능은 다음과 같습니다.

 - **가벼움** (기본적이며 디바이스의 화면만을 보여줌)
 - **뛰어난 성능** (30~60fps)
 - **높은 품질** (1920×1080 이상의 해상도)
 - **빠른 반응 속도** ([35~70ms][lowlatency])
 - **짧은 부팅 시간** (첫 사진을 보여주는데 최대 1초 소요됨)
 - **장치 설치와는 무관함** (디바이스에 설치하지 않아도 됨)

[lowlatency]: https://github.com/Genymobile/scrcpy/pull/646


## 요구사항

안드로이드 장치는 최소 API 21 (Android 5.0) 을 필요로 합니다.

디바이스에 [adb debugging][enable-adb]이 가능한지 확인하십시오.

[enable-adb]: https://developer.android.com/studio/command-line/adb.html#Enabling

어떤 디바이스에서는, 키보드와 마우스를 사용하기 위해서 [추가 옵션][control] 이 필요하기도 합니다.

[control]: https://github.com/Genymobile/scrcpy/issues/70#issuecomment-373286323


## 앱 설치하기


### Linux (리눅스)

리눅스 상에서는 보통 [어플을 직접 설치][BUILD] 해야합니다. 어렵지 않으므로 걱정하지 않아도 됩니다.

[BUILD]:https://github.com/Genymobile/scrcpy/blob/master/BUILD.md

[Snap] 패키지가 가능합니다 : [`scrcpy`][snap-link].

[snap-link]: https://snapstats.org/snaps/scrcpy

[snap]: https://en.wikipedia.org/wiki/Snappy_(package_manager)

Arch Linux에서, [AUR] 패키지가 가능합니다 : [`scrcpy`][aur-link].

[AUR]: https://wiki.archlinux.org/index.php/Arch_User_Repository
[aur-link]: https://aur.archlinux.org/packages/scrcpy/

Gentoo에서 ,[Ebuild] 가 가능합니다 : [`scrcpy/`][ebuild-link].

[Ebuild]: https://wiki.gentoo.org/wiki/Ebuild
[ebuild-link]: https://github.com/maggu2810/maggu2810-overlay/tree/master/app-mobilephone/scrcpy


### Windows (윈도우)

윈도우 상에서, 간단하게 설치하기 위해 종속성이 있는 사전 구축된 아카이브가 제공됩니다 (`adb` 포함) :
해당 파일은 Readme원본 링크를 통해서 다운로드가 가능합니다.
 - [`scrcpy-win`][direct-win]

[direct-win]: https://github.com/Genymobile/scrcpy/blob/master/README.md#windows


[어플을 직접 설치][BUILD] 할 수도 있습니다.


### macOS (맥 OS)

이 어플리케이션은 아래 사항을 따라 설치한다면 [Homebrew] 에서도 사용 가능합니다 :

[Homebrew]: https://brew.sh/

```bash
brew install scrcpy
```

`PATH` 로부터 접근 가능한 `adb` 가 필요합니다. 아직 설치하지 않았다면 다음을 따라 설치해야 합니다 :

```bash
brew cask install android-platform-tools
```

[어플을 직접 설치][BUILD] 할 수도 있습니다.


## 실행

안드로이드 디바이스를 연결하고 실행하십시오:

```bash
scrcpy
```

다음과 같이 명령창 옵션 기능도 제공합니다.

```bash
scrcpy --help
```

## 기능

### 캡쳐 환경 설정


###사이즈 재정의

가끔씩 성능을 향상시키기위해 안드로이드 디바이스를 낮은 해상도에서 미러링하는 것이 유용할 때도 있습니다.

너비와 높이를 제한하기 위해 특정 값으로 지정할 수 있습니다 (e.g. 1024) :

```bash
scrcpy --max-size 1024
scrcpy -m 1024  # 축약 버전
```

이 외의 크기도 디바이스의 가로 세로 비율이 유지된 상태에서 계산됩니다.
이러한 방식으로 디바이스 상에서 1920×1080 는 모니터 상에서1024×576로 미러링될 것 입니다.


### bit-rate 변경

기본 bit-rate 는 8 Mbps입니다. 비디오 bit-rate 를 변경하기 위해선 다음과 같이 입력하십시오 (e.g. 2 Mbps로 변경):

```bash
scrcpy --bit-rate 2M
scrcpy -b 2M  # 축약 버전
```

###프레임 비율 제한

안드로이드 버전 10이상의 디바이스에서는, 다음의 명령어로 캡쳐 화면의 프레임 비율을 제한할 수 있습니다:

```bash
scrcpy --max-fps 15
```


### Crop (잘라내기)

디바이스 화면은 화면의 일부만 미러링하기 위해 잘라질 것입니다.

예를 들어, *Oculus Go* 의 한 쪽 눈만 미러링할 때 유용합니다 :

```bash
scrcpy --crop 1224:1440:0:0   # 1224x1440 at offset (0,0)
scrcpy -c 1224:1440:0:0       # 축약 버전
```

만약 `--max-size` 도 지정하는 경우, 잘라낸 다음에 재정의된 크기가 적용될 것입니다.


### 화면 녹화

미러링하는 동안 화면 녹화를 할 수 있습니다 :

```bash
scrcpy --record file.mp4
scrcpy -r file.mkv
```

녹화하는 동안 미러링을 멈출 수 있습니다 :

```bash
scrcpy --no-display --record file.mp4
scrcpy -Nr file.mkv
# Ctrl+C 로 녹화를 중단할 수 있습니다.
# 윈도우 상에서 Ctrl+C 는 정상정으로 종료되지 않을 수 있으므로, 디바이스 연결을 해제하십시오.
```

"skipped frames" 은 모니터 화면에 보여지지 않았지만 녹화되었습니다 ( 성능 문제로 인해 ). 프레임은 디바이스 상에서 _타임 스탬프 ( 어느 시점에 데이터가 존재했다는 사실을 증명하기 위해 특정 위치에 시각을 표시 )_ 되었으므로, [packet delay
variation] 은 녹화된 파일에 영향을 끼치지 않습니다.

[packet delay variation]: https://en.wikipedia.org/wiki/Packet_delay_variation

## 연결

### 무선연결

_Scrcpy_ 장치와 정보를 주고받기 위해 `adb` 를 사용합니다.  `adb` 는 TCIP/IP 를 통해 디바이스와 [연결][connect] 할 수 있습니다 :

1. 컴퓨터와 디바이스를 동일한 Wi-Fi 에 연결합니다.
2. 디바이스의 IP address 를 확인합니다 (설정 → 내 기기 → 상태 / 혹은 인터넷에 '내 IP'검색 시 확인 가능합니다. ).
3. TCP/IP 를 통해 디바이스에서 adb 를 사용할 수 있게 합니다: `adb tcpip 5555`.
4. 디바이스 연결을 해제합니다.
5. adb 를 통해 디바이스에 연결을 합니다\: `adb connect DEVICE_IP:5555` _(`DEVICE_IP` 대신)_.
6. `scrcpy` 실행합니다.

다음은 bit-rate 와 해상도를 줄이는데 유용합니다 :

```bash
scrcpy --bit-rate 2M --max-size 800
scrcpy -b2M -m800  # 축약 버전
```

[connect]: https://developer.android.com/studio/command-line/adb.html#wireless



### 여러 디바이스 사용 가능

만약에 여러 디바이스들이 `adb devices` 목록에 표시되었다면, _serial_ 을 명시해야합니다:

```bash
scrcpy --serial 0123456789abcdef
scrcpy -s 0123456789abcdef  # 축약 버전
```

_scrcpy_ 로 여러 디바이스를 연결해 사용할 수 있습니다.


#### SSH tunnel

떨어져 있는 디바이스와 연결하기 위해서는, 로컬 `adb` client와 떨어져 있는 `adb` 서버를 연결해야 합니다.  (디바이스와 클라이언트가 동일한 버전의 _adb_ protocol을 사용할 경우에 제공됩니다.):

```bash
adb kill-server    # 5037의 로컬 local adb server를 중단
ssh -CN -L5037:localhost:5037 -R27183:localhost:27183 your_remote_computer
# 실행 유지
```

다른 터미널에서는 :

```bash
scrcpy
```

무선 연결과 동일하게, 화질을 줄이는 것이 나을 수 있습니다:

```
scrcpy -b2M -m800 --max-fps 15
```

## Window에서의 배치

### 맞춤형 window 제목

기본적으로, window의 이름은 디바이스의 모델명 입니다.
다음의 명령어를 통해 변경하세요.

```bash
scrcpy --window-title 'My device'
```


### 배치와 크기

초기 window창의 배치와 크기는 다음과 같이 설정할 수 있습니다:

```bash
scrcpy --window-x 100 --window-y 100 --window-width 800 --window-height 600
```


### 경계 없애기

윈도우 장식(경계선 등)을 다음과 같이 제거할 수 있습니다:

```bash
scrcpy --window-borderless
```

### 항상 모든 윈도우 위에 실행창 고정

이 어플리케이션의 윈도우 창은 다음의 명령어로 다른 window 위에 디스플레이 할 수 있습니다:

```bash
scrcpy --always-on-top
scrcpy -T  # 축약 버전
```

### 전체 화면

이 어플리케이션은 전체화면으로 바로 시작할 수 있습니다.

```bash
scrcpy --fullscreen
scrcpy -f  # short version
```

전체 화면은  `Ctrl`+`f`키로 끄거나 켤 수 있습니다.


## 다른 미러링 옵션

### 읽기 전용(Read-only)

권한을 제한하기 위해서는 (디바이스와 관련된 모든 것: 입력 키, 마우스 이벤트 , 파일의 드래그 앤 드랍(drag&drop)):

```bash
scrcpy --no-control
scrcpy -n
```

### 화면 끄기

미러링을 실행하는 와중에 디바이스의 화면을 끌 수 있게 하기 위해서는
다음의 커맨드 라인 옵션을(command line option) 입력하세요:

```bash
scrcpy --turn-screen-off
scrcpy -S
```

혹은 `Ctrl`+`o`을 눌러 언제든지 디바이스의 화면을 끌 수 있습니다.

다시 화면을 켜기 위해서는`POWER` (혹은 `Ctrl`+`p`)를 누르세요.


### 유효기간이 지난 프레임 제공 (Render expired frames)

디폴트로, 대기시간을 최소화하기 위해 _scrcpy_ 는 항상 마지막으로 디코딩된 프레임을 제공합니다
과거의 프레임은 하나씩 삭제합니다.

모든 프레임을 강제로 렌더링하기 위해서는 (대기 시간이 증가될 수 있습니다)
다음의 명령어를 사용하세요:

```bash
scrcpy --render-expired-frames
```


### 화면에 터치 나타내기

발표를 할 때, 물리적인 기기에 한 물리적 터치를 나타내는 것이 유용할 수 있습니다.

안드로이드 운영체제는 이런 기능을 _Developers options_에서 제공합니다.

_Scrcpy_ 는 이런 기능을 시작할 때와 종료할 때 옵션으로 제공합니다.

```bash
scrcpy --show-touches
scrcpy -t
```

화면에 _물리적인 터치만_ 나타나는 것에 유의하세요 (손가락을 디바이스에 대는 행위).


### 입력 제어

#### 복사-붙여넣기

컴퓨터와 디바이스 양방향으로 클립보드를 복사하는 것이 가능합니다:

 - `Ctrl`+`c` 디바이스의 클립보드를 컴퓨터로 복사합니다;
 - `Ctrl`+`Shift`+`v` 컴퓨터의 클립보드를 디바이스로 복사합니다;
 - `Ctrl`+`v` 컴퓨터의 클립보드를 text event 로써 _붙여넣습니다_  ( 그러나, ASCII 코드가 아닌 경우 실행되지 않습니다 )

#### 텍스트 삽입 우선 순위

텍스트를 입력할 때 생성되는 두 가지의 [events][textevents] 가 있습니다:
 - _key events_, 키가 눌려있는 지에 대한 신호;
 - _text events_, 텍스트가 입력되었는지에 대한 신호.

기본적으로, 글자들은 key event 를 이용해 입력되기 때문에, 키보드는 게임에서처럼 처리합니다 ( 보통 WASD 키에 대해서 ).

그러나 이는 [issues 를 발생][prefertext]시킵니다. 이와 관련된 문제를 접할 경우, 아래와 같이 피할 수 있습니다:

```bash
scrcpy --prefer-text
```

( 그러나 이는 게임에서의 처리를 중단할 수 있습니다 )

[textevents]: https://blog.rom1v.com/2018/03/introducing-scrcpy/#handle-text-input
[prefertext]: https://github.com/Genymobile/scrcpy/issues/650#issuecomment-512945343


### 파일 드랍

### APK 실행하기

APK를 실행하기 위해서는, APK file(파일명이`.apk`로 끝나는 파일)을  드래그하고 _scrcpy_ window에 드랍하세요 (drag and drop)

시각적인 피드백은 없고,log 하나가 콘솔에 출력될 것입니다.

### 디바이스에 파일 push하기

디바이스의`/sdcard/`에 파일을 push하기 위해서는,
APK파일이 아닌 파일을_scrcpy_ window에 드래그하고 드랍하세요.(drag and drop).

시각적인 피드백은 없고,log 하나가 콘솔에 출력될 것입니다.

해당 디렉토리는 시작할 때 변경이 가능합니다:

```bash
scrcpy --push-target /sdcard/foo/bar/
```

### 오디오의 전달

_scrcpy_는 오디오를 직접 전달해주지 않습니다. [USBaudio] (Linux-only)를 사용하세요.

추가적으로 [issue #14]를 참고하세요.

[USBaudio]: https://github.com/rom1v/usbaudio
[issue #14]: https://github.com/Genymobile/scrcpy/issues/14

## 단축키

 | 실행내용                                |   단축키                       |   단축키 (macOS)
 | -------------------------------------- |:----------------------------- |:-----------------------------
 | 전체화면 모드로 전환                      | `Ctrl`+`f`                    | `Cmd`+`f`
 | window를 1:1비율로 전환하기(픽셀 맞춤)   | `Ctrl`+`g`                    | `Cmd`+`g`
 | 검은 공백 제거 위한 window 크기 조정  | `Ctrl`+`x` \| _Double-click¹_ | `Cmd`+`x`  \| _Double-click¹_
 |`HOME` 클릭                        | `Ctrl`+`h` \| _Middle-click_  | `Ctrl`+`h` \| _Middle-click_
 | `BACK` 클릭                      | `Ctrl`+`b` \| _Right-click²_  | `Cmd`+`b`  \| _Right-click²_
 | `APP_SWITCH` 클릭                 | `Ctrl`+`s`                    | `Cmd`+`s`
 | `MENU` 클릭                       | `Ctrl`+`m`                    | `Ctrl`+`m`
 | `VOLUME_UP` 클릭                   | `Ctrl`+`↑` _(up)_             | `Cmd`+`↑` _(up)_
 | `VOLUME_DOWN` 클릭                | `Ctrl`+`↓` _(down)_           | `Cmd`+`↓` _(down)_
 | `POWER` 클릭                      | `Ctrl`+`p`                    | `Cmd`+`p`
 | 전원 켜기                               | _Right-click²_                | _Right-click²_
 | 미러링 중 디바이스 화면 끄기    | `Ctrl`+`o`                    | `Cmd`+`o`
 | 알림 패널 늘리기               | `Ctrl`+`n`                    | `Cmd`+`n`
 | 알림 패널 닫기            | `Ctrl`+`Shift`+`n`            | `Cmd`+`Shift`+`n`
 | 디바이스의 clipboard 컴퓨터로 복사하기      | `Ctrl`+`c`                    | `Cmd`+`c`
 | 컴퓨터의 clipboard 디바이스에 붙여넣기     | `Ctrl`+`v`                    | `Cmd`+`v`
 | Copy computer clipboard to device      | `Ctrl`+`Shift`+`v`            | `Cmd`+`Shift`+`v`
 | Enable/disable FPS counter (on stdout) | `Ctrl`+`i`                    | `Cmd`+`i`

_¹검은 공백을 제거하기 위해서는 그 부분을 더블 클릭하세요_
_²화면이 꺼진 상태에서 우클릭 시 다시 켜지며, 그 외의 상태에서는 뒤로 돌아갑니다.

## 맞춤 경로 (custom path)

특정한 _adb_ binary를 사용하기 위해서는, 그것의 경로를 환경변수로 설정하세요.
`ADB`:

    ADB=/path/to/adb scrcpy

`scrcpy-server.jar`파일의 경로에 오버라이드 하기 위해서는, 그것의 경로를 `SCRCPY_SERVER_PATH`에 저장하세요.

[useful]: https://github.com/Genymobile/scrcpy/issues/278#issuecomment-429330345


## _scrcpy_ 인 이유?

한 동료가 [gnirehtet]와 같이 발음하기 어려운 이름을 찾을 수 있는지 도발했습니다.

[`strcpy`] 는 **str**ing을 copy하고; `scrcpy`는 **scr**een을 copy합니다.

[gnirehtet]: https://github.com/Genymobile/gnirehtet
[`strcpy`]: http://man7.org/linux/man-pages/man3/strcpy.3.html



## 빌드하는 방법?

[BUILD]을 참고하세요.

[BUILD]: BUILD.md

## 흔한 issue

[FAQ](FAQ.md)을 참고하세요.


## 개발자들

[developers page]를 참고하세요.

[developers page]: DEVELOP.md


## 라이선스

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

## 관련 글 (articles)

- [scrcpy 소개][article-intro]
- [무선으로 연결하는 Scrcpy][article-tcpip]

[article-intro]: https://blog.rom1v.com/2018/03/introducing-scrcpy/
[article-tcpip]: https://www.genymotion.com/blog/open-source-project-scrcpy-now-works-wirelessly/
