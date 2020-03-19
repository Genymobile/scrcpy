package com.genymobile.scrcpy;

public final class DisplayInfo {
    private final int displayId;
    private final Size size;
    private final int rotation;
    private final int layerStack;

    public DisplayInfo(int displayId, Size size, int rotation, int layerStack) {
        this.displayId = displayId;
        this.size = size;
        this.rotation = rotation;
        this.layerStack = layerStack;
    }

    public int getDisplayId() {
        return displayId;
    }

    public Size getSize() {
        return size;
    }

    public int getRotation() {
        return rotation;
    }

    public int getLayerStack() {
        return layerStack;
    }
}

