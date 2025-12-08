package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.FakeContext;

import android.content.ClipData;
import android.content.Context;
import android.os.Process;

public final class ClipboardManager {
    private final android.content.ClipboardManager manager;

    // Polling mode when running as AID_SYSTEM
    private final boolean usePolling;
    private Thread pollingThread;
    private CharSequence lastText;

    static ClipboardManager create() {
        android.content.ClipboardManager manager =
                (android.content.ClipboardManager) FakeContext.get()
                        .getSystemService(Context.CLIPBOARD_SERVICE);

        if (manager == null) {
            // Some devices have no clipboard manager
            // <https://github.com/Genymobile/scrcpy/issues/1440>
            // <https://github.com/Genymobile/scrcpy/issues/1556>
            return null;
        }
        return new ClipboardManager(manager);
    }

    private ClipboardManager(android.content.ClipboardManager manager) {
        this.manager = manager;

        int uid = Process.myUid();
        this.usePolling = (uid == 1000); // AID_SYSTEM

        if (usePolling) {
            startPollingThread();
        }
    }

    public CharSequence getText() {
        ClipData clipData = manager.getPrimaryClip();
        if (clipData == null || clipData.getItemCount() == 0) {
            return null;
        }
        return clipData.getItemAt(0).getText();
    }

    public boolean setText(CharSequence text) {
        ClipData clipData = ClipData.newPlainText(null, text);
        manager.setPrimaryClip(clipData);
        return true;
    }

    /**
     * In AID_SHELL mode, use normal listener.
     * In AID_SYSTEM mode, the listener does not work; polling calls the listener manually.
     */
    public void addPrimaryClipChangedListener(android.content.ClipboardManager.OnPrimaryClipChangedListener listener) {
        if (!usePolling) {
            manager.addPrimaryClipChangedListener(listener);
            return;
        }

        // Polling mode: wrap listener so polling thread can notify it.
        // Polling thread stores this and invokes on change.
        this.pollingListener = listener;
    }

    private android.content.ClipboardManager.OnPrimaryClipChangedListener pollingListener;

    private void startPollingThread() {
        pollingThread = new Thread(() -> {
            while (!Thread.interrupted()) {
                try {
                    CharSequence now = getText();
                    if (!same(now, lastText)) {
                        lastText = now;
                        if (pollingListener != null) {
                            pollingListener.onPrimaryClipChanged();
                        }
                    }
                    Thread.sleep(250);
                } catch (Throwable ignored) {
                }
            }
        }, "scrcpy-clipboard-poll");
        pollingThread.setDaemon(true);
        pollingThread.start();
    }

    private static boolean same(CharSequence a, CharSequence b) {
        if (a == b) return true;
        if (a == null || b == null) return false;
        return a.toString().contentEquals(b);
    }

    public void destroy() {
        if (usePolling && pollingThread != null) {
            pollingThread.interrupt();
        }
    }
}
