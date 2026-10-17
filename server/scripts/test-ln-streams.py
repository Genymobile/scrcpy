#!/usr/bin/env python3
"""Host-only real-Ln subprocess stream tests; no Android runtime required."""
import pathlib
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='scrcpy-ln-streams-') as temp:
    build = pathlib.Path(temp)
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
    harness = build / 'StreamProbe.java'
    harness.write_text('''import com.genymobile.scrcpy.util.Ln;
import java.io.DataOutputStream;
import java.io.FileDescriptor;
import java.io.FileOutputStream;
public final class StreamProbe {
    public static void main(String[] args) throws Exception {
        if (!args[0].equals("default")) {
            Ln.useStdErr();
        }
        if (args[0].equals("stderr-disabled")) {
            Ln.disableSystemStreams();
        }
        Ln.initLogLevel(Ln.Level.VERBOSE);
        Ln.v("verbose marker");
        Ln.d("debug marker");
        Ln.i("info marker");
        Ln.w("warning marker");
        Ln.e("error marker");
        DataOutputStream ready = new DataOutputStream(new FileOutputStream(FileDescriptor.out));
        ready.writeInt(0x4250454e);
        ready.flush();
    }
}
''')
    subprocess.run(['javac', '-Xlint:all', '-Werror', '-d', temp, str(stub),
                    str(root / 'server/src/main/java/com/genymobile/scrcpy/util/Ln.java'),
                    str(harness)], check=True, timeout=30)
    for mode in ('default', 'stderr', 'stderr-disabled'):
        result = subprocess.run(['java', '-cp', temp, 'StreamProbe', mode],
                                check=True, capture_output=True, timeout=10)
        if mode != 'default':
            assert result.stdout == b'BPEN', (mode, result.stdout)
            for marker in (b'verbose marker', b'debug marker', b'info marker',
                           b'warning marker', b'error marker'):
                assert marker in result.stderr, (mode, marker, result.stderr)
        else:
            for marker in (b'verbose marker', b'debug marker', b'info marker'):
                assert marker in result.stdout, (mode, marker, result.stdout)
                assert marker not in result.stderr
            assert result.stdout.endswith(b'BPEN')
            assert b'warning marker' in result.stderr and b'error marker' in result.stderr
print('LnStreams: binary stdout, all log levels on stderr, disabled System streams and default routing passed')
