package com.genymobile.scrcpy;

import android.graphics.Rect;

public final class ScreenInfo {
    private final Rect contentRect; // device size, possibly cropped
    private final Size videoSize;
    private final int rotation;

    public ScreenInfo(Rect contentRect, Size videoSize, int rotation) {
        this.contentRect = contentRect;
        this.videoSize = videoSize;
        this.rotation = rotation;
    }

    public Rect getContentRect() {
        return contentRect;
    }

    public Size getVideoSize() {
        return videoSize;
    }

    public int getRotation() {
        return rotation;
    }

    public ScreenInfo withRotation(int newRotation) {
        if ((rotation + newRotation) % 2 == 0) { // 180s don't need flipping
            return this;
        }
        return new ScreenInfo(Device.flipRect(contentRect), videoSize.rotate(), newRotation);
    }
}
