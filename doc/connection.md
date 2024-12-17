# Connection

## Selection

If exactly one device is connected (i.e. listed by `adb devices`), then it is
automatically selected.

However, if there are multiple devices connected, you must specify the one to
use in one of 4 ways:
 - by its serial:
   ```bash
   scrcpy --serial=0123456789abcdef
   scrcpy -s 0123456789abcdef   # short version

   # the serial is the ip:port if connected over TCP/IP (same behavior as adb)
   scrcpy --serial=192.168.1.1:5555
   ```
 - the one connected over USB (if there is exactly one):
   ```bash
   scrcpy --select-usb
   scrcpy -d   # short version
   ```
 - the one connected over TCP/IP (if there is exactly one):
   ```bash
   scrcpy --select-tcpip
   scrcpy -e   # short version
   ```
 - a device already listening on TCP/IP (see [below](#tcpip-wireless)):
   ```bash
   scrcpy --tcpip=192.168.1.1:5555
   scrcpy --tcpip=192.168.1.1        # default port is 5555
   ```

The serial may also be provided via the environment variable `ANDROID_SERIAL`
(also used by `adb`):

```bash
# in bash
export ANDROID_SERIAL=0123456789abcdef
scrcpy
```

```cmd
:: in cmd
set ANDROID_SERIAL=0123456789abcdef
scrcpy
```

```powershell
# in PowerShell
$env:ANDROID_SERIAL = '0123456789abcdef'
scrcpy
```


## TCP/IP (wireless)

_Scrcpy_ uses `adb` to communicate with the device, and `adb` can [connect] to a
device over TCP/IP. The device must be connected on the same network as the
computer.

[connect]: https://developer.android.com/studio/command-line/adb.html#wireless


### Automatic

An option `--tcpip` allows to configure the connection automatically. There are
two variants.

If _adb_ TCP/IP mode is disabled on the device (or if you don't know the IP
address), connect the device over USB, then run:

```bash
scrcpy --tcpip   # without arguments
```

It will automatically find the device IP address and adb port, enable TCP/IP
mode if necessary, then connect to the device before starting.

If the device (accessible at 192.168.1.1 in this example) already listens on a
port (typically 5555) for incoming _adb_ connections, then run:

```bash
scrcpy --tcpip=192.168.1.1       # default port is 5555
scrcpy --tcpip=192.168.1.1:5555
```

Prefix the address with a '+' to force a reconnection:

```bash
scrcpy --tcpip=+192.168.1.1
```


### Manual

Alternatively, it is possible to enable the TCP/IP connection manually using
`adb`:

1. Plug the device into a USB port on your computer.
2. Connect the device to the same Wi-Fi network as your computer.
3. Get your device IP address, in Settings → About phone → Status, or by
   executing this command:

    ```bash
    adb shell ip route | awk '{print $9}'
    ```

4. Enable `adb` over TCP/IP on your device: `adb tcpip 5555`.
5. Unplug your device.
6. Connect to your device: `adb connect DEVICE_IP:5555` _(replace `DEVICE_IP`
with the device IP address you found)_.
7. Run `scrcpy` as usual.
8. Run `adb disconnect` once you're done.

Since Android 11, a [wireless debugging option][adb-wireless] allows to bypass
having to physically connect your device directly to your computer.

[adb-wireless]: https://developer.android.com/studio/command-line/adb#wireless-android11-command-line


## Autostart

A small tool (by the scrcpy author) allows to run arbitrary commands whenever a
new Android device is connected: [AutoAdb]. It can be used to start scrcpy:

```bash
autoadb scrcpy -s '{}'
```

Alternatively, on Windows, Task Scheduler and Event Viewer can be used to run
scrcpy when a particular device is connected:

1. Open Event Viewer & go to Applications and Services Logs > Microsoft >
   Windows > DeviceSetupManager > Admin.
2. (If connected, disconnect the device you want to use scrcpy with, then)
   Connect the device (you may need to select MTP/File Transfer or Charge
   Only), and refresh the Admin view with F5.
4. Look for a recent event with Level: Information & Event ID: 112.
5. Click the event to open it, then open the Details tab and select XML View.
6. The XML should look similar to the screenshot below, with a unique ID
   for the device concerned (highlighted in the red box). Make note of this
   entire ID, including '{}', for use in the next steps.  
   ![Event Viewer Screenshot](https://github.com/Genymobile/scrcpy/assets/58115698/b85dcb05-d68b-44b2-ac73-87f878f68aad)
8. Open Task Scheduler, and select Create Task from the right hand side panel.
9. Give your task a name, then select the Triggers tab.
10. Create a new trigger, and select Begin the task: On an event.
11. Select Custom, then click New Event Filter.
12. Select the XML tab and check the Edit query manually box.
13. Replace any existing XML if there is any present, and paste the following:
    ```xml
    <QueryList>
      <Query Id="0" Path="Microsoft-Windows-DeviceSetupManager/Admin">
        <Select Path="Microsoft-Windows-DeviceSetupManager/Admin">*[System[Provider[@Name='Microsoft-Windows-DeviceSetupManager'] and (Level=4 or Level=0) and (EventID=112)]] and *[EventData[Data[@Name='Prop_ContainerId'] and (Data='YOUR_DEVICE_ID_HERE')]]</Select>
      </Query>
    </QueryList>
    ```
 14. Replace "YOUR_DEVICE_ID_HERE" with the device ID from step 6, and verify
     that that your secreen appears similar to the following screenshot.  
     ![Task Scheduler Triggers Screenshot](https://github.com/Genymobile/scrcpy/assets/58115698/fa808038-a133-4e6b-9491-65b674a2f003)
 15. Select OK, then ensure that only the Enabled checkbox is checked on the
     window, then select OK again.
 16. Select the Actions tab.
 17. Select New, and input "cmd.exe" into Program/script.
 18. Paste `/c "taskkill /im scrcpy.exe /t /f"` into Add arguments.
 19. Select OK, then add another New action.
 20. In Program/script, use the path to whichever script for scrpy you wish
     to run.
 21. Select OK.  
     ![Task Scheduler Complete Screenshot](https://github.com/Genymobile/scrcpy/assets/58115698/c0c6516e-023f-4c0e-a10b-3a0099db918d)
 22. Change Conditions and Settings to taste.  
     Note that the actual task execution duration does not include the duration
     of scrcpy's execution.
 23. Select OK to create the task.




[AutoAdb]: https://github.com/rom1v/autoadb
