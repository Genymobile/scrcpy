# 자주하는 질문 (FAQ)

다음은 자주 제보되는 문제들과 그들의 현황입니다.


### Window 운영체제에서, 디바이스가 발견되지 않습니다.

가장 흔한 제보는 `adb`에 발견되지 않는 디바이스 혹은 권한 관련 문제입니다.
다음 명령어를 호출하여 모든 것들에 이상이 없는지 확인하세요:

    adb devices

Window는 당신의 디바이스를 감지하기 위해 [drivers]가 필요할 수도 있습니다.

[drivers]: https://developer.android.com/studio/run/oem-usb.html


### 내 디바이스의 미러링만 가능하고, 디바이스와 상호작용을 할 수 없습니다.

일부 디바이스에서는, [simulating input]을 허용하기 위해서 한가지 옵션을 활성화해야 할 수도 있습니다.
개발자 옵션에서 (developer options) 다음을 활성화 하세요:

> **USB debugging (Security settings)**
> _권한 부여와 USB 디버깅을 통한 simulating input을 허용한다_

[simulating input]: https://github.com/Genymobile/scrcpy/issues/70#issuecomment-373286323


### 마우스 클릭이 다른 곳에 적용됩니다.

Mac 운영체제에서, HiDPI support 와 여러 스크린 창이 있는 경우, 입력 위치가 잘못 파악될 수 있습니다.
[issue 15]를 참고하세요.

[issue 15]: https://github.com/Genymobile/scrcpy/issues/15

차선책은 HiDPI support을 비활성화 하고 build하는 방법입니다:

```bash
meson x --buildtype release -Dhidpi_support=false
```

하지만, 동영상은 낮은 해상도로 재생될 것 입니다.


### HiDPI display의 화질이 낮습니다.

Windows에서는, [scaling behavior] 환경을 설정해야 할 수도 있습니다.

> `scrcpy.exe` > Properties > Compatibility > Change high DPI settings >
> Override high DPI scaling behavior > Scaling performed by: _Application_.

[scaling behavior]: https://github.com/Genymobile/scrcpy/issues/40#issuecomment-424466723


### KWin compositor가 실행되지 않습니다

Plasma Desktop에서는,_scrcpy_ 가 실행중에는 compositor가 비활성화 됩니다.

차석책으로는, ["Block compositing"를 비활성화하세요][kwin].

[kwin]: https://github.com/Genymobile/scrcpy/issues/114#issuecomment-378778613


###비디오 스트림을 열 수 없는 에러가 발생합니다.(Could not open video stream).

여러가지 원인이 있을 수 있습니다. 가장 흔한 원인은 디바이스의 하드웨어 인코더(hardware encoder)가
주어진 해상도를 인코딩할 수 없는 경우입니다.

```
ERROR: Exception on thread Thread[main,5,main]
android.media.MediaCodec$CodecException: Error 0xfffffc0e
...
Exit due to uncaughtException in main thread:
ERROR: Could not open video stream
INFO: Initial texture: 1080x2336
```

더 낮은 해상도로 시도 해보세요:

```
scrcpy -m 1920
scrcpy -m 1024
scrcpy -m 800
```
