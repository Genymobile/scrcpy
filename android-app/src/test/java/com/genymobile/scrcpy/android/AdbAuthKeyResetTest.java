package com.genymobile.scrcpy.android;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThrows;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import org.junit.Test;

public final class AdbAuthKeyResetTest {
    @Test
    public void deletesWrappingKeyBeforePreferences() throws IOException {
        List<String> events = new ArrayList<>();
        AdbAuthKeyReset.reset(new AdbAuthKeyReset.Operations() {
            @Override
            public void deleteWrappingKey() {
                events.add("keystore");
            }

            @Override
            public boolean clearPreferences() {
                events.add("preferences");
                return true;
            }
        });

        assertEquals(List.of("keystore", "preferences"), events);
    }

    @Test
    public void reportsPreferenceDeletionFailure() {
        assertThrows(IOException.class, () -> AdbAuthKeyReset.reset(
                new AdbAuthKeyReset.Operations() {
                    @Override
                    public void deleteWrappingKey() {
                    }

                    @Override
                    public boolean clearPreferences() {
                        return false;
                    }
                }));
    }

    @Test
    public void doesNotClearPreferencesIfKeystoreDeletionFails() {
        boolean[] preferencesCleared = {false};
        assertThrows(IOException.class, () -> AdbAuthKeyReset.reset(
                new AdbAuthKeyReset.Operations() {
                    @Override
                    public void deleteWrappingKey() throws Exception {
                        throw new Exception("keystore unavailable");
                    }

                    @Override
                    public boolean clearPreferences() {
                        preferencesCleared[0] = true;
                        return true;
                    }
                }));
        assertFalse(preferencesCleared[0]);
    }
}
