package com.genymobile.scrcpy;

public final class DisplayInfo {
    private final Size size;
    private final int rotation;
    private final int layerStack;

    public DisplayInfo(Size size, int rotation, int layerStack) {
        this.size = size;
        this.rotation = rotation;
        this.layerStack = layerStack;
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

