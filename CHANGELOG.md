# Changelog

Changes in this fork (jacobjeger/scrcpy) on top of upstream
[Genymobile/scrcpy](https://github.com/Genymobile/scrcpy). Upstream's own
changes are in its release notes. The fork has no releases of its own; the
`vX.Y.Z` tags in this repo are upstream's.

The format is based on [Keep a Changelog](https://keepachangelog.com/).

## [Unreleased]

### Changed
- Rebased onto upstream scrcpy 5.0.1, which uses SDL3 (building now needs
  `sdl3` instead of `sdl2`).
- The screenshot button and Ctrl+Shift+S work with hardware video decoding,
  which upstream 5.0 enables by default.

### Fixed
- The FIT toolbar button now shows its label (the letters were missing from
  the toolbar font).
- The log buttons, Ctrl+Shift+L and the D-pad focus log now target the
  mirrored device when several devices are connected.
- Releasing a mouse button over the toolbar after a drag on the device screen
  no longer leaves the touch stuck down on the device.

### Added (fork features before the 5.0.1 port)
- Toolbar above the mirror: FIT, HOME, SYS (dump all logs), APP (dump
  foreground-app logs), LOG (live logcat in Terminal, also Ctrl+Shift+L) and
  CAM (screenshot to ~/Downloads and the clipboard, also Ctrl+Shift+S, with a
  white flash).
- The window opens at the device's resolution instead of being scaled down.
- All connected devices are mirrored, one window each, when none is selected.
- D-pad key events and the resulting focus changes are logged.
