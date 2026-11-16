package com.genymobile.scrcpy.video;

import android.media.MediaCodecInfo;

public class VideoConstraints {
    private final int maxSize;
    private final int alignment;
    private final MediaCodecInfo.VideoCapabilities caps;

    public VideoConstraints(int maxSize, int alignment, MediaCodecInfo.VideoCapabilities caps) {
        assert maxSize >= 0 : "Max size must not be negative";
        this.maxSize = maxSize;

        assert alignment > 0 : "Alignment must be positive";
        assert (alignment & (alignment - 1)) == 0 : "Alignment must be a power-of-two";
        this.alignment = alignment;

        this.caps = caps;
    }

    /**
     * Return the max size (0 if not requested)
     *
     * @return the max requested size
     */
    public int getMaxSize() {
        return maxSize;
    }

    /**
     * Return the video alignment.
     * <p>
     * This a power-of-2 value that the video width and height must be multiples of.
     *
     * @return the video alignment
     */
    public int getAlignment() {
        return alignment;
    }

    /**
     * Return the video encoder capabilities.
     *
     * @return the video encoder capabilities
     */
    public MediaCodecInfo.VideoCapabilities getEncoderCapabilities() {
        return caps;
    }

    /**
     * Return the video constraints with the provided max size.
     *
     * @param maxSize the max requested size
     * @return the new video constraints
     */
    public VideoConstraints withMaxSize(int maxSize) {
        return new VideoConstraints(maxSize, alignment, caps);
    }

    /**
     * Return the video constraints with the provided video capabilities.
     *
     * @param caps the video encoder capabilities
     * @return the new video constraints
     */
    public VideoConstraints withCapabilities(MediaCodecInfo.VideoCapabilities caps) {
        return new VideoConstraints(maxSize, alignment, caps);
    }
}
