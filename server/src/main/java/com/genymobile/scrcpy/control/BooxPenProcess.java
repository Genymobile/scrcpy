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
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/** Private child of scrcpy, like CleanUp: EOF releases ink even if its parent dies. No listener or installed service. */
public final class BooxPenProcess implements BooxPenStartup.Writer {
    private static final int READY = 0x4250454e;
    private static final int BUSY = 0x42555359;
    private final Connection connection;
    private boolean closed;

    public BooxPenProcess(int rotation, int width, int height) throws IOException, InterruptedException {
        connection = start(rotation, width, height);
    }

    private static Connection start(int rotation, int width, int height) throws IOException, InterruptedException {
        AtomicBoolean notified = new AtomicBoolean();
        Runnable notice = () -> {
            if (notified.compareAndSet(false, true)) {
                Ln.w("Pen not initialized on the tablet. Please touch the screen with the stylus, then lift it away to continue.");
            }
        };
        BooxDevice expected = BooxPenInitialization.awaitIdle(() -> readMetadata("getevent", "-lp"), notice, Thread::sleep);
        while (true) {
            if (Thread.currentThread().isInterrupted()) {
                throw new InterruptedException("BOOX pen startup cancelled");
            }
            List<String> command = new ArrayList<>();
            Collections.addAll(command, "app_process", "/", BooxPenProcess.class.getName(), String.valueOf(rotation),
                    String.valueOf(width), String.valueOf(height));
            Collections.addAll(command, expected.identity());
            Connection attempt = new Connection(new ProcessBuilder(command));
            boolean accepted = false;
            try {
                int reply = attempt.awaitReply();
                if (reply == READY) {
                    accepted = true;
                    return attempt;
                }
                if (reply != BUSY) {
                    throw new IOException("Invalid BOOX writer startup reply");
                }
            } finally {
                if (!accepted) {
                    attempt.close();
                }
            }
            notice.run();
            Thread.sleep(1000); // Pace retries even if activity ends before the next parent probe.
            BooxPenInitialization.awaitIdle(() -> readMetadata("getevent", "-lp"), notice, Thread::sleep, expected);
        }
    }

    public synchronized void inject(int action, long pointer, float x, float y, float pressure) throws IOException {
        if (closed) {
            throw new IOException("BOOX writer closed");
        }
        DataOutputStream out = connection.out;
        out.writeInt(action);
        out.writeLong(pointer);
        out.writeFloat(x);
        out.writeFloat(y);
        out.writeFloat(pressure);
        out.flush();
    }

    @Override
    public synchronized void close() throws IOException {
        if (!closed) {
            closed = true;
            connection.close();
        }
    }

    /** One bounded startup attempt; only an explicit pre-open BUSY reply permits a retry. */
    private static final class Connection implements Closeable {
        private final Process process;
        private final DataOutputStream out;

        Connection(ProcessBuilder builder) throws IOException {
            builder.environment().put("CLASSPATH", Server.SERVER_PATH);
            builder.redirectError(ProcessBuilder.Redirect.INHERIT);
            process = builder.start();
            out = new DataOutputStream(process.getOutputStream());
        }

        int awaitReply() throws IOException, InterruptedException {
            AtomicInteger reply = new AtomicInteger();
            AtomicReference<IOException> failure = new AtomicReference<>();
            Thread ready = new Thread(() -> {
                try {
                    reply.set(new DataInputStream(process.getInputStream()).readInt());
                } catch (IOException e) {
                    failure.set(e);
                }
            }, "boox-ready");
            ready.setDaemon(true);
            ready.start();
            ready.join(10000);
            if (ready.isAlive() || failure.get() != null) {
                throw new IOException("BOOX writer startup failed or timed out", failure.get());
            }
            return reply.get();
        }

        @Override
        public void close() throws IOException {
            try {
                out.close(); // EOF releases an accepted writer before it drops the ownership lock.
            } finally {
                try {
                    if (!process.waitFor(2, TimeUnit.SECONDS)) {
                        process.destroy();
                    }
                } catch (InterruptedException e) {
                    process.destroy();
                    Thread.currentThread().interrupt();
                }
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
        try {
            if (!p.waitFor(3, TimeUnit.SECONDS)) {
                throw new IOException("BOOX input metadata discovery timed out");
            }
            drain.join(1000);
            if (drain.isAlive() || p.exitValue() != 0 || bytes.size() >= 262144) {
                throw new IOException("BOOX input metadata discovery failed");
            }
            return new String(bytes.toByteArray(), StandardCharsets.UTF_8);
        } finally {
            p.destroy(); // Also stop discovery if startup is cancelled during waitFor/join.
        }
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
                BooxPenPreparation preparation = BooxPenPreparation.prepare(() -> readMetadata("getevent", "-lp"),
                        () -> readMetadata("dumpsys", "input"), Integer.parseInt(args[0]),
                        Integer.parseInt(args[1]), Integer.parseInt(args[2]), Arrays.copyOfRange(args, 3, args.length));
                if (preparation == null) {
                    DataOutputStream reply = new DataOutputStream(new FileOutputStream(FileDescriptor.out));
                    reply.writeInt(BUSY);
                    reply.flush();
                    return; // No evdev open or writes; release this attempt's lock before retry.
                }
                BooxDevice d = preparation.device;
                int rotation = preparation.rotation;
                if (preparation.mapping != null) {
                    Ln.i("BOOX automatic input mapping: " + (rotation * 90) + " degrees, "
                            + preparation.mapping.width + "x" + preparation.mapping.height);
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
