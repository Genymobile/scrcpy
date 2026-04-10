package com.genymobile.scrcpy.video;

public class VideoConstraints {
    private final int maxSize;
    private final int alignment;

    public VideoConstraints(int maxSize, int alignment) {
        assert maxSize >= 0 : "Max size must not be negative";
        this.maxSize = maxSize;

        assert alignment > 0 : "Alignment must be positive";
        assert (alignment & (alignment - 1)) == 0 : "Alignment must be a power-of-two";
        this.alignment = alignment;
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
}
