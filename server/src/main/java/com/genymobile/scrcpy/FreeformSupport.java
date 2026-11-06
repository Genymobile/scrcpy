package com.genymobile.scrcpy;

import com.genymobile.scrcpy.util.Ln;
import com.genymobile.scrcpy.util.Settings;
import com.genymobile.scrcpy.util.SettingsException;

import android.os.Looper;
import android.system.Os;

import java.io.Closeable;
import java.io.FileDescriptor;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.RandomAccessFile;
import java.nio.channels.FileChannel;
import java.nio.channels.FileLock;
import java.nio.channels.OverlappingFileLockException;
import java.util.Objects;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.FutureTask;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Enables freeform support for one explicitly requested virtual-display session.
 *
 * The independent process restores the setting when the server dies. Killing this process too,
 * or rebooting the device, cannot guarantee restoration. The lock coordinates participating
 * scrcpy sessions only; Android does not provide an atomic compare-and-restore for settings.
 */
public final class FreeformSupport implements Closeable {

    private static final String KEY = "enable_freeform_support";
    private static final String LOCK_PATH = "/data/local/tmp/scrcpy-freeform.lock";
    private static final int READY = 1;
    private static final long TIMEOUT_MS = 5000;

    private final Process process;
    private boolean closed;

    private FreeformSupport(Process process) {
        this.process = process;
    }

    public static FreeformSupport start() throws IOException {
        ProcessBuilder builder = new ProcessBuilder("app_process", "/", FreeformSupport.class.getName());
        builder.environment().put("CLASSPATH", Server.SERVER_PATH);
        builder.redirectError(ProcessBuilder.Redirect.INHERIT);
        FreeformSupport support = new FreeformSupport(builder.start());
        FutureTask<Integer> ready = new FutureTask<>(() -> {
            try (InputStream in = support.process.getInputStream()) {
                return in.read();
            }
        });
        Thread reader = new Thread(ready, "freeform-ready");
        reader.setDaemon(true);
        reader.start();

        try {
            if (ready.get(TIMEOUT_MS, TimeUnit.MILLISECONDS) != READY) {
                throw new IOException("Could not enable freeform support (see guardian error)");
            }
            return support;
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            throw closeAfterFailure(support, new IOException("Interrupted while enabling freeform support", e));
        } catch (ExecutionException | TimeoutException e) {
            throw closeAfterFailure(support, new IOException("Freeform support guardian did not become ready", e));
        } catch (IOException e) {
            throw closeAfterFailure(support, e);
        }
    }

    private static IOException closeAfterFailure(FreeformSupport support, IOException error) {
        try {
            support.close();
        } catch (IOException cleanupError) {
            error.addSuppressed(cleanupError);
        }
        return error;
    }

    @Override
    public synchronized void close() throws IOException {
        if (closed) {
            return;
        }
        closed = true;
        // EOF requests restoration. Never kill the guardian: it may still be restoring the setting.
        IOException error = null;
        try {
            process.getOutputStream().close();
        } catch (IOException e) {
            error = e;
        }
        try {
            if (!process.waitFor(TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
                throw new IOException("Freeform support guardian is still cleaning up");
            }
            if (process.exitValue() != 0) {
                throw new IOException("Freeform support guardian failed (see guardian error)");
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            IOException interrupted = new IOException("Interrupted while waiting for freeform support restoration", e);
            if (error != null) {
                interrupted.addSuppressed(error);
            }
            throw interrupted;
        } catch (IOException e) {
            if (error != null) {
                e.addSuppressed(error);
            }
            throw e;
        }
        if (error != null) {
            throw error;
        }
    }

    interface SettingStore {
        String get() throws IOException;
        void put(String value) throws IOException;
        void delete() throws IOException;
    }

    // Kept independent of Android APIs so that failure and restoration semantics can be tested.
    static final class SettingSession implements Closeable {
        private final SettingStore store;
        private final String original;
        private boolean changed;

        SettingSession(SettingStore store) throws IOException {
            this.store = store;
            original = store.get();
        }

        void enable() throws IOException {
            if (!"1".equals(original)) {
                // Arm restoration before writing: a failed binder call may already have changed the value.
                changed = true;
                store.put("1");
            }
            if (!"1".equals(store.get())) {
                throw new IOException("Could not verify freeform support setting");
            }
        }

        @Override
        public void close() throws IOException {
            if (!changed || !"1".equals(store.get())) {
                // Leave a later external change alone. This comparison is necessarily non-atomic.
                return;
            }
            if (original == null) {
                store.delete();
            } else {
                store.put(original);
            }
            if (!Objects.equals(original, store.get())) {
                throw new IOException("Could not restore freeform support setting");
            }
            changed = false;
        }
    }

    static FileLock acquireLock(FileChannel channel) throws IOException {
        FileLock lock;
        try {
            lock = channel.tryLock();
        } catch (OverlappingFileLockException e) {
            throw new IOException("Another scrcpy session owns the freeform support setting", e);
        }
        if (lock == null) {
            throw new IOException("Another scrcpy session owns the freeform support setting");
        }
        return lock;
    }

    static void runSession(SettingStore store, InputStream lifetime, OutputStream ready) throws IOException {
        try (SettingSession session = new SettingSession(store)) {
            session.enable();
            // This only confirms the setting write. The server must also verify Android display eligibility.
            ready.write(READY);
            ready.flush();
            while (lifetime.read() != -1) {
                // The pipe's lifetime, rather than its contents, represents ownership.
            }
        }
    }

    private static SettingStore createStore() {
        return new SettingStore() {
            @Override
            public String get() throws IOException {
                try {
                    return Settings.getValue(Settings.TABLE_GLOBAL, KEY);
                } catch (SettingsException e) {
                    throw new IOException(e);
                }
            }

            @Override
            public void put(String value) throws IOException {
                try {
                    Settings.putValue(Settings.TABLE_GLOBAL, KEY, value);
                } catch (SettingsException e) {
                    throw new IOException(e);
                }
            }

            @Override
            public void delete() throws IOException {
                try {
                    Settings.deleteValue(Settings.TABLE_GLOBAL, KEY);
                } catch (SettingsException e) {
                    throw new IOException(e);
                }
            }
        };
    }

    @SuppressWarnings("deprecation")
    public static void main(String... args) {
        // Reserve stdout for the readiness byte, including while applying Android workarounds.
        Ln.initLogLevel(Ln.Level.WARN);
        Ln.disableSystemStreams();
        int status = 1;
        try {
            Os.setsid();
            Looper.prepareMainLooper();
            Workarounds.apply();
            try (RandomAccessFile file = new RandomAccessFile(LOCK_PATH, "rw");
                    FileLock lock = acquireLock(file.getChannel())) {
                // Acquire ownership even if the setting is already enabled by another participant.
                runSession(createStore(), System.in, new FileOutputStream(FileDescriptor.out));
            }
            status = 0;
        } catch (Throwable e) {
            Ln.e("Freeform support guardian failed", e);
        } finally {
            System.exit(status);
        }
    }
}
