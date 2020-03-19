package com.genymobile.scrcpy;

import android.graphics.Rect;

public final class ScreenInfo {
    private final Rect contentRect; // device size, possibly cropped
    private final Size videoSize;
    private final boolean rotated;
    private final int layerStack;

    public ScreenInfo(Rect contentRect, Size videoSize, boolean rotated, int layerStack) {
        this.contentRect = contentRect;
        this.videoSize = videoSize;
        this.rotated = rotated;
        this.layerStack = layerStack;
    }

    public Rect getContentRect() {
        return contentRect;
    }

    public Size getVideoSize() {
        return videoSize;
    }

    public int getLayerStack() {
        return layerStack;
    }

    public ScreenInfo withRotation(int rotation) {
        boolean newRotated = (rotation & 1) != 0;
        if (rotated == newRotated) {
            return this;
        }
        return new ScreenInfo(Device.flipRect(contentRect), videoSize.rotate(), newRotated, layerStack);
    }
}
