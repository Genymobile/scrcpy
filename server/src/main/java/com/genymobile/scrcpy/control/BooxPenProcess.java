package com.genymobile.scrcpy.control;

import com.genymobile.scrcpy.Server;
import com.genymobile.scrcpy.util.Ln;

import android.os.SystemClock;
import android.system.Os;

import java.io.Closeable;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.channels.FileLock;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/** Private child of scrcpy, like CleanUp: EOF releases ink even if its parent dies. No listener or installed service. */
public final class BooxPenProcess implements Closeable {
    private static final int READY = 0x4250454e;
    private final Process process;
    private final DataOutputStream out;
    private boolean closed;

    public BooxPenProcess(int rotation, int width, int height) throws IOException {
        ProcessBuilder builder = new ProcessBuilder("app_process", "/", BooxPenProcess.class.getName(), String.valueOf(rotation),
                String.valueOf(width), String.valueOf(height));
        builder.environment().put("CLASSPATH", Server.SERVER_PATH);
        builder.redirectError(ProcessBuilder.Redirect.INHERIT);
        process = builder.start();
        out = new DataOutputStream(process.getOutputStream());
        AtomicReference<IOException> failure = new AtomicReference<>();
        Thread ready = new Thread(() -> {
            try {
                if (new DataInputStream(process.getInputStream()).readInt() != READY) {
                    throw new IOException("Invalid BOOX writer startup reply");
                }
            } catch (IOException e) {
                failure.set(e);
            }
        }, "boox-ready");
        ready.setDaemon(true);
        ready.start();
        try {
            ready.join(10000);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            close();
            throw new IOException("Interrupted starting BOOX writer", e);
        }
        if (ready.isAlive() || failure.get() != null) {
            close();
            throw new IOException("BOOX writer startup failed or timed out", failure.get());
        }
    }

    public synchronized void inject(int action, long pointer, float x, float y, float pressure) throws IOException {
        if (closed) {
            throw new IOException("BOOX writer closed");
        }
        out.writeInt(action);
        out.writeLong(pointer);
        out.writeFloat(x);
        out.writeFloat(y);
        out.writeFloat(pressure);
        out.flush();
    }

    @Override
    public synchronized void close() throws IOException {
        if (closed) {
            return;
        }
        closed = true;
        try {
            out.close(); // EOF: child's finally releases before dropping the ownership lock.
        } finally {
            try {
                if (!process.waitFor(2, TimeUnit.SECONDS)) {
                    process.destroy();
                }
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
    }

    private static String readMetadata(String... command) throws IOException, InterruptedException {
        Process p = new ProcessBuilder(command).redirectErrorStream(true).start();
        // Capability output is bounded on the supported device. Drain concurrently with the timeout.
        java.io.ByteArrayOutputStream bytes = new java.io.ByteArrayOutputStream();
        Thread drain = new Thread(() -> {
            try {
                byte[] buf = new byte[4096];
                int n;
                while ((n = p.getInputStream().read(buf)) != -1) {
                    if (bytes.size() + n > 262144) {
                        p.destroy();
                        return;
                    }
                    bytes.write(buf, 0, n);
                }
            } catch (IOException e) {
                p.destroy();
            }
        }, "boox-discovery");
        drain.setDaemon(true);
        drain.start();
        if (!p.waitFor(3, TimeUnit.SECONDS)) {
            p.destroy();
            throw new IOException("BOOX input metadata discovery timed out");
        }
        drain.join(1000);
        if (drain.isAlive() || p.exitValue() != 0 || bytes.size() >= 262144) {
            throw new IOException("BOOX input metadata discovery failed");
        }
        return new String(bytes.toByteArray(), StandardCharsets.UTF_8);
    }

    public static void main(String... args) {
        Ln.useStdErr(); // stdout is exclusively the binary READY protocol, including before startup.
        ScheduledExecutorService watchdog = Executors.newSingleThreadScheduledExecutor();
        try {
            Os.setsid(); // Survive termination of the parent's adb shell session.
            try (RandomAccessFile lockFile = new RandomAccessFile("/data/local/tmp/scrcpy-boox-pen.lock", "rw");
                    FileLock lock = lockFile.getChannel().tryLock()) {
                if (lock == null) {
                    throw new IOException("Another scrcpy BOOX writer owns the digitizer");
                }
                BooxDevice d = BooxDevice.parse(readMetadata("getevent", "-lp"));
                int rotation = Integer.parseInt(args[0]);
                if (rotation == -2) {
                    BooxMapping mapping = BooxMapping.parse(readMetadata("dumpsys", "input"), d.path, d.minX, d.maxX, d.minY, d.maxY);
                    mapping.requireDisplaySize(Integer.parseInt(args[1]), Integer.parseInt(args[2]));
                    rotation = mapping.rotation;
                    // Recheck physical idle state after the potentially slow metadata query.
                    BooxDevice fresh = BooxDevice.parse(readMetadata("getevent", "-lp"));
                    if (!fresh.path.equals(d.path) || fresh.minX != d.minX || fresh.maxX != d.maxX
                            || fresh.minY != d.minY || fresh.maxY != d.maxY || fresh.maxPressure != d.maxPressure) {
                        throw new IOException("BOOX digitizer changed during discovery");
                    }
                    d = fresh;
                    Ln.i("BOOX automatic input mapping: " + (rotation * 90) + " degrees, " + mapping.width + "x" + mapping.height);
                }
                // Never cache event numbers or reopen a replaced device after an error.
                try (BooxPen pen = new BooxPen(new FileOutputStream(d.path), android.os.Process.is64Bit(),
                        d.minX, d.maxX, d.minY, d.maxY, d.maxPressure, d.x, d.y, d.distance, rotation)) {
                    Ln.w("BOOX pen writer: " + d.path + "; do not use the physical pen concurrently");
                    watchdog.scheduleAtFixedRate(() -> {
                        try {
                            pen.expire(SystemClock.uptimeMillis());
                        } catch (IOException e) {
                            Ln.e("BOOX watchdog release failed", e);
                            // Fatal I/O: retry release/close, then terminate instead of accepting more ink.
                            try {
                                pen.close();
                            } catch (IOException closeError) {
                                Ln.e("BOOX digitizer could not be restored", closeError);
                            }
                            System.exit(1);
                        }
                    }, 100, 100, TimeUnit.MILLISECONDS);
                    DataOutputStream ready = new DataOutputStream(new FileOutputStream(FileDescriptor.out));
                    ready.writeInt(READY);
                    ready.flush();
                    BooxPenSession.run(new FileInputStream(FileDescriptor.in), pen, SystemClock::uptimeMillis);
                }
            }
        } catch (Exception e) {
            Ln.e("BOOX writer stopped", e);
        } finally {
            watchdog.shutdownNow();
        }
    }
}
