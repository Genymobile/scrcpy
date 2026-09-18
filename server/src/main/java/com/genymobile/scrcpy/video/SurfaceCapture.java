package com.genymobile.scrcpy.video;

import com.genymobile.scrcpy.model.ConfigurationException;
import com.genymobile.scrcpy.model.Size;

import android.view.Surface;

import java.io.IOException;

/**
 * A video source which can be rendered on a Surface for encoding.
 */
public abstract class SurfaceCapture {

    private CaptureControl captureControl;
    private VideoConstraints videoConstraints;

    /**
     * Called once before the first capture starts.
     */
    public final void init(CaptureControl captureControl, VideoConstraints videoConstraints) throws ConfigurationException, IOException {
        this.captureControl = captureControl;
        this.videoConstraints = videoConstraints;
        init(videoConstraints);
    }

    public CaptureControl getCaptureControl() {
        return captureControl;
    }

    /**
     * Update the maximum encoded video size and restart the current capture.
     *
     * @param maxSize the new maximum long edge
     * @return true if the new constraints were accepted
     */
    public final synchronized boolean setMaxSize(int maxSize) {
        if (videoConstraints == null || videoConstraints.getMaxSize() == maxSize) {
            return videoConstraints != null;
        }
        VideoConstraints updated = videoConstraints.withMaxSize(maxSize);
        if (!applyNewVideoConstraints(updated)) {
            return false;
        }
        videoConstraints = updated;
        captureControl.reset(CaptureControl.RESET_REASON_CLIENT_RESIZED);
        return true;
    }

    /**
     * Called once before the first capture starts.
     *
     * @param videoConstraints the video constraints
     */
    protected abstract void init(VideoConstraints videoConstraints) throws ConfigurationException, IOException;

    /**
     * Called after the last capture ends (if and only if {@link #init(VideoConstraints)} has been called).
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
     * Set new video constraints (called by the encoder to retry when encoding fails)
     *
     * @param videoConstraints the video constraints
     * @return {@code true} if the implementation accepts the new constraints, {@code false} otherwise.
     */
    protected abstract boolean applyNewVideoConstraints(VideoConstraints videoConstraints);
}
