#!/usr/bin/env python3
"""Host-only real-Ln subprocess protocol checks; no Android or evdev access."""
import pathlib
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='scrcpy-boox-startup-') as temp:
    build = pathlib.Path(temp)
    # Test-only Android logger sink; console streams remain the real production Ln.
    stub = build / 'android/util/Log.java'
    stub.parent.mkdir(parents=True)
    stub.write_text('''package android.util;
public final class Log {
    public static int v(String t, String m) { return 0; }
    public static int d(String t, String m) { return 0; }
    public static int i(String t, String m) { return 0; }
    public static int w(String t, String m, Throwable e) { return 0; }
    public static int e(String t, String m, Throwable e) { return 0; }
}
''')
    harness = build / 'StartupProbe.java'
    harness.write_text('''import com.genymobile.scrcpy.util.Ln;
import java.io.DataOutputStream;
import java.io.FileDescriptor;
import java.io.FileOutputStream;
public final class StartupProbe {
    public static void main(String[] args) throws Exception {
        if (args[0].equals("child")) {
            Ln.useStdErr();
        }
        Ln.initLogLevel(Ln.Level.VERBOSE);
        Ln.v("verbose marker");
        Ln.d("debug marker");
        Ln.i("BOOX automatic input mapping: 270 degrees, 1404x1872");
        Ln.w("writer warning");
        Ln.e("error marker");
        DataOutputStream ready = new DataOutputStream(new FileOutputStream(FileDescriptor.out));
        ready.writeInt(0x4250454e);
        ready.flush();
    }
}
''')
    subprocess.run(['javac', '-Xlint:all', '-Werror', '-d', temp, str(stub),
                    str(root / 'server/src/main/java/com/genymobile/scrcpy/util/Ln.java'),
                    str(harness)], check=True)
    for mode in ('default', 'child'):
        result = subprocess.run(['java', '-cp', temp, 'StartupProbe', mode],
                                check=True, capture_output=True, timeout=10)
        if mode == 'child':
            assert result.stdout == b'BPEN', (mode, result.stdout)
            for marker in (b'verbose marker', b'debug marker', b'BOOX automatic input mapping',
                           b'writer warning', b'error marker'):
                assert marker in result.stderr, (mode, marker, result.stderr)
        else:
            assert b'BOOX automatic input mapping' in result.stdout
            assert b'verbose marker' in result.stdout and b'debug marker' in result.stdout
            assert result.stdout.endswith(b'BPEN')
            assert b'writer warning' in result.stderr and b'error marker' in result.stderr
    # Integration guard: the child must select stderr before starting workers or discovery.
    process = (root / 'server/src/main/java/com/genymobile/scrcpy/control/BooxPenProcess.java').read_text()
    main = process.split('public static void main(String... args) {', 1)[1]
    assert main.index('Ln.useStdErr();') < main.index('Executors.newSingleThreadScheduledExecutor()')
print('BooxStartup: binary-only READY, all child log levels on stderr, default logging unchanged; wiring verified')
