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
        if (newRotation == rotation) {
            return this;
        }
        // true if changed between portrait and landscape
        boolean orientationChanged = (rotation + newRotation) % 2 != 0;
        Rect newContentRect;
        Size newVideoSize;
        if (orientationChanged) {
            newContentRect = Device.flipRect(contentRect);
            newVideoSize = videoSize.rotate();
        } else {
            newContentRect = contentRect;
            newVideoSize = videoSize;
        }
        return new ScreenInfo(newContentRect, newVideoSize, newRotation);
    }
}
