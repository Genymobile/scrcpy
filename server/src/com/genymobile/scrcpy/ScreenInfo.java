package com.genymobile.scrcpy;

public class ScreenInfo {
    private final int width;
    private final int height;
    private int rotation;

    public ScreenInfo(int width, int height, int rotation) {
        this.width = width;
        this.height = height;
        this.rotation = rotation;
    }

    public ScreenInfo withRotation(int rotation) {
        return new ScreenInfo(width, height, rotation);
    }

    public int getLogicalWidth() {
        return (rotation & 1) == 0 ? width : height;
    }

    public int getLogicalHeight() {
        return (rotation & 1) == 0 ? height : width;
    }
}

