package com.genymobile.scrcpy.android;

import java.io.IOException;
import java.io.ByteArrayOutputStream;

final class AdbLimits {
    static final int MAX_VIDEO_DIMENSION = 8192;
    static final long MAX_VIDEO_PIXELS = 16L * 1024 * 1024;
    static final int MAX_VIDEO_PACKET_SIZE = 16 * 1024 * 1024;
    static final int MAX_SHELL_OUTPUT_SIZE = 64 * 1024;

    private AdbLimits() {
    }

    static long videoPixels(int width, int height) throws IOException {
        if (width <= 0 || height <= 0 || width > MAX_VIDEO_DIMENSION
                || height > MAX_VIDEO_DIMENSION || (long) width * height > MAX_VIDEO_PIXELS) {
            throw new IOException("Target returned invalid video size: " + width + "x" + height);
        }
        return (long) width * height;
    }

    static void checkVideoPacketLength(int length) throws IOException {
        if (length <= 0 || length > MAX_VIDEO_PACKET_SIZE) {
            throw new IOException("Invalid video packet length: " + length);
        }
    }

    static void checkShellOutputSize(int length) throws IOException {
        if (length < 0 || length > MAX_SHELL_OUTPUT_SIZE) {
            throw new IOException("ADB shell output exceeded the safety limit");
        }
    }

    static void appendShellOutput(ByteArrayOutputStream output, byte[] chunk) throws IOException {
        long size = (long) output.size() + chunk.length;
        if (size > MAX_SHELL_OUTPUT_SIZE) {
            throw new IOException("ADB shell output exceeded the safety limit");
        }
        output.write(chunk, 0, chunk.length);
    }

    static long remainingMillis(long deadlineNanos, long nowNanos) {
        long remainingNanos = deadlineNanos - nowNanos;
        if (remainingNanos <= 0) {
            return 0;
        }
        return Math.max(1L, (remainingNanos + 999_999L) / 1_000_000L);
    }
}
