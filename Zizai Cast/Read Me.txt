Zizai Cast — Read me (short version)
====================================

Mirror the screen of an iPhone / iPad / Android phone to this Windows PC.
Full illustrated guide: "ZizaiCast-Guide.html" in the install folder ("How to connect?" on the waiting screen opens it too).
The main window has two big tabs: "Phone mirroring" (sections 1-5 and 7) and "Remote control" (section 6: use this PC from a phone).

[Phone mirroring]
1. Before you start
  - The phone and the PC must be on the same Wi-Fi / local network (not a guest network, not mobile data).
  - On first run, when the Windows firewall asks, tick "Private networks" and click "Allow access".
  - Turn off VPNs on the PC and the phone first.
  - School, office and hotel networks may block device discovery, so the phone can't find the PC.

2. iPhone / iPad
  1. On the iPhone open "Control Center" → "Screen Mirroring" → choose "Zizai Cast".
  2. To stop: Control Center → "Screen Mirroring" → "Stop Mirroring" (or press Ctrl+D on the PC, see section 5).
  - Ask new phones for a PIN (in Settings): when on, a new iPhone must enter the 4-digit PIN shown on the PC the first time (phones that succeeded are remembered).
  - When a second phone connects (in Settings): "Switch to the new one" = the new phone is shown; "Keep the current one" = other phones are refused.
  - Mirroring pauses while the iPhone screen is off and resumes after you unlock it.
  - The iPhone's volume buttons control the volume played on the PC.

3. Android option 1: Cast (Miracast, watch only, no control)
  Phone's Quick Settings casting tile → choose "Zizai Cast". The name depends on the brand:
    Samsung: Smart View      Xiaomi/Redmi: Cast, Wireless display
    OPPO/realme: Screencast, Multi-screen      vivo: Screen mirroring
    ASUS: Wireless display, PlayTo      Sony: Screen mirroring
    Google Pixel: not supported, use option 2.
  The PC's Wi-Fi adapter must support Wireless Display, and Wi-Fi must be on. To check:
    in Terminal type  netsh wlan show drivers
    it works only if you see "Wireless Display Supported: Yes".

4. Android option 2: Wireless debugging (control the phone with mouse and keyboard, needs Android 11 or later)
  1. Settings → About phone → tap "Build number" 7 times to turn on "Developer options".
     (Samsung: About phone → Software information → Build number; Xiaomi: All specs → MIUI version)
  2. Developer options → turn on "Wireless debugging" → tap it → "Pair device with QR code".
  3. PC: right-click the window → "Connect a phone" → "Connect Android (scan QR)" (or click "Show QR" on the "Android: control from the PC" card on the waiting screen), and scan the QR code on screen with the phone.
  4. After that it connects automatically whenever wireless debugging is on. Android turns it off when the phone restarts; Zizai Cast tells you once: turn it on again, no new pairing.
     A pairing that waits too long stops (the QR code dims, "Timed out"): click "New QR code" and scan again.
  Control: left-click = tap, drag = swipe, wheel = scroll, right-click = Back, middle-click = Home, keyboard = typing.
  Right-click is "Back" by default, so to open the menu on an Android screen use "More" in the toolbar at the top, or Shift + right-click (always opens the menu).
  Prefer right-click to open the menu? Settings → "Right-click action": change "Back (default)" to "Open menu"; then use the toolbar's Back button to go back.
  Esc is sent to the phone (usually Back); in full screen, Esc leaves full screen.
  No response to the QR code after about 45 seconds: make sure the phone and the PC are on the same Wi-Fi, or click "Use pairing code".
  If the phone turns off wireless debugging or leaves the Wi-Fi, the PC returns to the waiting screen within seconds ("Android disconnected").
  Security: turn "Wireless debugging" off when you're not using it, and never turn it on on public Wi-Fi.

5. Features and shortcuts (right-click the picture for every feature)
  Disconnect (any phone): Ctrl+D, or move the mouse to the top of the picture → the red button at the right end of the toolbar,
    or right-click the tray icon → "Disconnect (phone name)". An Android phone disconnected from the PC won't reconnect
    automatically until you pair it again with the QR code or restart Zizai Cast.
  Toolbar: appears when the mouse moves to the top of the picture (or enters the window) while mirroring, and stays while the pointer is on it; a thin pink line at the top shows where it is while hidden:
    (Android: Back, Home, Recent apps) Screenshot, Record (red while recording), Translate, Zoom (these four show text labels),
    Rotate, Full screen, More (full menu), Disconnect. A hint appears the first time you connect.
  Keyboard shortcuts: F1 (or Help → Keyboard shortcuts) lists them all.
  With no phone connected, menu items such as Screenshot, Start recording and Magnifier appear greyed out and say "Once a phone is connected".
  F11 / double-click the picture: Full screen (Esc to leave)      Ctrl+T: Always on top
  Ctrl+S: Screenshot (截圖 / Screenshots folder)      Ctrl+R: Start/stop recording (錄影 / Recordings folder, MP4 with sound)
  Ctrl+→ / Ctrl+←: rotate 90°   Ctrl+H: flip horizontally   Ctrl+0: reset   Ctrl+F: iPhone frame
  Picture quality (in Settings): Standard (smoothest, 1080p) / High (recommended, 1440p) / Highest (needs strong Wi-Fi, 4K);
    High and Highest need "HEVC Video Extensions" from the Microsoft Store.
    After changing Picture quality or the PIN setting, it normally applies when mirroring ends; "Reconnect now to apply" next to the hint applies it immediately.
  Themes: Sakura, Mint, Night Sky, Milk Tea.
  Language: Settings → "Language / 語言" (Automatic (Windows) / 繁體中文 / English / 日本語 / 한국어).
  Closing the window asks "Quit, or keep running in the background?"; "Keep running in the background" sends the app to the system tray at the bottom right, where it keeps waiting for phones
    (Windows 11 may hide the icon under "^"; drag it to the taskbar to pin it). To quit, right-click the tray icon → "Quit".
    Change it later in Settings → "When closing the window": Ask every time / Keep running in the background / Quit.
  Start with Windows (waits for phones in the background after Windows starts), Check for updates: in Settings / Help or the tray menu (updates are also checked at every start).
  Updates: when a new version is out, the "A new version of Zizai Cast" window offers "Update now", "Remind me later"
  or "Skip this version"; "Update to vX.Y.Z" also appears at the top of the menu, on the waiting screen and in the toolbar.
  It installs only when you click it (never by itself) and restarts afterwards. A newer installer placed in the 安裝檔 (Installers) folder is found too.

  New in 0.7:
  - Magnifier: right-click menu → "Magnifier" ("Zoom" in the toolbar). Ctrl+= zoom in, Ctrl+- zoom out (up to 8×), Ctrl+Shift+0 back to 1×;
    Ctrl+wheel zooms where the pointer is; drag to move around (Ctrl+drag while controlling an Android phone).
    Freeze picture (Ctrl+P): holds the picture still on the PC while the phone carries on.
  - Translate: right-click menu → "Translate" ("Translate" in the toolbar). Translate the whole screen (Ctrl+L, again to close), Translate a selected area
    (Ctrl+Shift+L), Show original (Ctrl+O, available after translating), Live translation (re-translates when the screen changes), Translate into: 繁體中文 / English / 日本語 / 한국어,
    Show translations: Automatic / Over the text / List, Advanced translation settings (Local AI translation…, Manage translation models; not needed for everyday use).
    Runs offline on this PC, nothing is sent to the internet; the first time it asks, then downloads a model (about 50 MB per direction).
    Japanese/Korean not recognised: Windows Settings → Time & language → Language & region → Add a language, tick only "Optical character recognition".
  - Send to phone: after a screenshot or recording click "Send to phone" (also in the right-click menu under "Send to phone"). Android (wireless debugging): straight into the gallery;
    iPhone / iPad / Android casting: scan the QR code on the PC with the phone's camera (same Wi-Fi, works for 10 minutes).

[Remote control]
6. Remote control: use this PC from a phone (the second big feature)
  See the PC's screen in the phone's browser, hear its sound and use the mouse and keyboard with your fingers; no app (Safari on iPhone, Chrome on Android).
  Off every time Zizai Cast starts: turn it on yourself. Full illustrated steps: the "Remote control" chapter of the guide.

  Turn it on: the main window has two big tabs, "Phone mirroring" and "Remote control". Switch to "Remote control" and turn it on
    (the "Remote control" item in the right-click menu and the tray menu is the same switch). While connected, a
    "Remote control in progress" bar stays at the top of the PC screen.

  At home (same Wi-Fi):
    1. Turn it on in the "Remote control" tab and open the "Same Wi-Fi" page: a QR code appears.
    2. Phone on the same Wi-Fi: scan the QR code with the camera.
    3. The PC asks "A phone wants to control this PC" -> click "Allow" only for your own phone (tick "Remember this device" to skip it next time).
    4. Tap "Tap for sound" on the phone (or the speaker labelled "Sound on" at the bottom right) to hear the PC.

  Away (for you): fixed address + a password you choose (the address never changes; nothing on the phone; Tailscale on the PC)
    1. Install Tailscale on the PC from https://tailscale.com/download/windows, right-click its icon -> Log in,
       sign in with a Google / Microsoft / GitHub / Apple account (the first time pick Personal use).
    2. A new account must allow Funnel and HTTPS first (once): right-click Start -> Terminal and run
         tailscale funnel --bg --https=8443 http://127.0.0.2:8091
       The first time it prints a link: open it, sign in, approve (Enable).
       Easier: just do step 3; Zizai Cast says why it can't start and offers "Open the setup page", then "Done, try again".
    3. "Allow remote control" at the top of the "Remote control" tab -> "Set a password (fixed address + temporary accounts)"
       (at least 8 characters) -> "Save and turn on".
    4. The first time, wait a minute or two: the block says "Preparing the address, about 1-2 minutes" (Tailscale is getting the HTTPS
       certificate; the phone can't open it yet). Zizai Cast checks it by itself and says "The fixed address is on" once it works.
    5. Open the fixed address on the phone and enter just your password. To be reachable as soon as the PC starts,
       tick "Turn the fixed address on when the PC starts" in "More settings"
       (changing the password and turning the fixed address off are there too).
    6. Add the page to the Home Screen (the icon is called "Zizai Remote"):
       iPhone: Safari "Share" -> "Add to Home Screen"; Android: Chrome "⋮" -> "Add to Home screen".
    5 wrong passwords in a row lock sign-in for 15 minutes. With HTTPS on, the PC's name appears in public certificate logs: rename it first if it is your real name.
  Away (Tailscale on the phone too): same account on both, scan the QR code on the "Away: Tailscale" card; no password, no Funnel.
  Lend it to a friend (both ways alike: the first time your friend connects after signing in, someone at the PC clicks "Allow"; by default it can't
  be remembered. Then the same phone with the same account or address is not asked again for a quality / screen switch, a reconnect
  or a reload; after "Deny", "End", or once the account expires or is deleted, it is asked again):
    - Temporary account (recommended; the fixed address must be on): "Allow remote control" -> "Add a temporary account", pick
      "Valid for" (One use / 1 hour / 24 hours / 7 days), "Allowed to" (Control / View only); "Can transfer files" is ticked already (untick it if not wanted) -> "Create"
      -> "Copy all" and paste it to your friend. The address carries the account: your friend only types the password.
      The password is shown only once ("Reset password" makes a new one). It is deleted when it expires or is used up.
    - Temporary address: without a fixed address "Create a temporary address"; with one, "More settings" -> "Temporary address (no app
      on the phone ...)". After 10-20 s a QR code and a 6-digit passcode appear. "Copy address" and send it (works once, within
      10 minutes), tell the passcode separately. "Stop the temporary address" when done.
  Ask before someone connects ("More settings"): your account, guest accounts and the temporary address each get "Ask every time",
    "Remember this device" or "Password only" (defaults: your account unchanged, guest accounts and the temporary address ask every time).
    Memories are kept per place (home / fixed address / each guest account / temporary address); Deny, End, a deleted or expired
    guest account, a password reset or a new main password forget them. "Remembered phones" removes one or all.
    Home Wi-Fi and Tailscale always need Allow (you can tick Remember).

  Control on the phone (Settings -> Control):
    - Direct (default): your finger clicks where it touches; hold half a second = right click; hold and move = drag.
    - Trackpad: slide to move the pointer, tap = left click, two-finger tap = right click, two fingers = scroll.
    Pinch = zoom, three-finger tap = keyboard. In Trackpad mode a big arrow (pink ring) shows where the PC pointer is.
    The bottom-right buttons: Sound on, Zoom, Files (grey when files are off: a tap says why), Keyboard, Stats, Settings.
    Picture rows in Settings: Picture (Smooth / Standard / Sharp), Frame rate (30 / 60 / 120, 60 by default, at most the PC screen's
    refresh rate), Match the phone's resolution (on by default on phones and tablets, off in a computer's browser). They change
    inside the same connection: no black flash.

  From another computer's browser (same address): every mouse button, the wheel and dragging work, a click lands where you point;
    you type with that computer's input method, shortcuts go to the remote PC. Mouse to the top = the bar: Full screen (hold Esc to
    leave), Actual size, the screens, Game mode (Esc or Ctrl+Alt+Shift+Q leaves), Alt+Tab, Win, Ctrl+Alt+Del, Ctrl+W, Task Manager,
    Clipboard (Ctrl+V pastes this computer's text there), sound, Files, Stats, Settings.

  Sound:
    - The phone page starts silent: tap "Tap for sound", or the speaker labelled "Sound on" at the bottom right.
    - The PC sets a good level by itself; "Phone volume" at its default 100% is usually enough. Too quiet: slide it right, up to 400%, no clipping.
    - On the PC, "More settings" → "Sound" → "Send the PC's sound to the phone" must be ticked; "Mute the PC while the phone plays sound"
      (the phone page's "Mute the PC's speakers") keeps the PC's speakers silent. Phone volume above 100% is loudness boost (up to 400%, no clipping).
    - Don't use Windows mute (taskbar speaker, keyboard mute key): the phone may go silent too.

  Safety:
    - The fixed address is public. Your password is the only protection: use a long one you haven't used anywhere else.
    - You can turn on "New phones from outside also need Allow on the PC" for one more lock (don't, when you are away from the PC).
    - End it at any time: "End" on the bar or Ctrl+D; "View only" is available too. A phone idle for 30 minutes is disconnected.
  Limits:
    - Windows running as administrator (e.g. Task Manager), UAC prompts and the lock screen (incl. Ctrl+Alt+Del) can't be operated from the phone (the phone page says so when you click one).
    - The PC must be on and signed in.
    - The fixed address needs Tailscale running on the PC.
  Troubleshooting:
    - No sound: tap "Tap for sound" on the page (always on iPhone); check the page's volume isn't 0%; turn Windows mute off.
    - Can't connect: at home use the same Wi-Fi (AP isolation / guest network, Windows Firewall or a VPN block it); for the fixed
      address check the PC is on, signed in, Tailscale is running and "Turn the fixed address on when the PC starts" is ticked
      (the first time, wait while it says "Preparing the address"); after 5 wrong passwords wait 15 minutes.
    - Forgot the password: reset it on the PC in the "Remote control" tab (it can't be reset from the phone).

[Phone mirroring] FAQ
7. FAQ
  - Zizai Cast doesn't appear: check same network, firewall allowed, VPN off;
    only one AirPlay receiver can run on a PC (close AirServer, UxPlay, etc. first).
  - Clicked Cancel on the firewall: search "Allow an app through Windows Firewall" → Change settings → tick "Private" for Zizai Cast.
  - Picture but no sound: turn up the iPhone volume, check the Ring/Silent switch and the Windows volume / output device.
  - Lag, stutter: use 5 GHz Wi-Fi or connect the PC by cable, and set Picture quality to "Standard (smoothest)".
  - Blurry picture: set Picture quality to "High (recommended)" or "Highest (needs strong Wi-Fi)" (applies from the next connection, or click "Reconnect now to apply").
  - Someone else's phone took over: set "When a second phone connects" to "Keep the current one", or turn on "Ask new phones for a PIN".
  - Miracast won't connect: check that the PC supports Wireless Display, Wi-Fi is on and Mobile hotspot is off; otherwise use option 2.
  - Wireless debugging keeps dropping: turn wireless debugging on again; turn off the phone's battery saver; if that fails, remove the pairing and scan the QR code again.

Settings and log: %LOCALAPPDATA%\PhoneMirror (settings.ini, phonemirror.log).
Uninstall: Settings → Apps → Zizai Cast → Uninstall (screenshots and recordings are kept).
License: GPL-3.0 (see LICENSE.txt). The AirPlay part is ported from UxPlay.
