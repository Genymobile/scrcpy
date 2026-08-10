package com.genymobile.scrcpy.ime;

import com.genymobile.scrcpy.AsyncProcessor;
import com.genymobile.scrcpy.FakeContext;
import com.genymobile.scrcpy.Server;
import com.genymobile.scrcpy.util.Command;
import com.genymobile.scrcpy.util.Ln;

import android.content.ContentProviderClient;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.os.SystemClock;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.OutputStream;

public final class ImeManager implements AsyncProcessor, AutoCloseable {
    public static final String IME_ID = "com.genymobile.scrcpy.ime/.ScrcpyInputMethodService";
    public static final String PROVIDER_AUTHORITY = "com.genymobile.scrcpy.ime.connection";
    public static final Uri CONNECTION_URI = Uri.parse("content://" + PROVIDER_AUTHORITY + "/connection");

    private static final String METHOD_GET_ORIGINAL_IME = "get_original_ime";
    private static final String METHOD_PREPARE_SESSION = "prepare_session";
    private static final String EXTRA_ORIGINAL_IME = "original_ime";

    private static final int CONNECT_ATTEMPTS = 50;
    private static final int CONNECT_DELAY_MS = 100;
    private final Object lock = new Object();
    private final Process watchdog;
    private final OutputStream watchdogInput;
    private final DataInputStream input;
    private final DataOutputStream output;

    private Thread monitorThread;
    private CursorAnchorListener cursorAnchorListener;
    private boolean stopping;
    private boolean stopped;

    private ImeManager(Process watchdog, ParcelFileDescriptor descriptor) throws IOException {
        this.watchdog = watchdog;
        watchdogInput = watchdog.getOutputStream();
        ParcelFileDescriptor inputDescriptor = ParcelFileDescriptor.dup(descriptor.getFileDescriptor());
        input = new DataInputStream(new ParcelFileDescriptor.AutoCloseInputStream(inputDescriptor));
        output = new DataOutputStream(new ParcelFileDescriptor.AutoCloseOutputStream(descriptor));
    }

    private static boolean containsIme(String imeList, String imeId) {
        for (String line : imeList.split("[\\r\\n]+")) {
            if (imeId.equals(line.trim())) {
                return true;
            }
        }
        return false;
    }

    @SuppressWarnings("deprecation") // ContentProviderClient.close() requires Android 7
    private static Bundle callProvider(String method, String arg) throws IOException {
        ContentProviderClient client = FakeContext.get().getContentResolver().acquireContentProviderClient(PROVIDER_AUTHORITY);
        if (client == null) {
            throw new IOException("Could not access scrcpy IME connection provider");
        }
        try {
            return client.call(method, arg, null);
        } catch (RemoteException | RuntimeException e) {
            throw new IOException("Could not call scrcpy IME connection provider", e);
        } finally {
            client.release();
        }
    }

    private static String getSavedOriginalIme() throws IOException {
        Bundle result = callProvider(METHOD_GET_ORIGINAL_IME, null);
        if (result == null) {
            return null;
        }
        String originalIme = result.getString(EXTRA_ORIGINAL_IME);
        return originalIme == null ? null : originalIme.trim();
    }

    private static void prepareSession(String originalIme) throws IOException {
        callProvider(METHOD_PREPARE_SESSION, originalIme);
    }

    private static Process startWatchdog(String originalIme, boolean wasEnabled) throws IOException {
        String[] cmd = {
                "app_process",
                "/",
                ImeCleanUp.class.getName(),
                originalIme,
                String.valueOf(wasEnabled),
        };
        ProcessBuilder builder = new ProcessBuilder(cmd);
        builder.environment().put("CLASSPATH", Server.SERVER_PATH);
        return builder.start();
    }

    @SuppressWarnings("deprecation") // ContentProviderClient.close() requires Android 7
    private static ParcelFileDescriptor connect() throws IOException {
        IOException lastError = null;
        for (int i = 0; i < CONNECT_ATTEMPTS; ++i) {
            try {
                ContentProviderClient client = FakeContext.get().getContentResolver().acquireContentProviderClient(PROVIDER_AUTHORITY);
                if (client != null) {
                    try {
                        ParcelFileDescriptor descriptor = client.openFile(CONNECTION_URI, "rw", null);
                        if (descriptor != null) {
                            return descriptor;
                        }
                    } finally {
                        client.release();
                    }
                }
                lastError = new FileNotFoundException("scrcpy IME connection provider returned no descriptor");
            } catch (FileNotFoundException e) {
                lastError = e;
            } catch (RemoteException e) {
                lastError = new IOException("Could not open scrcpy IME connection", e);
            } catch (SecurityException e) {
                throw new IOException("scrcpy IME rejected the shell connection", e);
            }
            SystemClock.sleep(CONNECT_DELAY_MS);
        }
        throw new IOException("Could not connect to scrcpy IME", lastError);
    }

    public static ImeManager create() throws IOException {
        String originalIme;
        boolean wasEnabled;
        try {
            originalIme = Command.execReadLine("settings", "get", "secure", "default_input_method");
            if (originalIme != null) {
                originalIme = originalIme.trim();
            }
            if (originalIme == null || originalIme.isEmpty() || "null".equals(originalIme)) {
                throw new IOException("Could not determine the current input method");
            }
            String enabledImes = Command.execReadOutput("ime", "list", "-s");
            wasEnabled = containsIme(enabledImes, IME_ID);
            if (IME_ID.equals(originalIme)) {
                String savedOriginalIme = getSavedOriginalIme();
                if (savedOriginalIme == null || savedOriginalIme.isEmpty() || IME_ID.equals(savedOriginalIme)) {
                    savedOriginalIme = ImeRecovery.findFallbackIme(enabledImes, IME_ID);
                    if (savedOriginalIme == null) {
                        throw new IOException("scrcpy IME is already the default input method and no other enabled input method is available");
                    }
                    Ln.w("The input method from the interrupted scrcpy session is unknown; falling back to: " + savedOriginalIme);
                }
                if (!containsIme(enabledImes, savedOriginalIme)) {
                    Command.exec("ime", "enable", savedOriginalIme);
                }
                Ln.i("Recovering input method left by an interrupted scrcpy session: " + savedOriginalIme);
                Command.exec("ime", "set", savedOriginalIme);
                originalIme = savedOriginalIme;
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            throw new IOException("Interrupted while reading input method state", e);
        }

        prepareSession(originalIme);
        Process watchdog = startWatchdog(originalIme, wasEnabled);
        ImeManager manager = null;
        boolean success = false;
        try {
            if (!wasEnabled) {
                Command.exec("ime", "enable", IME_ID);
            }
            Command.exec("ime", "set", IME_ID);

            ParcelFileDescriptor descriptor = connect();
            manager = new ImeManager(watchdog, descriptor);
            manager.handshake(originalIme);
            success = true;
            Ln.i("scrcpy IME enabled");
            return manager;
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            throw new IOException("Interrupted while enabling scrcpy IME", e);
        } finally {
            if (!success && manager != null) {
                manager.close();
            } else if (!success) {
                try {
                    watchdog.getOutputStream().close();
                    watchdog.waitFor();
                } catch (IOException e) {
                    // ignore the cleanup error after the primary failure
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
            }
        }
    }

    private void handshake(String originalIme) throws IOException {
        ImeProtocol.writeHandshake(output, originalIme);
        output.flush();
        int ack = input.readUnsignedByte();
        if (ack == ImeProtocol.ACK_BUSY) {
            throw new IOException("scrcpy IME is already used by another session");
        }
        if (ack != ImeProtocol.ACK_OK) {
            throw new IOException("scrcpy IME rejected the handshake: " + ack);
        }
    }

    public void sendText(String text) throws IOException {
        synchronized (lock) {
            if (stopping || stopped) {
                throw new IOException("scrcpy IME is disconnected");
            }
            ImeProtocol.writeText(output, text);
            output.flush();
        }
    }

    public void sendComposingText(String text) throws IOException {
        synchronized (lock) {
            if (stopping || stopped) {
                throw new IOException("scrcpy IME is disconnected");
            }
            ImeProtocol.writeComposingText(output, text);
            output.flush();
        }
    }

    public void setCursorAnchorListener(CursorAnchorListener listener) {
        cursorAnchorListener = listener;
    }

    @Override
    public void start(TerminationListener listener) {
        monitorThread = new Thread(() -> {
            boolean fatal = false;
            try {
                while (true) {
                    ImeProtocol.CursorAnchor anchor = ImeProtocol.readCursorAnchor(input);
                    CursorAnchorListener anchorListener = cursorAnchorListener;
                    if (anchorListener != null) {
                        anchorListener.onCursorAnchor(
                                anchor.isValid(), anchor.getX1(), anchor.getY1(), anchor.getX2(), anchor.getY2());
                    }
                }
            } catch (IOException e) {
                synchronized (lock) {
                    fatal = !stopping;
                }
                if (fatal) {
                    Ln.e("scrcpy IME disconnected", e);
                }
            } finally {
                listener.onTerminated(fatal);
            }
        }, "ime-monitor");
        monitorThread.start();
    }

    @Override
    public void stop() {
        synchronized (lock) {
            if (stopping || stopped) {
                return;
            }
            stopping = true;
            try {
                ImeProtocol.writeClose(output);
                output.flush();
            } catch (IOException e) {
                // The watchdog still restores the input method.
            }
            try {
                output.close();
            } catch (IOException e) {
                // ignore
            }
            try {
                input.close();
            } catch (IOException e) {
                // ignore
            }
            try {
                watchdogInput.close();
            } catch (IOException e) {
                Ln.w("Could not notify IME cleanup watchdog", e);
            }
        }
    }

    @Override
    public void join() throws InterruptedException {
        if (monitorThread != null) {
            monitorThread.join();
        }
        watchdog.waitFor();
        synchronized (lock) {
            stopped = true;
        }
    }

    @Override
    public void close() {
        stop();
        try {
            join();
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }

    public interface CursorAnchorListener {
        void onCursorAnchor(boolean valid, float x1, float y1, float x2, float y2);
    }
}
