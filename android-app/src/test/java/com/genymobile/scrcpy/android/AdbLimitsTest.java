package com.genymobile.scrcpy.android;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThrows;

import java.io.IOException;
import java.io.ByteArrayOutputStream;

import org.junit.Test;

public final class AdbLimitsTest {
    @Test
    public void acceptsReasonableVideoSizes() throws IOException {
        assertEquals(1920 * 1080L, AdbLimits.videoPixels(1920, 1080));
    }

    @Test
    public void rejectsUnreasonableVideoSizes() {
        assertThrows(IOException.class, () -> AdbLimits.videoPixels(0, 1080));
        assertThrows(IOException.class, () -> AdbLimits.videoPixels(8193, 1080));
        assertThrows(IOException.class, () -> AdbLimits.videoPixels(8192, 8192));
    }

    @Test
    public void rejectsOversizedPackets() {
        assertThrows(IOException.class, () -> AdbLimits.checkVideoPacketLength(16 * 1024 * 1024 + 1));
        assertThrows(IOException.class, () -> AdbLimits.checkShellOutputSize(64 * 1024 + 1));
    }

    @Test
    public void boundsShellOutputAndRoundsShortDeadlinesUp() throws IOException {
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        AdbLimits.appendShellOutput(output, new byte[64 * 1024]);
        assertEquals(64 * 1024, output.size());
        assertThrows(IOException.class, () -> AdbLimits.appendShellOutput(output, new byte[1]));
        assertEquals(1L, AdbLimits.remainingMillis(1_000_000L, 999_999L));
        assertEquals(0L, AdbLimits.remainingMillis(999_999L, 1_000_000L));
    }
}
