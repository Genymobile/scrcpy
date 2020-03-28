package com.genymobile.scrcpy;

import android.graphics.Rect;

public final class ScreenInfo {
    /**
     * Device (physical) size, possibly cropped
     */
    private final Rect contentRect; // device size, possibly cropped

    /**
     * Video size, possibly smaller than the device size, already taking the device rotation and crop into account.
     * <p>
     * However, it does not include the locked video orientation.
     */
    private final Size unlockedVideoSize;

    /**
     * Device rotation, related to the natural device orientation (0, 1, 2 or 3)
     */
    private final int deviceRotation;

    /**
     * The locked video orientation (-1: disabled, 0: normal, 1: 90° CCW, 2: 180°, 3: 90° CW)
     */
    private final int lockedVideoOrientation;

    public ScreenInfo(Rect contentRect, Size unlockedVideoSize, int deviceRotation, int lockedVideoOrientation) {
        this.contentRect = contentRect;
        this.unlockedVideoSize = unlockedVideoSize;
        this.deviceRotation = deviceRotation;
        this.lockedVideoOrientation = lockedVideoOrientation;
    }

    public Rect getContentRect() {
        return contentRect;
    }

    /**
     * Return the video size as if locked video orientation was not set.
     *
     * @return the unlocked video size
     */
    public Size getUnlockedVideoSize() {
        return unlockedVideoSize;
    }

    /**
     * Return the actual video size if locked video orientation is set.
     *
     * @return the actual video size
     */
    public Size getVideoSize() {
        if (getVideoRotation() % 2 == 0) {
            return unlockedVideoSize;
        }

        return unlockedVideoSize.rotate();
    }

    public int getDeviceRotation() {
        return deviceRotation;
    }

    public ScreenInfo withDeviceRotation(int newDeviceRotation) {
        if (newDeviceRotation == deviceRotation) {
            return this;
        }
        // true if changed between portrait and landscape
        boolean orientationChanged = (deviceRotation + newDeviceRotation) % 2 != 0;
        Rect newContentRect;
        Size newUnlockedVideoSize;
        if (orientationChanged) {
            newContentRect = flipRect(contentRect);
            newUnlockedVideoSize = unlockedVideoSize.rotate();
        } else {
            newContentRect = contentRect;
            newUnlockedVideoSize = unlockedVideoSize;
        }
        return new ScreenInfo(newContentRect, newUnlockedVideoSize, newDeviceRotation, lockedVideoOrientation);
    }

    public static ScreenInfo computeScreenInfo(DisplayInfo displayInfo, Rect crop, int maxSize, int lockedVideoOrientation) {
        int rotation = displayInfo.getRotation();
        Size deviceSize = displayInfo.getSize();
        Rect contentRect = new Rect(0, 0, deviceSize.getWidth(), deviceSize.getHeight());
        if (crop != null) {
            if (rotation % 2 != 0) { // 180s preserve dimensions
                // the crop (provided by the user) is expressed in the natural orientation
                crop = flipRect(crop);
            }
            if (!contentRect.intersect(crop)) {
                // intersect() changes contentRect so that it is intersected with crop
                Ln.w("Crop rectangle (" + formatCrop(crop) + ") does not intersect device screen (" + formatCrop(deviceSize.toRect()) + ")");
                contentRect = new Rect(); // empty
            }
        }

        Size videoSize = computeVideoSize(contentRect.width(), contentRect.height(), maxSize);
        return new ScreenInfo(contentRect, videoSize, rotation, lockedVideoOrientation);
    }

    private static String formatCrop(Rect rect) {
        return rect.width() + ":" + rect.height() + ":" + rect.left + ":" + rect.top;
    }

    private static Size computeVideoSize(int w, int h, int maxSize) {
        // Compute the video size and the padding of the content inside this video.
        // Principle:
        // - scale down the great side of the screen to maxSize (if necessary);
        // - scale down the other side so that the aspect ratio is preserved;
        // - round this value to the nearest multiple of 8 (H.264 only accepts multiples of 8)
        w &= ~7; // in case it's not a multiple of 8
        h &= ~7;
        if (maxSize > 0) {
            if (BuildConfig.DEBUG && maxSize % 8 != 0) {
                throw new AssertionError("Max size must be a multiple of 8");
            }
            boolean portrait = h > w;
            int major = portrait ? h : w;
            int minor = portrait ? w : h;
            if (major > maxSize) {
                int minorExact = minor * maxSize / major;
                // +4 to round the value to the nearest multiple of 8
                minor = (minorExact + 4) & ~7;
                major = maxSize;
            }
            w = portrait ? minor : major;
            h = portrait ? major : minor;
        }
        return new Size(w, h);
    }

    private static Rect flipRect(Rect crop) {
        return new Rect(crop.top, crop.left, crop.bottom, crop.right);
    }

    /**
     * Return the rotation to apply to the device rotation to get the requested locked video orientation
     *
     * @return the rotation offset
     */
    public int getVideoRotation() {
        if (lockedVideoOrientation == -1) {
            // no offset
            return 0;
        }
        return (deviceRotation + 4 - lockedVideoOrientation) % 4;
    }

    /**
     * Return the rotation to apply to the requested locked video orientation to get the device rotation
     *
     * @return the (reverse) rotation offset
     */
    public int getReverseVideoRotation() {
        if (lockedVideoOrientation == -1) {
            // no offset
            return 0;
        }
        return (lockedVideoOrientation + 4 - deviceRotation) % 4;
    }
}
