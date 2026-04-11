package com.genymobile.scrcpy.video;

public class VideoConstraints {
    private final int alignment;

    public VideoConstraints(int alignment) {
        assert alignment > 0 : "Alignment must be positive";
        assert (alignment & (alignment - 1)) == 0 : "Alignment must be a power-of-two";
        this.alignment = alignment;
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
