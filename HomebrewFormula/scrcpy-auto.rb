class ScrcpyAuto < Formula
  desc "Automation-focused scrcpy fork: screen mirroring + scriptable control/daemon"
  homepage "https://github.com/fish-dapangyu-dev/scrcpy"
  # Stable install pins a source revision (a git checkout; no tarball needed).
  # Bump `revision` (and `version` when the fork version changes) per release.
  url "https://github.com/fish-dapangyu-dev/scrcpy.git",
      revision: "b631bb5e55a322449183e5ce9cff9d0a017bef75"
  version "4.0"
  license "Apache-2.0"

  # `brew install --HEAD ...` builds the latest fork work instead of the pin.
  head "https://github.com/fish-dapangyu-dev/scrcpy.git", branch: "feat/control"

  depends_on "meson" => :build
  depends_on "ninja" => :build
  depends_on "pkgconf" => :build

  depends_on "ffmpeg"  # provides libavcodec/format/util + libswscale (fork dep)
  depends_on "libusb"
  depends_on "sdl3"
  # `adb` is required at runtime; the android-platform-tools cask provides it.
  # If your Homebrew rejects a cask dependency here, delete this line and run
  # `brew install --cask android-platform-tools` yourself (see the README).
  depends_on cask: "android-platform-tools"

  # The device-side server (a .jar) needs the Android SDK to build, so ship the
  # matching UPSTREAM release artifact instead. Its wire protocol must match the
  # client version, so this is pinned alongside the source revision above; bump
  # both together. `scrcpy-server-v4.0` is byte-identical for the fork — the
  # fork only renames the file on install, it does not modify the server.
  resource "server" do
    url "https://github.com/Genymobile/scrcpy/releases/download/v4.0/scrcpy-server-v4.0"
    sha256 "84924bd564a1eb6089c872c7521f968058977f91f5ff02514a8c74aff3210f3a"
  end

  def install
    # Stage the prebuilt server jar, then let meson's `prebuilt_server` copy it
    # to share/scrcpy-auto/scrcpy-auto-server (no Android toolchain needed).
    resource("server").stage do
      buildpath.install Dir["*"].first => "scrcpy-server"
    end

    system "meson", "setup", "build",
           "-Dprebuilt_server=#{buildpath}/scrcpy-server",
           *std_meson_args
    system "meson", "compile", "-C", "build"
    system "meson", "install", "-C", "build"
  end

  def caveats
    <<~EOS
      scrcpy-auto drives an Android device through ADB.

      1. adb comes from the android-platform-tools cask (a dependency).
         Verify it is on your PATH:  adb version
      2. On the device: enable Developer options > USB debugging, connect it,
         and accept the "Allow USB debugging" prompt. Confirm with:  adb devices

      The device-side server was installed to:
        #{opt_pkgshare}/scrcpy-auto-server
      scrcpy-auto finds it automatically; override with SCRCPY_SERVER_PATH.

      This is a fork (binary: scrcpy-auto, server: scrcpy-auto-server) and does
      NOT conflict with an upstream `scrcpy` install.
    EOS
  end

  test do
    # --version prints the version and the linked dependency versions, exit 0.
    assert_match version.to_s, shell_output("#{bin}/scrcpy-auto --version")
    # the prebuilt server must have been installed alongside the client
    assert_path_exists pkgshare/"scrcpy-auto-server"
  end
end
