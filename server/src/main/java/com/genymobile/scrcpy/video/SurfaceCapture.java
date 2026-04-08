package com.genymobile.scrcpy.video;

import com.genymobile.scrcpy.device.ConfigurationException;
import com.genymobile.scrcpy.device.Size;

import android.view.Surface;

import java.io.IOException;

/**
 * A video source which can be rendered on a Surface for encoding.
 */
public abstract class SurfaceCapture {

    public interface CaptureListener {
        void onInvalidated();
    }

    private CaptureListener listener;
    private int alignment;

    /**
     * Notify the listener that the capture has been invalidated (for example, because its size changed, or due to a manual user request).
     */
    public void invalidate() {
        listener.onInvalidated();
    }

    /**
     * Called once before the first capture starts.
     */
    public final void init(CaptureListener listener, int alignment) throws ConfigurationException, IOException {
        this.listener = listener;
        this.alignment = alignment;
        init();
    }

    /**
     * Called once before the first capture starts.
     */
    protected abstract void init() throws ConfigurationException, IOException;

    /**
     * Called after the last capture ends (if and only if {@link #init()} has been called).
     */
    public abstract void release();

    /**
     * Called once before each capture starts, before {@link #getSize()}.
     */
    public void prepare() throws ConfigurationException, IOException {
        // empty by default
    }

    /**
     * Start the capture to the target surface.
     *
     * @param surface the surface which will be encoded
     */
    public abstract void start(Surface surface) throws IOException;

    /**
     * Stop the capture.
     */
    public void stop() {
        // Do nothing by default
    }

    /**
     * Return the video size
     *
     * @return the video size
     */
    public abstract Size getSize();

    /**
     * Indicate if the capture has been closed internally.
     *
     * @return {@code true} is the capture is closed, {@code false} otherwise.
     */
    public boolean isClosed() {
        return false;
    }

    /**
     * Return the video alignment
     * <p>
     * This a power-of-2 value that the video width and height must be multiples of.
     *
     * @return the video alignment
     */
    protected int getAlignment() {
        return alignment;
    }
}
