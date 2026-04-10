package com.genymobile.scrcpy.video;

import com.genymobile.scrcpy.device.Size;

public class VideoConstraints {
    private final int maxSize;
    private final int alignment;
    private final Size maxCodecLandscapeSize;
    private final Size maxCodecPortraitSize;

    public VideoConstraints(int maxSize, int alignment, Size maxCodecLandscapeSize, Size maxCodecPortraitSize) {
        assert maxSize >= 0 : "Max size must not be negative";
        this.maxSize = maxSize;

        assert alignment > 0 : "Alignment must be positive";
        assert (alignment & (alignment - 1)) == 0 : "Alignment must be a power-of-two";
        this.alignment = alignment;

        assert maxCodecLandscapeSize != null;
        this.maxCodecLandscapeSize = maxCodecLandscapeSize;

        assert maxCodecPortraitSize != null;
        this.maxCodecPortraitSize = maxCodecPortraitSize;
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
     * Return the max landscape size supported by the codec.
     *
     * @return the max landscape size
     */
    public Size getMaxCodecLandscapeSize() {
        return maxCodecLandscapeSize;
    }

    /**
     * Return the max portrait size supported by the codec.
     *
     * @return the max portrait size
     */
    public Size getMaxCodecPortraitSize() {
        return maxCodecPortraitSize;
    }
}
