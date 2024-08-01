package com.genymobile.scrcpy.video;

import com.genymobile.scrcpy.device.Size;

import android.view.Surface;

import java.io.IOException;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * A video source which can be rendered on a Surface for encoding.
 */
public abstract class SurfaceCapture {

    private final AtomicBoolean resetCapture = new AtomicBoolean();

    /**
     * Request the encoding session to be restarted, for example if the capture implementation detects that the video source size has changed (on
     * device rotation for example).
     */
    protected void requestReset() {
        resetCapture.set(true);
    }

    /**
     * Consume the reset request (intended to be called by the encoder).
     *
     * @return {@code true} if a reset request was pending, {@code false} otherwise.
     */
    public boolean consumeReset() {
        return resetCapture.getAndSet(false);
    }

    /**
     * Called once before the capture starts.
     */
    public abstract void init() throws IOException;

    /**
     * Called after the capture ends (if and only if {@link #init()} has been called).
     */
    public abstract void release();

    /**
     * Start the capture to the target surface.
     *
     * @param surface the surface which will be encoded
     */
    public abstract void start(Surface surface) throws IOException;

    /**
     * Return the video size
     *
     * @return the video size
     */
    public abstract Size getSize();

    /**
     * Set the maximum capture size (set by the encoder if it does not support the current size).
     *
     * @param maxSize Maximum size
     */
    public abstract boolean setMaxSize(int maxSize);

    /**
     * Indicate if the capture has been closed internally.
     *
     * @return {@code true} is the capture is closed, {@code false} otherwise.
     */
    public boolean isClosed() {
        return false;
    }
}
