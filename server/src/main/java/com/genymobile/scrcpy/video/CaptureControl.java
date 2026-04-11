package com.genymobile.scrcpy.video;

import android.media.MediaCodec;

public class CaptureControl {

    public static final int RESET_REASON_TERMINATE = 1;
    public static final int RESET_REASON_SIZE_CHANGED = 1 << 1;
    public static final int RESET_REASON_CLIENT_RESET = 1 << 2;
    public static final int RESET_REASON_CLIENT_RESIZED = 1 << 2;

    private int reset = 0;

    // Current instance of MediaCodec to "interrupt" on reset
    private MediaCodec runningMediaCodec;

    public synchronized int getReset() {
        return reset;
    }

    public synchronized boolean isResetRequested() {
        return reset != 0;
    }

    public synchronized int consumeReset() {
        int value = reset;
        reset = 0;
        return value;
    }

    public synchronized void reset(int reason) {
        assert reason != 0;
        reset |= reason;
        if (runningMediaCodec != null) {
            try {
                runningMediaCodec.signalEndOfInputStream();
            } catch (IllegalStateException e) {
                // ignore
            }
        }
    }

    public synchronized void setRunningMediaCodec(MediaCodec runningMediaCodec) {
        this.runningMediaCodec = runningMediaCodec;
    }
}
