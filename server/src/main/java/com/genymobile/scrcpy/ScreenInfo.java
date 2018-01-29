package com.genymobile.scrcpy;

public final class ScreenInfo {
    private final Size deviceSize;
    private final Size videoSize;
    private final int padding; // padding inside the video stream, along the smallest dimension
    private final boolean rotated;

    public ScreenInfo(Size deviceSize, Size videoSize, int padding, boolean rotated) {
        this.deviceSize = deviceSize;
        this.videoSize = videoSize;
        this.padding = padding;
        this.rotated = rotated;
    }

    public Size getDeviceSize() {
        return deviceSize;
    }

    public Size getVideoSize() {
        return videoSize;
    }

    public int getXPadding() {
        return videoSize.getWidth() < videoSize.getHeight() ? padding : 0;
    }

    public int getYPadding() {
        return videoSize.getHeight() < videoSize.getWidth() ? padding : 0;
    }

    public ScreenInfo withRotation(int rotation) {
        boolean newRotated = (rotation & 1) != 0;
        if (rotated == newRotated) {
            return this;
        }
        return new ScreenInfo(deviceSize.rotate(), videoSize.rotate(), padding, newRotated);
    }
}
