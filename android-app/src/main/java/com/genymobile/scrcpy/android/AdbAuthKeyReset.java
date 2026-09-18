package com.genymobile.scrcpy.android;

import java.io.IOException;

/** Coordinates the destructive part of replacing the local ADB authorization key. */
final class AdbAuthKeyReset {
    interface Operations {
        void deleteWrappingKey() throws Exception;

        boolean clearPreferences();
    }

    private AdbAuthKeyReset() {
    }

    static void reset(Operations operations) throws IOException {
        try {
            operations.deleteWrappingKey();
        } catch (Exception error) {
            throw new IOException("Could not delete the stored ADB authorization key", error);
        }
        if (!operations.clearPreferences()) {
            throw new IOException("Could not clear the stored ADB authorization key");
        }
    }
}
