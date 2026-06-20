package com.genymobile.scrcpy.video;

import com.genymobile.scrcpy.model.Size;

public class VideoConstraints {
    private final int maxSize;
    private final int alignment;
    private final Size maxCodecLandscapeSize;
    private final Size maxCodecPortraitSize;
    private final int minCodecSize;

    public VideoConstraints(int maxSize, int alignment, Size maxCodecLandscapeSize, Size maxCodecPortraitSize, int minCodecSize) {
        assert maxSize >= 0 : "Max size must not be negative";
        this.maxSize = maxSize;

        assert alignment > 0 : "Alignment must be positive";
        assert (alignment & (alignment - 1)) == 0 : "Alignment must be a power-of-two";
        this.alignment = alignment;

        assert maxCodecLandscapeSize != null;
        this.maxCodecLandscapeSize = maxCodecLandscapeSize;

        assert maxCodecPortraitSize != null;
        this.maxCodecPortraitSize = maxCodecPortraitSize;

        assert minCodecSize >= 0;
        this.minCodecSize = minCodecSize;
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

    /**
     * Return the min size supported by the codec.
     *
     * @return the min size
     */
    public int getMinCodecSize() {
        return minCodecSize;
    }

    /**
     * Return the video constraints with the provided max size.
     *
     * @param maxSize the max requested size
     * @return the new video constraints
     */
    public VideoConstraints withMaxSize(int maxSize) {
        return new VideoConstraints(maxSize, alignment, maxCodecLandscapeSize, maxCodecPortraitSize, minCodecSize);
    }
}
