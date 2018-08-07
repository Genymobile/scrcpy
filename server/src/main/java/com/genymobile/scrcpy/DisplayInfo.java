package com.genymobile.scrcpy;

public final class DisplayInfo {
    private final Size size;
    private final int rotation;

    public DisplayInfo(Size size, int rotation) {
        this.size = size;
        this.rotation = rotation;
    }

    public Size getSize() {
        return size;
    }

    public int getRotation() {
        return rotation;
    }

    public DisplayInfo withRotation(int rotation) {
        if (rotation == this.rotation) {
            return this;
        }
        Size newSize = size;
        if ((rotation & 1) != (this.rotation & 1)) {
            newSize = size.rotate();
        }
        return new DisplayInfo(newSize, rotation);
    }
}

