# Tunnels

Scrcpy is designed to mirror local Android devices. Tunnels allow to connect to
a remote device (e.g. over the Internet).

To connect to a remote device, it is possible to connect a local `adb` client to
a remote `adb` server (provided they use the same version of the _adb_
protocol).


## Remote ADB server

To connect to a remote _adb server_, make the server listen on all interfaces:

```bash
adb kill-server
adb -a nodaemon server start
# keep this open
```

**Warning: all communications between clients and the _adb server_ are
unencrypted.**

Suppose that this server is accessible at 192.168.1.2. Then, from another
terminal, run `scrcpy`:

```bash
# in bash
export ADB_SERVER_SOCKET=tcp:192.168.1.2:5037
scrcpy --tunnel-host=192.168.1.2
```

```cmd
:: in cmd
set ADB_SERVER_SOCKET=tcp:192.168.1.2:5037
scrcpy --tunnel-host=192.168.1.2
```

```powershell
# in PowerShell
$env:ADB_SERVER_SOCKET = 'tcp:192.168.1.2:5037'
scrcpy --tunnel-host=192.168.1.2
```

By default, `scrcpy` uses the local port used for `adb forward` tunnel
establishment (typically `27183`, see `--port`). It is also possible to force a
different tunnel port (it may be useful in more complex situations, when more
redirections are involved):

```
scrcpy --tunnel-port=1234
```


## SSH tunnel

To communicate with a remote _adb server_ securely, it is preferable to use an
SSH tunnel.

First, make sure the _adb server_ is running on the remote computer:

```bash
adb start-server
```

Then, establish an SSH tunnel:

```bash
# local  5038 --> remote  5037
# local 27183 <-- remote 27183
ssh -CN -L5038:localhost:5037 -R27183:localhost:27183 your_remote_computer
# keep this open
```

From another terminal, run `scrcpy`:

```bash
# in bash
export ADB_SERVER_SOCKET=tcp:localhost:5038
scrcpy
```

```cmd
:: in cmd
set ADB_SERVER_SOCKET=tcp:localhost:5038
scrcpy
```

```powershell
# in PowerShell
$env:ADB_SERVER_SOCKET = 'tcp:localhost:5038'
scrcpy
```

To avoid enabling remote port forwarding, you could force a forward connection
instead (notice the `-L` instead of `-R`):

```bash
# local  5038 --> remote  5037
# local 27183 --> remote 27183
ssh -CN -L5038:localhost:5037 -L27183:localhost:27183 your_remote_computer
# keep this open
```

From another terminal, run `scrcpy`:

```bash
# in bash
export ADB_SERVER_SOCKET=tcp:localhost:5038
scrcpy --force-adb-forward
```

```cmd
:: in cmd
set ADB_SERVER_SOCKET=tcp:localhost:5038
scrcpy --force-adb-forward
```

```powershell
# in PowerShell
$env:ADB_SERVER_SOCKET = 'tcp:localhost:5038'
scrcpy --force-adb-forward
```
