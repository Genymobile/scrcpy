#!/usr/bin/env python3
"""Exercise the real parent constructor with host-only discovery/child executables.

Android APIs are compile-only stubs. The private writer main and evdev are never run.
"""
import os
from pathlib import Path
import subprocess
import tempfile
import time

root = Path(__file__).resolve().parents[2]
base = root / 'server/src/main/java/com/genymobile/scrcpy'


def snapshot(pressure=0, distance=0):
    return ('add device 1: /dev/input/event7\n name: "onyx_emp_Wacom I2C Digitizer"\n'
            ' ABS_X : value 0, min 0, max 210\n ABS_Y : value 0, min 0, max 420\n'
            f' ABS_PRESSURE : value {pressure}, min 0, max 4095\n'
            f' ABS_DISTANCE : value {distance}, min 0, max 255\n')


with tempfile.TemporaryDirectory(prefix='scrcpy-boox-parent-') as temp:
    build = Path(temp)
    stubs = {
        'com/genymobile/scrcpy/Server.java': 'package com.genymobile.scrcpy; public final class Server { public static final String SERVER_PATH = "host-fixture"; }',
        'com/genymobile/scrcpy/util/Ln.java': '''package com.genymobile.scrcpy.util;
public final class Ln {
 public static void useStdErr() {}
 public static void w(String text) { System.err.println(text); }
 public static void i(String text) { System.err.println(text); }
 public static void e(String text, Throwable e) { System.err.println(text); }
}''',
        'android/os/SystemClock.java': 'package android.os; public final class SystemClock { public static long uptimeMillis() { return 0; } }',
        'android/os/Process.java': 'package android.os; public final class Process { public static boolean is64Bit() { return true; } }',
        'android/system/Os.java': 'package android.system; public final class Os { public static void setsid() {} }',
        'com/genymobile/scrcpy/control/ParentProbe.java': '''package com.genymobile.scrcpy.control;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.concurrent.atomic.AtomicReference;
public final class ParentProbe {
 public static void main(String[] args) throws Exception {
  if (args[0].equals("startup")) {
   try (BooxPenProcess p = new BooxPenProcess(0, 210, 420)) {
    System.out.println("WRITER_READY");
   }
   return;
  }
  if (args[0].equals("cancel-startup")) {
   AtomicReference<Throwable> failure = new AtomicReference<>();
   Thread thread = new Thread(() -> {
    try (BooxPenProcess p = new BooxPenProcess(0, 210, 420)) { }
    catch (Exception e) { failure.set(e); }
   });
   thread.setDaemon(true);
   thread.start();
   long deadline = System.nanoTime() + 2000000000L;
   while (!Files.exists(Paths.get(args[1])) && System.nanoTime() < deadline) { Thread.sleep(5); }
   Thread.sleep(200);
   thread.interrupt();
   thread.join(5000);
   if (thread.isAlive() || !(failure.get() instanceof InterruptedException)) { throw new AssertionError("Startup cancellation failed", failure.get()); }
   System.out.println("CANCELLED");
   return;
  }
  Method method = BooxPenProcess.class.getDeclaredMethod("readMetadata", String[].class);
  method.setAccessible(true);
  String[] command = Arrays.copyOfRange(args, 2, args.length);
  if (args[0].equals("cancel")) {
   AtomicReference<Throwable> failure = new AtomicReference<>();
   Thread thread = new Thread(() -> {
    try { method.invoke(null, (Object) command); }
    catch (InvocationTargetException e) { failure.set(e.getCause()); }
    catch (Exception e) { failure.set(e); }
   });
   thread.start();
   long deadline = System.nanoTime() + 2000000000L;
   while (!Files.exists(Paths.get(args[1])) && System.nanoTime() < deadline) { Thread.sleep(5); }
   thread.interrupt();
   thread.join(2000);
   if (thread.isAlive() || !(failure.get() instanceof InterruptedException)) { throw new AssertionError("Cancellation failed"); }
   System.out.println("CANCELLED");
  } else {
   try { System.out.print(method.invoke(null, (Object) command)); }
   catch (InvocationTargetException e) { throw (Exception) e.getCause(); }
  }
 }
}''',
    }
    sources = []
    for name, content in stubs.items():
        path = build / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
        sources.append(str(path))
    for name in ('BooxDevice', 'BooxMapping', 'BooxPen', 'BooxPenSession', 'BooxPenInitialization', 'BooxPenStartup', 'BooxPenPreparation', 'BooxPenProcess'):
        sources.append(str(base / 'control' / (name + '.java')))
    subprocess.run(['javac', '-Xlint:all,-try', '-Werror', '-d', temp, *sources], check=True)
    command = ['java', '-cp', temp, 'com.genymobile.scrcpy.control.ParentProbe']
    caps = build / 'caps'
    caps.write_text(snapshot())
    marker = build / 'spawned'
    getevent = build / 'getevent'
    getevent.write_text('#!/usr/bin/env python3\nfrom pathlib import Path\nprint(Path(' + repr(str(caps)) + ').read_text(), end="")\n')
    getevent.chmod(0o755)
    child = build / 'app_process'
    child.write_text('#!/usr/bin/env python3\nfrom pathlib import Path\nimport sys\nPath(' + repr(str(marker)) + ').write_text("spawned")\nsys.stdout.buffer.write(b"BPEN"); sys.stdout.buffer.flush()\nsys.stdin.buffer.read()\n')
    child.chmod(0o755)
    env = {**os.environ, 'PATH': temp + os.pathsep + os.environ['PATH']}
    stderr = build / 'stderr'
    with stderr.open('w') as err:
        p = subprocess.Popen([*command, 'startup'], env=env, stdout=subprocess.PIPE, stderr=err, text=True)
        try:
            deadline = time.monotonic() + 5
            while 'Pen not initialized' not in stderr.read_text() and time.monotonic() < deadline:
                assert p.poll() is None, stderr.read_text()
                time.sleep(0.02)
            assert 'Pen not initialized' in stderr.read_text(), stderr.read_text()
            assert not marker.exists(), 'Child spawned before physical initialization'
            caps.write_text(snapshot(120, 0))
            time.sleep(1.2)
            assert p.poll() is None and not marker.exists(), 'Contact must not start writer'
            caps.write_text(snapshot(0, 56))
            stdout, _ = p.communicate(timeout=5)
            assert p.returncode == 0 and stdout == 'WRITER_READY\n', (stdout, stderr.read_text())
            assert marker.exists(), 'Writer did not start after strict idle'
            assert stderr.read_text().count('Pen not initialized') == 1, stderr.read_text()
        finally:
            if p.poll() is None:
                p.kill()
                p.communicate()
    # Deterministic handoff race: the parent saw idle, but the fresh child sees contact.
    marker.unlink()
    caps.write_text(snapshot(0, 56))
    attempts = build / 'attempts'
    child.write_text('#!/usr/bin/env python3\nfrom pathlib import Path\nimport sys\np=Path(' + repr(str(attempts)) + ')\nn=int(p.read_text())+1 if p.exists() else 1\np.write_text(str(n))\nif n == 1:\n Path(' + repr(str(caps)) + ').write_text(' + repr(snapshot(120, 0)) + ')\n sys.stdout.buffer.write(b"BUSY"); sys.stdout.buffer.flush()\nelse:\n sys.stdout.buffer.write(b"BPEN"); sys.stdout.buffer.flush(); sys.stdin.buffer.read()\n')
    with stderr.open('w') as err:
        p = subprocess.Popen([*command, 'startup'], env=env, stdout=subprocess.PIPE, stderr=err, text=True)
        try:
            deadline = time.monotonic() + 5
            while not attempts.exists() and time.monotonic() < deadline:
                time.sleep(0.02)
            assert attempts.exists(), stderr.read_text()
            time.sleep(1.2)
            assert p.poll() is None, 'Handoff became busy and crashed instead of returning to wait: ' + stderr.read_text()
            assert attempts.read_text() == '1', 'Do not respawn while physical contact persists'
            caps.write_text(snapshot(0, 56))
            stdout, _ = p.communicate(timeout=6)
            assert p.returncode == 0 and stdout == 'WRITER_READY\n', (stdout, stderr.read_text())
            assert attempts.read_text() == '2', 'Retry must complete within the same parent session'
            assert stderr.read_text().count('Pen not initialized') == 1, stderr.read_text()
        finally:
            if p.poll() is None:
                p.kill()
                p.communicate()
    # Repeated BUSY is paced, keeps one notice and preserves direct identity arguments.
    attempts.unlink()
    caps.write_text(snapshot(0, 56))
    race_child = child.read_text()
    identity = ['/dev/input/event7', 'onyx_emp_Wacom I2C Digitizer', '0', '210', '0', '420', '4095', '255']
    child.write_text('#!/usr/bin/env python3\nfrom pathlib import Path\nimport sys\nassert sys.argv[6:] == ' + repr(identity) + '\np=Path(' + repr(str(attempts)) + ')\nn=int(p.read_text())+1 if p.exists() else 1\np.write_text(str(n))\nsys.stdout.buffer.write(b"BUSY" if n < 3 else b"BPEN"); sys.stdout.buffer.flush()\nif n >= 3: sys.stdin.buffer.read()\n')
    result = subprocess.run([*command, 'startup'], env=env, capture_output=True, text=True, timeout=8)
    assert result.returncode == 0 and result.stdout == 'WRITER_READY\n', result.stderr
    assert attempts.read_text() == '3' and result.stderr.count('Pen not initialized') == 1, result.stderr
    # EOF/unknown replies are fatal, not a disguised physical-activity retry.
    for reply in (b'', b'NOPE'):
        attempts.unlink()
        child.write_text('#!/usr/bin/env python3\nfrom pathlib import Path\nimport sys\nPath(' + repr(str(attempts)) + ').write_text("1")\nsys.stdout.buffer.write(' + repr(reply) + '); sys.stdout.buffer.flush()\n')
        result = subprocess.run([*command, 'startup'], env=env, capture_output=True, text=True, timeout=5)
        assert result.returncode != 0 and attempts.read_text() == '1', result.stderr
        assert 'EOFException' in result.stderr if not reply else 'Invalid BOOX writer startup reply' in result.stderr
    # Same address is not sufficient: changed capability after BUSY is fatal.
    attempts.unlink()
    child.write_text(race_child)
    with stderr.open('w') as err:
        p = subprocess.Popen([*command, 'startup'], env=env, stdout=subprocess.PIPE, stderr=err, text=True)
        try:
            deadline = time.monotonic() + 5
            while caps.read_text() != snapshot(120, 0) and time.monotonic() < deadline:
                time.sleep(0.02)
            assert caps.read_text() == snapshot(120, 0), stderr.read_text()
            caps.write_text(snapshot(0, 56).replace('max 210', 'max 211'))
            p.communicate(timeout=5)
            assert p.returncode != 0 and 'digitizer changed' in stderr.read_text(), stderr.read_text()
            assert attempts.read_text() == '1', 'Replacement must never spawn another child'
        finally:
            if p.poll() is None:
                p.kill()
                p.communicate()
    # Cancellation during a pending handshake and after BUSY must reap the attempted child.
    for busy in (False, True):
        caps.write_text(snapshot(0, 56))
        child_pid = build / ('child-pid-' + str(busy))
        child.write_text('#!/usr/bin/env python3\nfrom pathlib import Path\nimport os,sys\nPath(' + repr(str(child_pid)) + ').write_text(str(os.getpid()))\n'
                         + ('Path(' + repr(str(caps)) + ').write_text(' + repr(snapshot(120, 0)) + ')\nsys.stdout.buffer.write(b"BUSY"); sys.stdout.buffer.flush()\n' if busy else 'sys.stdin.buffer.read()\n'))
        result = subprocess.run([*command, 'cancel-startup', str(child_pid)], env=env, capture_output=True, text=True, timeout=8)
        assert result.returncode == 0 and result.stdout == 'CANCELLED\n', result.stderr
        assert not Path('/proc/' + child_pid.read_text()).exists(), 'Cancelled attempt survived'
    # Real metadata child cancellation must destroy the process, not only interrupt the Java caller.
    pidfile = build / 'pid'
    sleeper = build / 'sleep.py'
    sleeper.write_text('import os,time\nfrom pathlib import Path\nPath(' + repr(str(pidfile)) + ').write_text(str(os.getpid()))\ntime.sleep(30)\n')
    result = subprocess.run([*command, 'cancel', str(pidfile), 'python3', str(sleeper)], capture_output=True, text=True, timeout=6)
    assert result.returncode == 0 and result.stdout == 'CANCELLED\n', (result.stdout, result.stderr)
    pid = int(pidfile.read_text())
    deadline = time.monotonic() + 2
    while Path(f'/proc/{pid}').exists() and time.monotonic() < deadline:
        time.sleep(0.02)
    assert not Path(f'/proc/{pid}').exists(), 'Cancelled discovery process survived'
    # Metadata errors remain fatal, rather than being converted to human-wait mode.
    result = subprocess.run([*command, 'metadata', '-', 'python3', '-c', 'raise SystemExit(7)'], capture_output=True, text=True, timeout=6)
    assert result.returncode != 0 and 'discovery failed' in result.stderr, result.stderr
    # Oversized successful output must fail even when the process has already exited.
    result = subprocess.run([*command, 'metadata', '-', 'python3', '-c',
                             'import sys; sys.stdout.write("x" * 262145)'],
                            capture_output=True, text=True, timeout=6)
    assert result.returncode != 0 and 'discovery failed' in result.stderr, 'Oversized metadata was accepted'
print('BooxParentProcess: boot wait, repeated BUSY handoff, same-session resume, one notice, identity binding, fatal EOF/unknown replies and cancellation passed (host substitutes only)')
