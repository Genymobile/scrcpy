# scrcpy (v1.18)

Bu uygulama Android cihazların USB (ya da [TCP/IP][article-tcpip]) üzerinden
görüntülenmesini ve kontrol edilmesini sağlar. _root_ erişimine ihtiyaç duymaz.
_GNU/Linux_, _Windows_ ve _macOS_ sistemlerinde çalışabilir.

![screenshot](assets/screenshot-debian-600.jpg)

Öne çıkan özellikler:

- **hafiflik** (doğal, sadece cihazın ekranını gösterir)
- **performans** (30~60fps)
- **kalite** (1920×1080 ya da üzeri)
- **düşük gecikme süresi** ([35~70ms][lowlatency])
- **düşük başlangıç süresi** (~1 saniye ilk kareyi gösterme süresi)
- **müdaheleci olmama** (cihazda kurulu yazılım kalmaz)

[lowlatency]: https://github.com/Genymobile/scrcpy/pull/646

## Gereksinimler

Android cihaz en düşük API 21 (Android 5.0) olmalıdır.

[Adb hata ayıklamasının][enable-adb] cihazınızda aktif olduğundan emin olun.

[enable-adb]: https://developer.android.com/studio/command-line/adb.html#Enabling

Bazı cihazlarda klavye ve fare ile kontrol için [ilave bir seçenek][control] daha
etkinleştirmeniz gerekebilir.

[control]: https://github.com/Genymobile/scrcpy/issues/70#issuecomment-373286323

## Uygulamayı indirin

<a href="https://repology.org/project/scrcpy/versions"><img src="https://repology.org/badge/vertical-allrepos/scrcpy.svg" alt="Packaging status" align="right"></a>

### Özet

- Linux: `apt install scrcpy`
- Windows: [indir][direct-win64]
- macOS: `brew install scrcpy`

Kaynak kodu derle: [BUILD] ([basitleştirilmiş süreç][build_simple])

[build]: BUILD.md
[build_simple]: BUILD.md#simple

### Linux

Debian (şimdilik _testing_ ve _sid_) ve Ubuntu (20.04) için:

```
apt install scrcpy
```

[Snap] paketi: [`scrcpy`][snap-link].

[snap-link]: https://snapstats.org/snaps/scrcpy
[snap]: https://en.wikipedia.org/wiki/Snappy_(package_manager)

Fedora için, [COPR] paketi: [`scrcpy`][copr-link].

[copr]: https://fedoraproject.org/wiki/Category:Copr
[copr-link]: https://copr.fedorainfracloud.org/coprs/zeno/scrcpy/

Arch Linux için, [AUR] paketi: [`scrcpy`][aur-link].

[aur]: https://wiki.archlinux.org/index.php/Arch_User_Repository
[aur-link]: https://aur.archlinux.org/packages/scrcpy/

Gentoo için, [Ebuild] mevcut: [`scrcpy/`][ebuild-link].

[ebuild]: https://wiki.gentoo.org/wiki/Ebuild
[ebuild-link]: https://github.com/maggu2810/maggu2810-overlay/tree/master/app-mobilephone/scrcpy

Ayrıca [uygulamayı el ile de derleyebilirsiniz][build] ([basitleştirilmiş süreç][build_simple]).

### Windows

Windows için (`adb` dahil) tüm gereksinimleri ile derlenmiş bir arşiv mevcut:

 - [README](README.md#windows)

[Chocolatey] ile kurulum:

[chocolatey]: https://chocolatey.org/

```bash
choco install scrcpy
choco install adb    # if you don't have it yet
```

[Scoop] ile kurulum:

```bash
scoop install scrcpy
scoop install adb    # if you don't have it yet
```

[scoop]: https://scoop.sh

Ayrıca [uygulamayı el ile de derleyebilirsiniz][build].

### macOS

Uygulama [Homebrew] içerisinde mevcut. Sadece kurun:

[homebrew]: https://brew.sh/

```bash
brew install scrcpy
```

`adb`, `PATH` içerisinden erişilebilir olmalıdır. Eğer değilse:

```bash
brew install android-platform-tools
```

[MacPorts] kullanılarak adb ve uygulamanın birlikte kurulumu yapılabilir:

```bash
sudo port install scrcpy
```

[macports]: https://www.macports.org/

Ayrıca [uygulamayı el ile de derleyebilirsiniz][build].

## Çalıştırma

Android cihazınızı bağlayın ve aşağıdaki komutu çalıştırın:

```bash
scrcpy
```

Komut satırı argümanları aşağıdaki komut ile listelenebilir:

```bash
scrcpy --help
```

## Özellikler

### Ekran yakalama ayarları

#### Boyut azaltma

Bazen, Android cihaz ekranını daha düşük seviyede göstermek performansı artırabilir.

Hem genişliği hem de yüksekliği bir değere sabitlemek için (ör. 1024):

```bash
scrcpy --max-size 1024
scrcpy -m 1024  # kısa versiyon
```

Diğer boyut en-boy oranı korunacak şekilde hesaplanır.
Bu şekilde ekran boyutu 1920x1080 olan bir cihaz 1024x576 olarak görünür.

#### Bit-oranı değiştirme

Varsayılan bit-oranı 8 Mbps'dir. Değiştirmek için (ör. 2 Mbps):

```bash
scrcpy --bit-rate 2M
scrcpy -b 2M  # kısa versiyon
```

#### Çerçeve oranı sınırlama

Ekran yakalama için maksimum çerçeve oranı için sınır koyulabilir:

```bash
scrcpy --max-fps 15
```

Bu özellik Android 10 ve sonrası sürümlerde resmi olarak desteklenmektedir,
ancak daha önceki sürümlerde çalışmayabilir.

#### Kesme

Cihaz ekranının sadece bir kısmı görünecek şekilde kesilebilir.

Bu özellik Oculus Go'nun bir gözünü yakalamak gibi durumlarda kullanışlı olur:

```bash
scrcpy --crop 1224:1440:0:0   # (0,0) noktasından 1224x1440
```

Eğer `--max-size` belirtilmişse yeniden boyutlandırma kesme işleminden sonra yapılır.

#### Video yönünü kilitleme

Videonun yönünü kilitlemek için:

```bash
scrcpy --lock-video-orientation     # başlangıç yönü
scrcpy --lock-video-orientation=0   # doğal yön
scrcpy --lock-video-orientation=1   # 90° saatin tersi yönü
scrcpy --lock-video-orientation=2   # 180°
scrcpy --lock-video-orientation=3   # 90° saat yönü
```

Bu özellik kaydetme yönünü de etkiler.

[Pencere ayrı olarak döndürülmüş](#rotation) olabilir.

#### Kodlayıcı

Bazı cihazlar birden fazla kodlayıcıya sahiptir, ve bunların bazıları programın
kapanmasına sebep olabilir. Bu durumda farklı bir kodlayıcı seçilebilir:

```bash
scrcpy --encoder OMX.qcom.video.encoder.avc
```

Mevcut kodlayıcıları listelemek için geçerli olmayan bir kodlayıcı ismi girebilirsiniz,
hata mesajı mevcut kodlayıcıları listeleyecektir:

```bash
scrcpy --encoder _
```

### Yakalama

#### Kaydetme

Ekran yakalama sırasında kaydedilebilir:

```bash
scrcpy --record file.mp4
scrcpy -r file.mkv
```

Yakalama olmadan kayıt için:

```bash
scrcpy --no-display --record file.mp4
scrcpy -Nr file.mkv
# Ctrl+C ile kayıt kesilebilir
```

"Atlanan kareler" gerçek zamanlı olarak gösterilmese (performans sebeplerinden ötürü) dahi kaydedilir.
Kareler cihazda _zamandamgası_ ile saklanır, bu sayede [paket gecikme varyasyonu]
kayıt edilen dosyayı etkilemez.

[paket gecikme varyasyonu]: https://en.wikipedia.org/wiki/Packet_delay_variation

#### v4l2loopback

Linux'ta video akışı bir v4l2 loopback cihazına gönderilebilir. Bu sayede Android
cihaz bir web kamerası gibi davranabilir.

Bu işlem için `v4l2loopback` modülü kurulu olmalıdır:

```bash
sudo apt install v4l2loopback-dkms
```

v4l2 cihazı oluşturmak için:

```bash
sudo modprobe v4l2loopback
```

Bu komut `/dev/videoN` adresinde `N` yerine bir tamsayı koyarak yeni bir video
cihazı oluşturacaktır.
(birden fazla cihaz oluşturmak veya spesifik ID'ye sahip cihazlar için
diğer [seçenekleri](https://github.com/umlaeute/v4l2loopback#options) inceleyebilirsiniz.)

Aktif cihazları listelemek için:

```bash
# v4l-utils paketi ile
v4l2-ctl --list-devices

# daha basit ama yeterli olabilecek şekilde
ls /dev/video*
```

v4l2 kullanarak scrpy kullanmaya başlamak için:

```bash
scrcpy --v4l2-sink=/dev/videoN
scrcpy --v4l2-sink=/dev/videoN --no-display  # ayna penceresini kapatarak
scrcpy --v4l2-sink=/dev/videoN -N            # kısa versiyon
```

(`N` harfini oluşturulan cihaz ID numarası ile değiştirin. `ls /dev/video*` cihaz ID'lerini görebilirsiniz.)

Aktifleştirildikten sonra video akışını herhangi bir v4l2 özellikli araçla açabilirsiniz:

```bash
ffplay -i /dev/videoN
vlc v4l2:///dev/videoN   # VLC kullanırken yükleme gecikmesi olabilir
```

Örneğin, [OBS] ile video akışını kullanabilirsiniz.

[obs]: https://obsproject.com/

### Bağlantı

#### Kablosuz

_Scrcpy_ cihazla iletişim kurmak için `adb`'yi kullanır, Ve `adb`
bir cihaza TCP/IP kullanarak [bağlanabilir].

1. Cihazınızı bilgisayarınızla aynı Wi-Fi ağına bağlayın.
2. Cihazınızın IP adresini bulun. Ayarlar → Telefon Hakkında → Durum sekmesinden veya
   aşağıdaki komutu çalıştırarak öğrenebilirsiniz:

   ```bash
   adb shell ip route | awk '{print $9}'
   ```

3. Cihazınızda TCP/IP üzerinden adb kullanımını etkinleştirin: `adb tcpip 5555`.
4. Cihazınızı bilgisayarınızdan sökün.
5. Cihazınıza bağlanın: `adb connect DEVICE_IP:5555` _(`DEVICE_IP` değerini değiştirin)_.
6. `scrcpy` komutunu normal olarak çalıştırın.

Bit-oranını ve büyüklüğü azaltmak yararlı olabilir:

```bash
scrcpy --bit-rate 2M --max-size 800
scrcpy -b2M -m800  # kısa version
```

[bağlanabilir]: https://developer.android.com/studio/command-line/adb.html#wireless

#### Birden fazla cihaz

Eğer `adb devices` komutu birden fazla cihaz listeliyorsa _serial_ değerini belirtmeniz gerekir:

```bash
scrcpy --serial 0123456789abcdef
scrcpy -s 0123456789abcdef  # kısa versiyon
```

Eğer cihaz TCP/IP üzerinden bağlanmışsa:

```bash
scrcpy --serial 192.168.0.1:5555
scrcpy -s 192.168.0.1:5555  # kısa version
```

Birden fazla cihaz için birden fazla _scrcpy_ uygulaması çalıştırabilirsiniz.

#### Cihaz bağlantısı ile otomatik başlatma

[AutoAdb] ile yapılabilir:

```bash
autoadb scrcpy -s '{}'
```

[autoadb]: https://github.com/rom1v/autoadb

#### SSH Tünel

Uzaktaki bir cihaza erişmek için lokal `adb` istemcisi, uzaktaki bir `adb` sunucusuna
(aynı _adb_ sürümünü kullanmak şartı ile) bağlanabilir :

```bash
adb kill-server    # 5037 portunda çalışan lokal adb sunucusunu kapat
ssh -CN -L5037:localhost:5037 -R27183:localhost:27183 your_remote_computer
# bunu açık tutun
```

Başka bir terminalde:

```bash
scrcpy
```

Uzaktan port yönlendirme ileri yönlü bağlantı kullanabilirsiniz
(`-R` yerine `-L` olduğuna dikkat edin):

```bash
adb kill-server    # 5037 portunda çalışan lokal adb sunucusunu kapat
ssh -CN -L5037:localhost:5037 -L27183:localhost:27183 your_remote_computer
# bunu açık tutun
```

Başka bir terminalde:

```bash
scrcpy --force-adb-forward
```

Kablosuz bağlantı gibi burada da kalite düşürmek faydalı olabilir:

```
scrcpy -b2M -m800 --max-fps 15
```

### Pencere ayarları

#### İsim

Cihaz modeli varsayılan pencere ismidir. Değiştirmek için:

```bash
scrcpy --window-title 'Benim cihazım'
```

#### Konum ve

Pencerenin başlangıç konumu ve boyutu belirtilebilir:

```bash
scrcpy --window-x 100 --window-y 100 --window-width 800 --window-height 600
```

#### Kenarlıklar

Pencere dekorasyonunu kapatmak için:

```bash
scrcpy --window-borderless
```

#### Her zaman üstte

Scrcpy penceresini her zaman üstte tutmak için:

```bash
scrcpy --always-on-top
```

#### Tam ekran

Uygulamayı tam ekran başlatmak için:

```bash
scrcpy --fullscreen
scrcpy -f  # kısa versiyon
```

Tam ekran <kbd>MOD</kbd>+<kbd>f</kbd> ile dinamik olarak değiştirilebilir.

#### Döndürme

Pencere döndürülebilir:

```bash
scrcpy --rotation 1
```

Seçilebilecek değerler:

- `0`: döndürme yok
- `1`: 90 derece saat yönünün tersi
- `2`: 180 derece
- `3`: 90 derece saat yönü

Döndürme <kbd>MOD</kbd>+<kbd>←</kbd>_(sol)_ ve
<kbd>MOD</kbd>+<kbd>→</kbd> _(sağ)_ ile dinamik olarak değiştirilebilir.

_scrcpy_'de 3 farklı döndürme olduğuna dikkat edin:

- <kbd>MOD</kbd>+<kbd>r</kbd> cihazın yatay veya dikey modda çalışmasını sağlar.
  (çalışan uygulama istenilen oryantasyonda çalışmayı desteklemiyorsa döndürme
  işlemini reddedebilir.)
- [`--lock-video-orientation`](#lock-video-orientation) görüntü yakalama oryantasyonunu
  (cihazdan bilgisayara gelen video akışının oryantasyonu) değiştirir. Bu kayıt işlemini
  etkiler.
- `--rotation` (or <kbd>MOD</kbd>+<kbd>←</kbd>/<kbd>MOD</kbd>+<kbd>→</kbd>)
  pencere içeriğini dönderir. Bu sadece canlı görüntüyü etkiler, kayıt işlemini etkilemez.

### Diğer ekran yakalama seçenekleri

#### Yazma korumalı

Kontrolleri devre dışı bırakmak için (cihazla etkileşime geçebilecek her şey: klavye ve
fare girdileri, dosya sürükleyip bırakma):

```bash
scrcpy --no-control
scrcpy -n
```

#### Ekran

Eğer cihazın birden fazla ekranı varsa hangi ekranın kullanılacağını seçebilirsiniz:

```bash
scrcpy --display 1
```

Kullanılabilecek ekranları listelemek için:

```bash
adb shell dumpsys display   # çıktı içerisinde "mDisplayId=" terimini arayın
```

İkinci ekran ancak cihaz Android sürümü 10 veya üzeri olmalıdır (değilse yazma korumalı
olarak görüntülenir).

#### Uyanık kalma

Cihazın uyku moduna girmesini engellemek için:

```bash
scrcpy --stay-awake
scrcpy -w
```

scrcpy kapandığında cihaz başlangıç durumuna geri döner.

#### Ekranı kapatma

Ekran yakalama sırasında cihazın ekranı kapatılabilir:

```bash
scrcpy --turn-screen-off
scrcpy -S
```

Ya da <kbd>MOD</kbd>+<kbd>o</kbd> kısayolunu kullanabilirsiniz.

Tekrar açmak için ise <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>o</kbd> tuşlarına basın.

Android'de, `GÜÇ` tuşu her zaman ekranı açar. Eğer `GÜÇ` sinyali scrcpy ile
gönderilsiyse (sağ tık veya <kbd>MOD</kbd>+<kbd>p</kbd>), ekran kısa bir gecikme
ile kapanacaktır. Fiziksel `GÜÇ` tuşuna basmak hala ekranın açılmasına sebep olacaktır.

Bu cihazın uykuya geçmesini engellemek için kullanılabilir:

```bash
scrcpy --turn-screen-off --stay-awake
scrcpy -Sw
```

#### Dokunuşları gösterme

Sunumlar sırasında fiziksel dokunuşları (fiziksel cihazdaki) göstermek
faydalı olabilir.

Android'de bu özellik _Geliştici seçenekleri_ içerisinde bulunur.

_Scrcpy_ bu özelliği çalışırken etkinleştirebilir ve kapanırken eski
haline geri getirebilir:

```bash
scrcpy --show-touches
scrcpy -t
```

Bu opsiyon sadece _fiziksel_ dokunuşları (cihaz ekranındaki) gösterir.

#### Ekran koruyucuyu devre dışı bırakma

Scrcpy varsayılan ayarlarında ekran koruyucuyu devre dışı bırakmaz.

Bırakmak için:

```bash
scrcpy --disable-screensaver
```

### Girdi kontrolü

#### Cihaz ekranını dönderme

<kbd>MOD</kbd>+<kbd>r</kbd> tuşları ile yatay ve dikey modlar arasında
geçiş yapabilirsiniz.

Bu kısayol ancak çalışan uygulama desteklediği takdirde ekranı döndürecektir.

#### Kopyala yapıştır

Ne zaman Android cihazdaki pano değişse bilgisayardaki pano otomatik olarak
senkronize edilir.

Tüm <kbd>Ctrl</kbd> kısayolları cihaza iletilir:

- <kbd>Ctrl</kbd>+<kbd>c</kbd> genelde kopyalar
- <kbd>Ctrl</kbd>+<kbd>x</kbd> genelde keser
- <kbd>Ctrl</kbd>+<kbd>v</kbd> genelde yapıştırır (bilgisayar ve cihaz arasındaki
  pano senkronizasyonundan sonra)

Bu kısayollar genelde beklediğiniz gibi çalışır.

Ancak kısayolun gerçekten yaptığı eylemi açık olan uygulama belirler.
Örneğin, _Termux_ <kbd>Ctrl</kbd>+<kbd>c</kbd> ile kopyalama yerine
SIGINT sinyali gönderir, _K-9 Mail_ ise yeni mesaj oluşturur.

Bu tip durumlarda kopyalama, kesme ve yapıştırma için (Android versiyon 7 ve
üstü):

- <kbd>MOD</kbd>+<kbd>c</kbd> `KOPYALA`
- <kbd>MOD</kbd>+<kbd>x</kbd> `KES`
- <kbd>MOD</kbd>+<kbd>v</kbd> `YAPIŞTIR` (bilgisayar ve cihaz arasındaki
  pano senkronizasyonundan sonra)

Bunlara ek olarak, <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd> tuşları
bilgisayar pano içeriğini tuş basma eylemleri şeklinde gönderir. Bu metin
yapıştırmayı desteklemeyen (_Termux_ gibi) uygulamar için kullanışlıdır,
ancak ASCII olmayan içerikleri bozabilir.

**UYARI:** Bilgisayar pano içeriğini cihaza yapıştırmak
(<kbd>Ctrl</kbd>+<kbd>v</kbd> ya da <kbd>MOD</kbd>+<kbd>v</kbd> tuşları ile)
içeriği cihaz panosuna kopyalar. Sonuç olarak, herhangi bir Android uygulaması
içeriğe erişebilir. Hassas içerikler (parolalar gibi) için bu özelliği kullanmaktan
kaçının.

Bazı cihazlar pano değişikleri konusunda beklenilen şekilde çalışmayabilir.
Bu durumlarda `--legacy-paste` argümanı kullanılabilir. Bu sayede
<kbd>Ctrl</kbd>+<kbd>v</kbd> ve <kbd>MOD</kbd>+<kbd>v</kbd> tuşları da
pano içeriğini tuş basma eylemleri şeklinde gönderir
(<kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd> ile aynı şekilde).

#### İki parmak ile yakınlaştırma

"İki parmak ile yakınlaştırma" için: <kbd>Ctrl</kbd>+_tıkla-ve-sürükle_.

Daha açıklayıcı şekilde, <kbd>Ctrl</kbd> tuşuna sol-tık ile birlikte basılı
tutun. Sol-tık serbest bırakılıncaya kadar yapılan tüm fare hareketleri
ekran içeriğini ekranın merkezini baz alarak dönderir, büyütür veya küçültür
(eğer uygulama destekliyorsa).

Scrcpy ekranın merkezinde bir "sanal parmak" varmış gibi davranır.

#### Metin gönderme tercihi

Metin girilirken ili çeşit [eylem][textevents] gerçekleştirilir:

- _tuş eylemleri_, bir tuşa basıldığı sinyalini verir;
- _metin eylemleri_, bir metin girildiği sinyalini verir.

Varsayılan olarak, harfler tuş eylemleri kullanılarak gönderilir. Bu sayede
klavye oyunlarda beklenilene uygun olarak çalışır (Genelde WASD tuşları).

Ancak bu [bazı problemlere][prefertext] yol açabilir. Eğer bu problemler ile
karşılaşırsanız metin eylemlerini tercih edebilirsiniz:

```bash
scrcpy --prefer-text
```

(Ama bu oyunlardaki klavye davranışlarını bozacaktır)

[textevents]: https://blog.rom1v.com/2018/03/introducing-scrcpy/#handle-text-input
[prefertext]: https://github.com/Genymobile/scrcpy/issues/650#issuecomment-512945343

#### Tuş tekrarı

Varsayılan olarak, bir tuşa basılı tutmak tuş eylemini tekrarlar. Bu durum
bazı oyunlarda problemlere yol açabilir.

Tuş eylemlerinin tekrarını kapatmak için:

```bash
scrcpy --no-key-repeat
```

#### Sağ-tık ve Orta-tık

Varsayılan olarak, sağ-tık GERİ (ya da GÜÇ açma) eylemlerini, orta-tık ise
ANA EKRAN eylemini tetikler. Bu kısayolları devre dışı bırakmak için:

```bash
scrcpy --forward-all-clicks
```

### Dosya bırakma

#### APK kurulumu

APK kurmak için, bilgisayarınızdaki APK dosyasını (`.apk` ile biten) _scrcpy_
penceresine sürükleyip bırakın.

Bu eylem görsel bir geri dönüt oluşturmaz, konsola log yazılır.

#### Dosyayı cihaza gönderme

Bir dosyayı cihazdaki `/sdcard/Download/` dizinine atmak için, (APK olmayan)
bir dosyayı _scrcpy_ penceresine sürükleyip bırakın.

Bu eylem görsel bir geri dönüt oluşturmaz, konsola log yazılır.

Hedef dizin uygulama başlatılırken değiştirilebilir:

```bash
scrcpy --push-target=/sdcard/Movies/
```

### Ses iletimi

_Scrcpy_ ses iletimi yapmaz. Yerine [sndcpy] kullanabilirsiniz.

Ayrıca bakınız [issue #14].

[sndcpy]: https://github.com/rom1v/sndcpy
[issue #14]: https://github.com/Genymobile/scrcpy/issues/14

## Kısayollar

Aşağıdaki listede, <kbd>MOD</kbd> kısayol tamamlayıcısıdır. Varsayılan olarak
(sol) <kbd>Alt</kbd> veya (sol) <kbd>Super</kbd> tuşudur.

Bu tuş `--shortcut-mod` argümanı kullanılarak `lctrl`, `rctrl`,
`lalt`, `ralt`, `lsuper` ve `rsuper` tuşlarından biri ile değiştirilebilir.
Örneğin:

```bash
# Sağ Ctrl kullanmak için
scrcpy --shortcut-mod=rctrl

# Sol Ctrl, Sol Alt veya Sol Super tuşlarından birini kullanmak için
scrcpy --shortcut-mod=lctrl+lalt,lsuper
```

_<kbd>[Super]</kbd> tuşu genelde <kbd>Windows</kbd> veya <kbd>Cmd</kbd> tuşudur._

[super]: https://en.wikipedia.org/wiki/Super_key_(keyboard_button)

| Action                                           | Shortcut                                                  |
| ------------------------------------------------ | :-------------------------------------------------------- |
| Tam ekran modunu değiştirme                      | <kbd>MOD</kbd>+<kbd>f</kbd>                               |
| Ekranı sola çevirme                              | <kbd>MOD</kbd>+<kbd>←</kbd> _(sol)_                       |
| Ekranı sağa çevirme                              | <kbd>MOD</kbd>+<kbd>→</kbd> _(sağ)_                       |
| Pencereyi 1:1 oranına çevirme (pixel-perfect)    | <kbd>MOD</kbd>+<kbd>g</kbd>                               |
| Penceredeki siyah kenarlıkları kaldırma          | <kbd>MOD</kbd>+<kbd>w</kbd> \| _Çift-sol-tık¹_            |
| `ANA EKRAN` tuşu                                 | <kbd>MOD</kbd>+<kbd>h</kbd> \| _Orta-tık_                 |
| `GERİ` tuşu                                      | <kbd>MOD</kbd>+<kbd>b</kbd> \| _Sağ-tık²_                 |
| `UYGULAMA_DEĞİŞTİR` tuşu                         | <kbd>MOD</kbd>+<kbd>s</kbd> \| _4.tık³_                   |
| `MENÜ` tuşu (ekran kilidini açma)                | <kbd>MOD</kbd>+<kbd>m</kbd>                               |
| `SES_AÇ` tuşu                                    | <kbd>MOD</kbd>+<kbd>↑</kbd> _(yukarı)_                    |
| `SES_KIS` tuşu                                   | <kbd>MOD</kbd>+<kbd>↓</kbd> _(aşağı)_                     |
| `GÜÇ` tuşu                                       | <kbd>MOD</kbd>+<kbd>p</kbd>                               |
| Gücü açma                                        | _Sağ-tık²_                                                |
| Cihaz ekranını kapatma (ekran yakalama durmadan) | <kbd>MOD</kbd>+<kbd>o</kbd>                               |
| Cihaz ekranını açma                              | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>o</kbd>              |
| Cihaz ekranını dönderme                          | <kbd>MOD</kbd>+<kbd>r</kbd>                               |
| Bildirim panelini genişletme                     | <kbd>MOD</kbd>+<kbd>n</kbd> \| _5.tık³_                   |
| Ayarlar panelini genişletme                      | <kbd>MOD</kbd>+<kbd>n</kbd>+<kbd>n</kbd> \| _Çift-5.tık³_ |
| Panelleri kapatma                                | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>n</kbd>              |
| Panoya kopyalama⁴                                | <kbd>MOD</kbd>+<kbd>c</kbd>                               |
| Panoya kesme⁴                                    | <kbd>MOD</kbd>+<kbd>x</kbd>                               |
| Panoları senkronize ederek yapıştırma⁴           | <kbd>MOD</kbd>+<kbd>v</kbd>                               |
| Bilgisayar panosundaki metini girme              | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd>              |
| FPS sayacını açma/kapatma (terminalde)           | <kbd>MOD</kbd>+<kbd>i</kbd>                               |
| İki parmakla yakınlaştırma                       | <kbd>Ctrl</kbd>+_tıkla-ve-sürükle_                        |

_¹Siyah kenarlıkları silmek için üzerine çift tıklayın._  
_²Sağ-tık ekran kapalıysa açar, değilse GERİ sinyali gönderir._  
_³4. ve 5. fare tuşları (eğer varsa)._  
_⁴Sadece Android 7 ve üzeri versiyonlarda._

Tekrarlı tuşu olan kısayollar tuş bırakılıp tekrar basılarak tekrar çalıştırılır.
Örneğin, "Ayarlar panelini genişletmek" için:

1.  <kbd>MOD</kbd> tuşuna basın ve basılı tutun.
2.  <kbd>n</kbd> tuşuna iki defa basın.
3.  <kbd>MOD</kbd> tuşuna basmayı bırakın.

Tüm <kbd>Ctrl</kbd>+_tuş_ kısayolları cihaza gönderilir. Bu sayede istenilen komut
uygulama tarafından çalıştırılır.

## Özel dizinler

Varsayılandan farklı bir _adb_ programı çalıştırmak için `ADB` ortam değişkenini
ayarlayın:

```bash
ADB=/path/to/adb scrcpy
```

`scrcpy-server` programının dizinini değiştirmek için `SCRCPY_SERVER_PATH`
değişkenini ayarlayın.

[useful]: https://github.com/Genymobile/scrcpy/issues/278#issuecomment-429330345

## Neden _scrcpy_?

Bir meslektaşım [gnirehtet] gibi söylenmesi zor bir isim bulmam için bana meydan okudu.

[`strcpy`] **str**ing kopyalıyor; `scrcpy` **scr**een kopyalıyor.

[gnirehtet]: https://github.com/Genymobile/gnirehtet
[`strcpy`]: http://man7.org/linux/man-pages/man3/strcpy.3.html

## Nasıl derlenir?

Bakınız [BUILD].

## Yaygın problemler

Bakınız [FAQ](FAQ.md).

## Geliştiriciler

[Geliştiriciler sayfası]nı okuyun.

[geliştiriciler sayfası]: DEVELOP.md

## Lisans

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

## Makaleler

- [Introducing scrcpy][article-intro]
- [Scrcpy now works wirelessly][article-tcpip]

[article-intro]: https://blog.rom1v.com/2018/03/introducing-scrcpy/
[article-tcpip]: https://www.genymotion.com/blog/open-source-project-scrcpy-now-works-wirelessly/
