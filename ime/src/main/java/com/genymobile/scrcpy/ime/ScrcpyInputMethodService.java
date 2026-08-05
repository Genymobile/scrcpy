package com.genymobile.scrcpy.ime;

import android.inputmethodservice.InputMethodService;
import android.view.View;
import android.view.inputmethod.InputConnection;

public final class ScrcpyInputMethodService extends InputMethodService {
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

    void restoreInputMethod(String inputMethodId) {
        try {
            switchInputMethod(inputMethodId);
        } catch (IllegalArgumentException e) {
            // The shell watchdog remains the final restoration layer.
        }
    }

    @Override
    public void onDestroy() {
        ScrcpyImeProvider.clearInputMethodService(this);
        super.onDestroy();
    }
}
