package com.genymobile.scrcpy.video;

import android.media.MediaCodec;

import java.util.concurrent.atomic.AtomicBoolean;

public class CaptureReset implements SurfaceCapture.CaptureListener {

    private final AtomicBoolean reset = new AtomicBoolean();

    // Current instance of MediaCodec to "interrupt" on reset
    private MediaCodec runningMediaCodec;

    public boolean consumeReset() {
        return reset.getAndSet(false);
    }

    public synchronized void reset() {
        reset.set(true);
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

    @Override
    public void onInvalidated() {
        reset();
    }
}
