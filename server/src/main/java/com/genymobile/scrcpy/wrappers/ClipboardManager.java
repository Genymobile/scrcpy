package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.FakeContext;

import android.content.ClipData;
import android.content.Context;

public final class ClipboardManager {
    private final android.content.ClipboardManager manager;

    static ClipboardManager create() {
        android.content.ClipboardManager manager = (android.content.ClipboardManager) FakeContext.get().getSystemService(Context.CLIPBOARD_SERVICE);
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

    public void addPrimaryClipChangedListener(android.content.ClipboardManager.OnPrimaryClipChangedListener listener) {
        manager.addPrimaryClipChangedListener(listener);
    }
}
