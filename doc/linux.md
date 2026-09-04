# On Linux

## Install

### From the official release

Download a static build of the [latest release]:

 - [`slink-linux-x86_64-v4.1.tar.gz`][direct-linux-x86_64] (x86_64)  
   <sub>SHA-256: `ad56ae8bfeedf41e824945c11dbf55fcb092b3e615b9b486f48a50e30d389635`</sub>

[latest release]: https://github.com/Genymobile/slink/releases/latest
[direct-linux-x86_64]: https://github.com/Genymobile/slink/releases/download/v4.1/slink-linux-x86_64-v4.1.tar.gz

and extract it.


### From your package manager

<a href="https://repology.org/project/slink/versions"><img src="https://repology.org/badge/vertical-allrepos/slink.svg" alt="Packaging status" align="right"></a>

slink is packaged in several distributions and package managers:

 - Debian/Ubuntu: ~~`apt install slink`~~ _(obsolete version)_
 - Arch Linux: `pacman -S slink`
 - Fedora: `dnf copr enable zeno/slink && dnf install slink`
 - Gentoo: `emerge slink`
 - Snap: ~~`snap install slink`~~ _(obsolete version)_
 - … (see [repology](https://repology.org/project/slink/versions))


### From an install script

To install the latest release from `master`, follow this simplified process.

First, you need to install the required packages:

```bash
# for Debian/Ubuntu
sudo apt install ffmpeg libsdl3-0 libusb-1.0-0 adb wget \
                 gcc git pkg-config meson ninja-build libsdl3-dev \
                 libavcodec-dev libavdevice-dev libavformat-dev libavutil-dev \
                 libswresample-dev libusb-1.0-0-dev libv4l-dev
```

Then clone the repo and execute the installation script
([source](/install_release.sh)):

```bash
git clone https://github.com/slickyincorp/slink
cd slink
./install_release.sh
```

When a new release is out, update the repo and reinstall:

```bash
git pull
./install_release.sh
```

To uninstall:

```bash
sudo ninja -Cbuild-auto uninstall
```

_Note that this simplified process only works for released versions (it
downloads a prebuilt server binary), so for example you can't use it for testing
the development branch (`dev`)._

_See [build.md](build.md) to build and install the app manually._


## Run

_Make sure that your device meets the [prerequisites](/README.md#prerequisites)._

Once installed, run from a terminal:

```bash
slink
```

or with arguments (here to disable audio and record to `file.mkv`):

```bash
slink --no-audio --record=file.mkv
```

Documentation for command line arguments is available:
 - `man slink`
 - `slink --help`
 - on [github](/README.md)
