package com.genymobile.scrcpy;

import android.graphics.Rect;

public final class ScreenInfo {
    private final Rect contentRect; // device size, possibly cropped
    private final Size videoSize;
    private final boolean rotated;
    private final int displayId;

    public ScreenInfo(int displayId, Rect contentRect, Size videoSize, boolean rotated) {
        this.displayId = displayId;
        this.contentRect = contentRect;
        this.videoSize = videoSize;
        this.rotated = rotated;
    }

    public int getDisplayId() {
        return displayId;
    }

    public Rect getContentRect() {
        return contentRect;
    }

    public Size getVideoSize() {
        return videoSize;
    }

    public ScreenInfo withRotation(int rotation) {
        boolean newRotated = (rotation & 1) != 0;
        if (rotated == newRotated) {
            return this;
        }
        return new ScreenInfo(displayId, Device.flipRect(contentRect), videoSize.rotate(), newRotated);
    }
}
