_Only the original [README](README.md) is guaranteed to be up-to-date._

# scrcpy (v1.16)

Aplikasi ini menyediakan tampilan dan kontrol perangkat Android yang terhubung pada USB (atau [melalui TCP/IP][article-tcpip]). Ini tidak membutuhkan akses _root_ apa pun. Ini bekerja pada _GNU/Linux_, _Windows_ and _macOS_.

![screenshot](assets/screenshot-debian-600.jpg)

Ini berfokus pada:

  - **keringanan** (asli, hanya menampilkan layar perangkat)
  - **kinerja** (30~60fps)
  - **kualitas** (1920×1080 atau lebih)
  - **latensi** rendah ([35~70ms][lowlatency])
  - **waktu startup rendah** (~1 detik untuk menampilkan gambar pertama)
  - **tidak mengganggu** (tidak ada yang terpasang di perangkat)


[lowlatency]: https://github.com/Genymobile/scrcpy/pull/646


## Persyaratan
Perangkat Android membutuhkan setidaknya API 21 (Android 5.0).

Pastikan Anda [mengaktifkan debugging adb][enable-adb] pada perangkat Anda.

[enable-adb]: https://developer.android.com/studio/command-line/adb.html#Enabling

Di beberapa perangkat, Anda juga perlu mengaktifkan [opsi tambahan][control] untuk mengontrolnya menggunakan keyboard dan mouse.

[control]: https://github.com/Genymobile/scrcpy/issues/70#issuecomment-373286323


## Dapatkan aplikasinya

### Linux

Di Debian (_testing_ dan _sid_ untuk saat ini) dan Ubuntu (20.04):

```
apt install scrcpy
```

Paket [Snap] tersedia: [`scrcpy`][snap-link].

[snap-link]: https://snapstats.org/snaps/scrcpy

[snap]: https://en.wikipedia.org/wiki/Snappy_(package_manager)

Untuk Fedora, paket [COPR] tersedia: [`scrcpy`][copr-link].

[COPR]: https://fedoraproject.org/wiki/Category:Copr
[copr-link]: https://copr.fedorainfracloud.org/coprs/zeno/scrcpy/

Untuk Arch Linux, paket [AUR] tersedia: [`scrcpy`][aur-link].

[AUR]: https://wiki.archlinux.org/index.php/Arch_User_Repository
[aur-link]: https://aur.archlinux.org/packages/scrcpy/

Untuk Gentoo, tersedia [Ebuild]: [`scrcpy/`][ebuild-link].

[Ebuild]: https://wiki.gentoo.org/wiki/Ebuild
[ebuild-link]: https://github.com/maggu2810/maggu2810-overlay/tree/master/app-mobilephone/scrcpy

Anda juga bisa [membangun aplikasi secara manual][BUILD] (jangan khawatir, tidak terlalu sulit).


### Windows

Untuk Windows, untuk kesederhanaan, arsip prebuilt dengan semua dependensi (termasuk `adb`) tersedia :

 - [README](README.md#windows)

Ini juga tersedia di [Chocolatey]:

[Chocolatey]: https://chocolatey.org/

```bash
choco install scrcpy
choco install adb    # jika Anda belum memilikinya
```

Dan di [Scoop]:

```bash
scoop install scrcpy
scoop install adb    # jika Anda belum memilikinya
```

[Scoop]: https://scoop.sh

Anda juga dapat [membangun aplikasi secara manual][BUILD].


### macOS

Aplikasi ini tersedia di [Homebrew]. Instal saja:

[Homebrew]: https://brew.sh/

```bash
brew install scrcpy
```
Anda membutuhkan `adb`, dapat diakses dari `PATH` Anda. Jika Anda belum memilikinya:

```bash
brew cask install android-platform-tools
```

Anda juga dapat [membangun aplikasi secara manual][BUILD].


## Menjalankan

Pasang perangkat Android, dan jalankan:

```bash
scrcpy
```

Ini menerima argumen baris perintah, didaftarkan oleh:

```bash
scrcpy --help
```

## Fitur

### Menangkap konfigurasi

#### Mengurangi ukuran

Kadang-kadang, berguna untuk mencerminkan perangkat Android dengan definisi yang lebih rendah untuk meningkatkan kinerja.

Untuk membatasi lebar dan tinggi ke beberapa nilai (mis. 1024):

```bash
scrcpy --max-size 1024
scrcpy -m 1024  # versi pendek
```

Dimensi lain dihitung agar rasio aspek perangkat dipertahankan.
Dengan begitu, perangkat 1920×1080 akan dicerminkan pada 1024×576.

#### Ubah kecepatan bit

Kecepatan bit default adalah 8 Mbps. Untuk mengubah bitrate video (mis. Menjadi 2 Mbps):

```bash
scrcpy --bit-rate 2M
scrcpy -b 2M  # versi pendek
```

#### Batasi frekuensi gambar

Kecepatan bingkai pengambilan dapat dibatasi:

```bash
scrcpy --max-fps 15
```

Ini secara resmi didukung sejak Android 10, tetapi dapat berfungsi pada versi sebelumnya.

#### Memotong

Layar perangkat dapat dipotong untuk mencerminkan hanya sebagian dari layar.

Ini berguna misalnya untuk mencerminkan hanya satu mata dari Oculus Go:

```bash
scrcpy --crop 1224:1440:0:0   # 1224x1440 Mengimbangi (0,0)
```

Jika `--max-size` juga ditentukan, pengubahan ukuran diterapkan setelah pemotongan.


#### Kunci orientasi video

Untuk mengunci orientasi pencerminan:

```bash
scrcpy --lock-video-orientation 0   # orientasi alami
scrcpy --lock-video-orientation 1   # 90° berlawanan arah jarum jam
scrcpy --lock-video-orientation 2   # 180°
scrcpy --lock-video-orientation 3   # 90° searah jarum jam
```

Ini mempengaruhi orientasi perekaman.


### Rekaman

Anda dapat merekam layar saat melakukan mirroring:

```bash
scrcpy --record file.mp4
scrcpy -r file.mkv
```

Untuk menonaktifkan pencerminan saat merekam:

```bash
scrcpy --no-display --record file.mp4
scrcpy -Nr file.mkv
# berhenti merekam dengan Ctrl+C
```

"Skipped frames" are recorded, even if they are not displayed in real time (for
performance reasons). Frames are _timestamped_ on the device, so [packet delay
variation] does not impact the recorded file.

"Frame yang dilewati" direkam, meskipun tidak ditampilkan secara real time (untuk alasan performa). Bingkai *diberi stempel waktu* pada perangkat, jadi [variasi penundaan paket] tidak memengaruhi file yang direkam.

[variasi penundaan paket]: https://en.wikipedia.org/wiki/Packet_delay_variation


### Koneksi

#### Wireless

_Scrcpy_ menggunakan `adb` untuk berkomunikasi dengan perangkat, dan` adb` dapat [terhubung] ke perangkat melalui TCP / IP:

1. Hubungkan perangkat ke Wi-Fi yang sama dengan komputer Anda.
2. Dapatkan alamat IP perangkat Anda (dalam Pengaturan → Tentang ponsel → Status).
3. Aktifkan adb melalui TCP / IP pada perangkat Anda: `adb tcpip 5555`.
4. Cabut perangkat Anda.
5. Hubungkan ke perangkat Anda: `adb connect DEVICE_IP: 5555` (*ganti* *`DEVICE_IP`*).
6. Jalankan `scrcpy` seperti biasa.

Mungkin berguna untuk menurunkan kecepatan bit dan definisi:

```bash
scrcpy --bit-rate 2M --max-size 800
scrcpy -b2M -m800  # versi pendek
```

[terhubung]: https://developer.android.com/studio/command-line/adb.html#wireless


#### Multi-perangkat

Jika beberapa perangkat dicantumkan di `adb devices`, Anda harus menentukan _serial_:

```bash
scrcpy --serial 0123456789abcdef
scrcpy -s 0123456789abcdef  # versi pendek
```

If the device is connected over TCP/IP:

```bash
scrcpy --serial 192.168.0.1:5555
scrcpy -s 192.168.0.1:5555  # versi pendek
```

Anda dapat memulai beberapa contoh _scrcpy_ untuk beberapa perangkat.

#### Mulai otomatis pada koneksi perangkat

Anda bisa menggunakan [AutoAdb]:

```bash
autoadb scrcpy -s '{}'
```

[AutoAdb]: https://github.com/rom1v/autoadb

#### Koneksi via SSH tunnel

Untuk menyambung ke perangkat jarak jauh, dimungkinkan untuk menghubungkan klien `adb` lokal ke server `adb` jarak jauh (asalkan mereka menggunakan versi yang sama dari _adb_ protocol):

```bash
adb kill-server    # matikan server adb lokal di 5037
ssh -CN -L5037:localhost:5037 -R27183:localhost:27183 komputer_jarak_jauh_anda
# jaga agar tetap terbuka
```

Dari terminal lain:

```bash
scrcpy
```

Untuk menghindari mengaktifkan penerusan port jarak jauh, Anda dapat memaksa sambungan maju sebagai gantinya (perhatikan `-L`, bukan` -R`):

```bash
adb kill-server    # matikan server adb lokal di 5037
ssh -CN -L5037:localhost:5037 -L27183:localhost:27183 komputer_jarak_jauh_anda
# jaga agar tetap terbuka
```

Dari terminal lain:

```bash
scrcpy --force-adb-forward
```

Seperti koneksi nirkabel, mungkin berguna untuk mengurangi kualitas:

```
scrcpy -b2M -m800 --max-fps 15
```

### Konfigurasi Jendela

#### Judul

Secara default, judul jendela adalah model perangkat. Itu bisa diubah:

```bash
scrcpy --window-title 'Perangkat Saya'
```

#### Posisi dan ukuran

Posisi dan ukuran jendela awal dapat ditentukan:

```bash
scrcpy --window-x 100 --window-y 100 --window-width 800 --window-height 600
```

#### Jendela tanpa batas

Untuk menonaktifkan dekorasi jendela:

```bash
scrcpy --window-borderless
```

#### Selalu di atas

Untuk menjaga jendela scrcpy selalu di atas:

```bash
scrcpy --always-on-top
```

#### Layar penuh

Aplikasi dapat dimulai langsung dalam layar penuh::

```bash
scrcpy --fullscreen
scrcpy -f  # versi pendek
```

Layar penuh kemudian dapat diubah secara dinamis dengan <kbd>MOD</kbd>+<kbd>f</kbd>.

#### Rotasi

Jendela mungkin diputar:

```bash
scrcpy --rotation 1
```

Nilai yang mungkin adalah:
 - `0`: tidak ada rotasi
 - `1`: 90 derajat berlawanan arah jarum jam
 - `2`: 180 derajat
 - `3`: 90 derajat searah jarum jam

Rotasi juga dapat diubah secara dinamis dengan <kbd>MOD</kbd>+<kbd>←</kbd>
_(kiri)_ and <kbd>MOD</kbd>+<kbd>→</kbd> _(kanan)_.

Perhatikan bahwa _scrcpy_ mengelola 3 rotasi berbeda::
 - <kbd>MOD</kbd>+<kbd>r</kbd> meminta perangkat untuk beralih antara potret dan lanskap (aplikasi yang berjalan saat ini mungkin menolak, jika mendukung orientasi yang diminta).
 - `--lock-video-orientation` mengubah orientasi pencerminan (orientasi video yang dikirim dari perangkat ke komputer). Ini mempengaruhi rekaman.
 - `--rotation` (atau <kbd>MOD</kbd>+<kbd>←</kbd>/<kbd>MOD</kbd>+<kbd>→</kbd>)
   memutar hanya konten jendela. Ini hanya mempengaruhi tampilan, bukan rekaman.


### Opsi pencerminan lainnya

#### Hanya-baca

Untuk menonaktifkan kontrol (semua yang dapat berinteraksi dengan perangkat: tombol input, peristiwa mouse, seret & lepas file):

```bash
scrcpy --no-control
scrcpy -n
```

#### Layar

Jika beberapa tampilan tersedia, Anda dapat memilih tampilan untuk cermin:

```bash
scrcpy --display 1
```

Daftar id tampilan dapat diambil dengan::

```
adb shell dumpsys display   # cari "mDisplayId=" di keluaran
```

Tampilan sekunder hanya dapat dikontrol jika perangkat menjalankan setidaknya Android 10 (jika tidak maka akan dicerminkan dalam hanya-baca).


#### Tetap terjaga

Untuk mencegah perangkat tidur setelah beberapa penundaan saat perangkat dicolokkan:

```bash
scrcpy --stay-awake
scrcpy -w
```

Keadaan awal dipulihkan ketika scrcpy ditutup.


#### Matikan layar

Dimungkinkan untuk mematikan layar perangkat saat pencerminan mulai dengan opsi baris perintah:

```bash
scrcpy --turn-screen-off
scrcpy -S
```

Atau dengan menekan <kbd>MOD</kbd>+<kbd>o</kbd> kapan saja.

Untuk menyalakannya kembali, tekan <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>o</kbd>.

Di Android, tombol `POWER` selalu menyalakan layar. Untuk kenyamanan, jika `POWER` dikirim melalui scrcpy (melalui klik kanan atau<kbd>MOD</kbd>+<kbd>p</kbd>), itu akan memaksa untuk mematikan layar setelah penundaan kecil (atas dasar upaya terbaik).
Tombol fisik `POWER` masih akan menyebabkan layar dihidupkan.

Ini juga berguna untuk mencegah perangkat tidur:

```bash
scrcpy --turn-screen-off --stay-awake
scrcpy -Sw
```

#### Render frame kedaluwarsa

Secara default, untuk meminimalkan latensi, _scrcpy_ selalu menampilkan frame yang terakhir didekodekan tersedia, dan menghapus frame sebelumnya.

Untuk memaksa rendering semua frame (dengan kemungkinan peningkatan latensi), gunakan:

```bash
scrcpy --render-expired-frames
```

#### Tunjukkan sentuhan

Untuk presentasi, mungkin berguna untuk menunjukkan sentuhan fisik (pada perangkat fisik).

Android menyediakan fitur ini di _Opsi Pengembang_.

_Scrcpy_ menyediakan opsi untuk mengaktifkan fitur ini saat mulai dan mengembalikan nilai awal saat keluar:

```bash
scrcpy --show-touches
scrcpy -t
```

Perhatikan bahwa ini hanya menunjukkan sentuhan _fisik_ (dengan jari di perangkat).


#### Nonaktifkan screensaver

Secara default, scrcpy tidak mencegah screensaver berjalan di komputer.

Untuk menonaktifkannya:

```bash
scrcpy --disable-screensaver
```


### Kontrol masukan

#### Putar layar perangkat

Tekan <kbd>MOD</kbd>+<kbd>r</kbd> untuk beralih antara mode potret dan lanskap.

Perhatikan bahwa itu berputar hanya jika aplikasi di latar depan mendukung orientasi yang diminta.

#### Salin-tempel

Setiap kali papan klip Android berubah, secara otomatis disinkronkan ke papan klip komputer.

Apa saja <kbd>Ctrl</kbd> pintasan diteruskan ke perangkat. Khususnya:
 - <kbd>Ctrl</kbd>+<kbd>c</kbd> biasanya salinan
 - <kbd>Ctrl</kbd>+<kbd>x</kbd> biasanya memotong
 - <kbd>Ctrl</kbd>+<kbd>v</kbd> biasanya menempel (setelah sinkronisasi papan klip komputer-ke-perangkat)

Ini biasanya berfungsi seperti yang Anda harapkan.

Perilaku sebenarnya tergantung pada aplikasi yang aktif. Sebagai contoh,
_Termux_ mengirim SIGINT ke <kbd>Ctrl</kbd>+<kbd>c</kbd> sebagai gantinya, dan _K-9 Mail_ membuat pesan baru.

Untuk menyalin, memotong dan menempel dalam kasus seperti itu (tetapi hanya didukung di Android> = 7):
 - <kbd>MOD</kbd>+<kbd>c</kbd> injeksi `COPY` _(salin)_
 - <kbd>MOD</kbd>+<kbd>x</kbd> injeksi `CUT` _(potong)_
 - <kbd>MOD</kbd>+<kbd>v</kbd> injeksi `PASTE` (setelah sinkronisasi papan klip komputer-ke-perangkat)

Tambahan, <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd> memungkinkan untuk memasukkan teks papan klip komputer sebagai urutan peristiwa penting. Ini berguna ketika komponen tidak menerima penempelan teks (misalnya di _Termux_), tetapi dapat merusak konten non-ASCII.

**PERINGATAN:** Menempelkan papan klip komputer ke perangkat (baik melalui
<kbd>Ctrl</kbd>+<kbd>v</kbd> or <kbd>MOD</kbd>+<kbd>v</kbd>) menyalin konten ke clipboard perangkat. Akibatnya, aplikasi Android apa pun dapat membaca kontennya. Anda harus menghindari menempelkan konten sensitif (seperti kata sandi) seperti itu.


#### Cubit untuk memperbesar/memperkecil

Untuk mensimulasikan "cubit-untuk-memperbesar/memperkecil": <kbd>Ctrl</kbd>+_klik-dan-pindah_.

Lebih tepatnya, tahan <kbd>Ctrl</kbd> sambil menekan tombol klik kiri. Hingga tombol klik kiri dilepaskan, semua gerakan mouse berskala dan memutar konten (jika didukung oleh aplikasi) relatif ke tengah layar.

Secara konkret, scrcpy menghasilkan kejadian sentuh tambahan dari "jari virtual" di lokasi yang dibalik melalui bagian tengah layar.


#### Preferensi injeksi teks

Ada dua jenis [peristiwa][textevents] dihasilkan saat mengetik teks:
- _peristiwa penting_, menandakan bahwa tombol ditekan atau dilepaskan;
- _peristiwa teks_, menandakan bahwa teks telah dimasukkan.

Secara default, huruf dimasukkan menggunakan peristiwa kunci, sehingga keyboard berperilaku seperti yang diharapkan dalam game (biasanya untuk tombol WASD).

Tapi ini mungkin [menyebabkan masalah][prefertext]. Jika Anda mengalami masalah seperti itu, Anda dapat menghindarinya dengan:

```bash
scrcpy --prefer-text
```

(tapi ini akan merusak perilaku keyboard dalam game)

[textevents]: https://blog.rom1v.com/2018/03/introducing-scrcpy/#handle-text-input
[prefertext]: https://github.com/Genymobile/scrcpy/issues/650#issuecomment-512945343


#### Ulangi kunci

Secara default, menahan tombol akan menghasilkan peristiwa kunci yang berulang. Ini dapat menyebabkan masalah kinerja di beberapa game, di mana acara ini tidak berguna.

Untuk menghindari penerusan peristiwa penting yang berulang:

```bash
scrcpy --no-key-repeat
```


### Seret/jatuhkan file

#### Pasang APK

Untuk menginstal APK, seret & lepas file APK (diakhiri dengan `.apk`) ke jendela _scrcpy_.

Tidak ada umpan balik visual, log dicetak ke konsol.


#### Dorong file ke perangkat

Untuk mendorong file ke `/sdcard/` di perangkat, seret & jatuhkan file (non-APK) ke jendela _scrcpy_.

Tidak ada umpan balik visual, log dicetak ke konsol.

Direktori target dapat diubah saat mulai:

```bash
scrcpy --push-target /sdcard/foo/bar/
```


### Penerusan audio

Audio tidak diteruskan oleh _scrcpy_. Gunakan [sndcpy].

Lihat juga [Masalah #14].

[sndcpy]: https://github.com/rom1v/sndcpy
[Masalah #14]: https://github.com/Genymobile/scrcpy/issues/14


## Pintasan

Dalam daftar berikut, <kbd>MOD</kbd> adalah pengubah pintasan. Secara default, ini (kiri) <kbd>Alt</kbd> atau (kiri) <kbd>Super</kbd>.

Ini dapat diubah menggunakan `--shortcut-mod`. Kunci yang memungkinkan adalah `lctrl`,`rctrl`, `lalt`,` ralt`, `lsuper` dan` rsuper`. Sebagai contoh:

```bash
# gunakan RCtrl untuk jalan pintas
scrcpy --shortcut-mod=rctrl

# gunakan baik LCtrl+LAlt atau LSuper untuk jalan pintas
scrcpy --shortcut-mod=lctrl+lalt,lsuper
```

_<kbd>[Super]</kbd> biasanya adalah <kbd>Windows</kbd> atau <kbd>Cmd</kbd> key._

[Super]: https://en.wikipedia.org/wiki/Super_key_(keyboard_button)

 | Aksi                                                  |   Pintasan
 | ------------------------------------------------------|:-----------------------------
 | Alihkan mode layar penuh                              | <kbd>MOD</kbd>+<kbd>f</kbd>
 | Putar layar kiri                                      | <kbd>MOD</kbd>+<kbd>←</kbd> _(kiri)_
 | Putar layar kanan                                     | <kbd>MOD</kbd>+<kbd>→</kbd> _(kanan)_
 | Ubah ukuran jendela menjadi 1:1 (piksel-sempurna)     | <kbd>MOD</kbd>+<kbd>g</kbd>
 | Ubah ukuran jendela menjadi hapus batas hitam         | <kbd>MOD</kbd>+<kbd>w</kbd> \| _klik-dua-kali¹_
 | Klik `HOME`                                           | <kbd>MOD</kbd>+<kbd>h</kbd> \| _Klik-tengah_
 | Klik `BACK`                                           | <kbd>MOD</kbd>+<kbd>b</kbd> \| _Klik-kanan²_
 | Klik `APP_SWITCH`                                     | <kbd>MOD</kbd>+<kbd>s</kbd>
 | Klik `MENU` (buka kunci layar)                        | <kbd>MOD</kbd>+<kbd>m</kbd>
 | Klik `VOLUME_UP`                                      | <kbd>MOD</kbd>+<kbd>↑</kbd> _(naik)_
 | Klik `VOLUME_DOWN`                                    | <kbd>MOD</kbd>+<kbd>↓</kbd> _(turun)_
 | Klik `POWER`                                          | <kbd>MOD</kbd>+<kbd>p</kbd>
 | Menyalakan                                            | _Klik-kanan²_
 | Matikan layar perangkat (tetap mirroring)             | <kbd>MOD</kbd>+<kbd>o</kbd>
 | Hidupkan layar perangkat                              | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>o</kbd>
 | Putar layar perangkat                                 | <kbd>MOD</kbd>+<kbd>r</kbd>
 | Luaskan panel notifikasi                              | <kbd>MOD</kbd>+<kbd>n</kbd>
 | Ciutkan panel notifikasi                              | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>n</kbd>
 | Menyalin ke papan klip³                               | <kbd>MOD</kbd>+<kbd>c</kbd>
 | Potong ke papan klip³                                 | <kbd>MOD</kbd>+<kbd>x</kbd>
 | Sinkronkan papan klip dan tempel³                     | <kbd>MOD</kbd>+<kbd>v</kbd>
 | Masukkan teks papan klip komputer                     | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd>
 | Mengaktifkan/menonaktifkan penghitung FPS (di stdout) | <kbd>MOD</kbd>+<kbd>i</kbd>
 | Cubit-untuk-memperbesar/memperkecil                   | <kbd>Ctrl</kbd>+_klik-dan-pindah_

_¹Klik-dua-kali pada batas hitam untuk menghapusnya._  
_²Klik-kanan akan menghidupkan layar jika mati, tekan BACK jika tidak._  
_³Hanya di Android >= 7._

Semua <kbd>Ctrl</kbd>+_key_ pintasan diteruskan ke perangkat, demikian adanya
ditangani oleh aplikasi aktif.


## Jalur kustom

Untuk menggunakan biner _adb_ tertentu, konfigurasikan jalurnya di variabel lingkungan `ADB`:

    ADB=/path/to/adb scrcpy

Untuk mengganti jalur file `scrcpy-server`, konfigurasikan jalurnya di
`SCRCPY_SERVER_PATH`.

[useful]: https://github.com/Genymobile/scrcpy/issues/278#issuecomment-429330345


## Mengapa _scrcpy_?

Seorang kolega menantang saya untuk menemukan nama yang tidak dapat diucapkan seperti [gnirehtet].

[`strcpy`] menyalin sebuah **str**ing; `scrcpy` menyalin sebuah **scr**een.

[gnirehtet]: https://github.com/Genymobile/gnirehtet
[`strcpy`]: http://man7.org/linux/man-pages/man3/strcpy.3.html


## Bagaimana Cara membangun?

Lihat [BUILD].

[BUILD]: BUILD.md


## Masalah umum

Lihat [FAQ](FAQ.md).


## Pengembang

Baca [halaman pengembang].

[halaman pengembang]: DEVELOP.md


## Lisensi

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

## Artikel

- [Introducing scrcpy][article-intro]
- [Scrcpy now works wirelessly][article-tcpip]

[article-intro]: https://blog.rom1v.com/2018/03/introducing-scrcpy/
[article-tcpip]: https://www.genymotion.com/blog/open-source-project-scrcpy-now-works-wirelessly/

