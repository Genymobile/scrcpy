package com.genymobile.scrcpy.ime;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.os.Binder;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.provider.Settings;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.EOFException;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

public final class ScrcpyImeProvider extends ContentProvider {
    private static final String IME_ID = "com.genymobile.scrcpy.ime/.ScrcpyInputMethodService";

    private static final int MAGIC = 0x5343494d; // "SCIM"
    private static final int PROTOCOL_VERSION = 1;
    private static final int ACK_OK = 0;
    private static final int ACK_BUSY = 1;
    private static final int ACK_INCOMPATIBLE = 2;
    private static final int FRAME_TEXT = 1;
    private static final int FRAME_CLOSE = 2;
    private static final int FRAME_COMPOSING_TEXT = 3;
    private static final int TEXT_MAX_LENGTH = 300;
    private static final int SHELL_UID = 2000;
    private static final long RESTORE_DELAY_MS = 2000;

    private static volatile ScrcpyInputMethodService inputMethodService;

    private final Object connectionLock = new Object();
    private final Handler mainHandler = new Handler(Looper.getMainLooper());

    private ParcelFileDescriptor activeDescriptor;
    private Runnable pendingRestore;

    static void setInputMethodService(ScrcpyInputMethodService service) {
        inputMethodService = service;
    }

    static void clearInputMethodService(ScrcpyInputMethodService service) {
        if (inputMethodService == service) {
            inputMethodService = null;
        }
    }

    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
        if (Binder.getCallingUid() != SHELL_UID) {
            throw new SecurityException("Only the Android shell may connect to scrcpy IME");
        }
        if (!"connection".equals(uri.getLastPathSegment())) {
            throw new FileNotFoundException("Unknown scrcpy IME endpoint");
        }

        ParcelFileDescriptor[] descriptors;
        try {
            descriptors = ParcelFileDescriptor.createSocketPair();
        } catch (IOException e) {
            throw new FileNotFoundException("Could not create scrcpy IME socket pair: " + e.getMessage());
        }

        ParcelFileDescriptor localDescriptor = descriptors[0];
        Thread connectionThread = new Thread(() -> handleConnection(localDescriptor), "scrcpy-ime-client");
        connectionThread.setDaemon(true);
        connectionThread.start();
        return descriptors[1];
    }

    private void handleConnection(ParcelFileDescriptor descriptor) {
        boolean active = false;
        boolean graceful = false;
        String originalIme = null;
        try (DataInputStream input = new DataInputStream(new ParcelFileDescriptor.AutoCloseInputStream(descriptor));
             DataOutputStream output = new DataOutputStream(
                     new ParcelFileDescriptor.AutoCloseOutputStream(ParcelFileDescriptor.dup(descriptor.getFileDescriptor())))) {
            int magic = input.readInt();
            int version = input.readUnsignedByte();
            int originalImeLength = input.readUnsignedShort();
            byte[] originalImeBytes = new byte[originalImeLength];
            input.readFully(originalImeBytes);
            originalIme = new String(originalImeBytes, StandardCharsets.UTF_8);

            if (magic != MAGIC || version != PROTOCOL_VERSION || originalIme.isEmpty()) {
                output.writeByte(ACK_INCOMPATIBLE);
                output.flush();
                return;
            }

            synchronized (connectionLock) {
                if (activeDescriptor != null) {
                    output.writeByte(ACK_BUSY);
                    output.flush();
                    return;
                }
                if (pendingRestore != null) {
                    mainHandler.removeCallbacks(pendingRestore);
                    pendingRestore = null;
                }
                activeDescriptor = descriptor;
                active = true;
            }

            output.writeByte(ACK_OK);
            output.flush();

            while (true) {
                int frameType = input.readUnsignedByte();
                if (frameType == FRAME_CLOSE) {
                    graceful = true;
                    break;
                }
                if (frameType != FRAME_TEXT && frameType != FRAME_COMPOSING_TEXT) {
                    throw new IOException("Unknown scrcpy IME frame: " + frameType);
                }
                int length = input.readInt();
                if (length < 0 || length > TEXT_MAX_LENGTH) {
                    throw new IOException("Invalid scrcpy IME text length: " + length);
                }
                byte[] textBytes = new byte[length];
                input.readFully(textBytes);
                String text = new String(textBytes, StandardCharsets.UTF_8);
                ParcelFileDescriptor sessionDescriptor = descriptor;
                boolean composing = frameType == FRAME_COMPOSING_TEXT;
                mainHandler.post(() -> sendText(sessionDescriptor, text, composing));
            }
        } catch (EOFException e) {
            // Unexpected disconnect; schedule restoration below.
        } catch (IOException e) {
            // Unexpected disconnect or malformed input; schedule restoration below.
        } finally {
            if (active) {
                boolean restore = !graceful;
                String restoreIme = originalIme;
                mainHandler.post(() -> finishSession(descriptor, restore ? restoreIme : null));
            }
        }
    }

    private void finishSession(ParcelFileDescriptor descriptor, String originalIme) {
        synchronized (connectionLock) {
            if (activeDescriptor != descriptor) {
                return;
            }
            activeDescriptor = null;
        }
        if (originalIme != null) {
            scheduleRestore(originalIme);
        }
    }

    private void sendText(ParcelFileDescriptor sessionDescriptor, String text, boolean composing) {
        synchronized (connectionLock) {
            if (activeDescriptor != sessionDescriptor) {
                return;
            }
        }
        ScrcpyInputMethodService service = inputMethodService;
        if (service != null) {
            if (composing) {
                service.setComposingText(text);
            } else {
                service.commitText(text);
            }
        }
    }

    private void scheduleRestore(String originalIme) {
        synchronized (connectionLock) {
            if (activeDescriptor != null) {
                return;
            }
            pendingRestore = () -> {
                synchronized (connectionLock) {
                    if (activeDescriptor != null) {
                        return;
                    }
                    pendingRestore = null;
                }
                String currentIme = Settings.Secure.getString(getContext().getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD);
                ScrcpyInputMethodService service = inputMethodService;
                if (IME_ID.equals(currentIme) && service != null) {
                    service.restoreInputMethod(originalIme);
                }
            };
            mainHandler.postDelayed(pendingRestore, RESTORE_DELAY_MS);
        }
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder) {
        return null;
    }

    @Override
    public String getType(Uri uri) {
        return null;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        return null;
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        return 0;
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        return 0;
    }
}
