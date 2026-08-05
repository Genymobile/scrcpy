package com.genymobile.scrcpy.ime;

import android.graphics.Matrix;
import android.inputmethodservice.InputMethodService;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.view.inputmethod.CursorAnchorInfo;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

public final class ScrcpyInputMethodService extends InputMethodService {
    private static final long[] CURSOR_ANCHOR_RETRY_DELAYS_MS = {50, 250, 1000};

    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private int cursorAnchorRequestGeneration;
    private boolean cursorAnchorReceived;

    @Override
    public void onCreate() {
        super.onCreate();
        ScrcpyImeProvider.setInputMethodService(this);
    }

    @Override
    public View onCreateInputView() {
        return null;
    }

    @Override
    public boolean onEvaluateInputViewShown() {
        super.onEvaluateInputViewShown();
        return false;
    }

    void commitText(String text) {
        commitText(getCurrentInputConnection(), text);
    }

    static boolean commitText(InputConnection connection, String text) {
        return connection != null && connection.commitText(text, 1);
    }

    void setComposingText(String text) {
        setComposingText(getCurrentInputConnection(), text);
    }

    static boolean setComposingText(InputConnection connection, String text) {
        return connection != null && connection.setComposingText(text, 1);
    }

    @Override
    public void onStartInput(EditorInfo attribute, boolean restarting) {
        super.onStartInput(attribute, restarting);
        requestCursorAnchorUpdates();
    }

    void requestCursorAnchorUpdates() {
        int generation = ++cursorAnchorRequestGeneration;
        cursorAnchorReceived = false;
        requestCursorAnchorUpdateOnce();
        for (long delay : CURSOR_ANCHOR_RETRY_DELAYS_MS) {
            mainHandler.postDelayed(() -> retryCursorAnchorUpdate(generation), delay);
        }
    }

    private void retryCursorAnchorUpdate(int generation) {
        if (generation == cursorAnchorRequestGeneration && !cursorAnchorReceived) {
            requestCursorAnchorUpdateOnce();
        }
    }

    private void requestCursorAnchorUpdateOnce() {
        InputConnection connection = getCurrentInputConnection();
        boolean requested = requestCursorAnchorUpdates(connection);
        if (!requested) {
            ScrcpyImeProvider.sendCursorAnchor(false, 0, 0, 0, 0);
        }
    }

    static boolean requestCursorAnchorUpdates(InputConnection connection) {
        return connection != null && connection.requestCursorUpdates(
                InputConnection.CURSOR_UPDATE_IMMEDIATE | InputConnection.CURSOR_UPDATE_MONITOR);
    }

    @Override
    public void onUpdateCursorAnchorInfo(CursorAnchorInfo info) {
        super.onUpdateCursorAnchorInfo(info);
        if (info == null) {
            ScrcpyImeProvider.sendCursorAnchor(false, 0, 0, 0, 0);
            return;
        }
        cursorAnchorReceived = true;

        float horizontal = info.getInsertionMarkerHorizontal();
        float top = info.getInsertionMarkerTop();
        float bottom = info.getInsertionMarkerBottom();
        int flags = info.getInsertionMarkerFlags();
        boolean invisible = (flags & CursorAnchorInfo.FLAG_HAS_INVISIBLE_REGION) != 0
                && (flags & CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION) == 0;
        if (Float.isNaN(horizontal) || Float.isNaN(top) || Float.isNaN(bottom) || invisible) {
            ScrcpyImeProvider.sendCursorAnchor(false, 0, 0, 0, 0);
            return;
        }

        float[] points = {horizontal, top, horizontal, bottom};
        Matrix matrix = info.getMatrix();
        matrix.mapPoints(points);
        ScrcpyImeProvider.sendCursorAnchor(true, points[0], points[1], points[2], points[3]);
    }

    @Override
    public void onUpdateSelection(int oldSelStart, int oldSelEnd, int newSelStart, int newSelEnd,
                                  int candidatesStart, int candidatesEnd) {
        super.onUpdateSelection(oldSelStart, oldSelEnd, newSelStart, newSelEnd, candidatesStart, candidatesEnd);
        if (!cursorAnchorReceived) {
            requestCursorAnchorUpdateOnce();
        }
    }

    @Override
    public void onFinishInput() {
        ++cursorAnchorRequestGeneration;
        cursorAnchorReceived = false;
        ScrcpyImeProvider.sendCursorAnchor(false, 0, 0, 0, 0);
        super.onFinishInput();
    }

    void restoreInputMethod(String inputMethodId) {
        try {
            switchInputMethod(inputMethodId);
        } catch (IllegalArgumentException e) {
            // The shell watchdog remains the final restoration layer.
        }
    }

    @Override
    public void onDestroy() {
        ++cursorAnchorRequestGeneration;
        mainHandler.removeCallbacksAndMessages(null);
        ScrcpyImeProvider.clearInputMethodService(this);
        super.onDestroy();
    }
}
