package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.FakeContext;

import com.genymobile.scrcpy.util.Ln;

import android.content.ClipData;
import android.content.ClipDescription;
import android.content.ContentResolver;
import android.content.Context;
import android.net.Uri;
import android.os.Build;
import android.provider.MediaStore;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.IOException;
import java.io.File;
import java.io.FileOutputStream;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;

public final class ClipboardManager {

    public static class ClipboardImage {
        private final String mimeType;
        private final byte[] data;

        public ClipboardImage(String mimeType, byte[] data) {
            this.mimeType = mimeType;
            this.data = data;
        }

        public String mimeType() {
            return mimeType;
        }

        public byte[] data() {
            return data;
        }
    }

    private final android.content.ClipboardManager manager;

    private File cachedFolder;

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

    public ClipboardImage getImage() {
        ClipData clipData = manager.getPrimaryClip();
        if (clipData == null || clipData.getItemCount() == 0) {
            return null;
        }

        ClipData.Item item = clipData.getItemAt(0);

        // Check if it's an image URI
        ClipDescription description = clipData.getDescription();
        if (description != null) {
            String mimeType = description.filterMimeTypes("image/*")[0];
            if (mimeType == null) {
                return null;
            }

            Uri uri = item.getUri();
            if (uri == null) {
                return null;
            }

            try (InputStream inputStream = FakeContext.get().getContentResolver().openInputStream(uri);
                ByteArrayOutputStream outputStream = new ByteArrayOutputStream()) {

                if (inputStream != null) {
                    byte[] buffer = new byte[8192];
                    int bytesRead;
                    while ((bytesRead = inputStream.read(buffer)) != -1) {
                        outputStream.write(buffer, 0, bytesRead);
                    }
                    return new ClipboardImage(mimeType, outputStream.toByteArray());
                }
            } catch (IOException e) {
                // Failed to read image data from URI
                Ln.e("Failed to read clipboard image", e);
            }
        }

        return null;
    }

    public boolean setImage(byte[] imageData, String mimeType) {
        try {
            if (cachedFolder == null) {
                android.content.Context context = FakeContext.get();

                if (Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.N) {
                    context = context.createDeviceProtectedStorageContext();
                }

                File fileRoot = context.getFilesDir();
                cachedFolder = new File(fileRoot, "bugreports");
                cachedFolder.mkdirs();
            }

            // Use atomic write: write to temporary file first, then move to final location
            File tempFile = new File(cachedFolder, "clipboard.tmp");
            try (FileOutputStream fos = new FileOutputStream(tempFile)) {
                fos.write(imageData);
            }

            File finalFile = new File(cachedFolder, "clipboard");
            Files.move(tempFile.toPath(), finalFile.toPath(), StandardCopyOption.REPLACE_EXISTING);

            android.net.Uri uri = android.net.Uri.parse("content://com.android.shell/bugreports/clipboard");

            ClipData clipData = new ClipData(
                null,
                new String[]{mimeType},
                new ClipData.Item(uri)
            );
            manager.setPrimaryClip(clipData);
            return true;
        } catch (Exception e) {
            Ln.e("Failed to set image clipboard", e);
            return false;
        }
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
