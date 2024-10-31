package com.genymobile.scrcpy.video;

import java.util.concurrent.atomic.AtomicBoolean;

public class CaptureReset implements SurfaceCapture.CaptureListener {

    private final AtomicBoolean reset = new AtomicBoolean();

    public boolean consumeReset() {
        return reset.getAndSet(false);
    }

    public void reset() {
        reset.set(true);
    }

    @Override
    public void onInvalidated() {
        reset();
    }
}
