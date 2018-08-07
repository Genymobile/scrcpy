package com.genymobile.scrcpy;

import android.graphics.Rect;

public final class ScreenInfo {
    private final Size deviceSize;
    private final Size videoSize;
    private final Rect crop;

    private ScreenInfo(Size deviceSize, Size videoSize, Rect crop, boolean rotated) {
        this.deviceSize = deviceSize;
        this.videoSize = videoSize;
        this.crop = crop;
    }

    public static ScreenInfo create(Size deviceSize, int maxSize, Rect crop, boolean rotated) {
        Size inputSize;
        if (crop == null) {
            inputSize = deviceSize;
        } else {
            if (rotated) {
                // the crop (provided by the user) is expressed in the natural orientation
                // the device size (provided by the system) already takes the rotation into account
                crop = rotateCrop(crop, deviceSize);
            }
            inputSize = new Size(crop.width(), crop.height());
        }
        Size videoSize = computeVideoSize(inputSize, maxSize);
        return new ScreenInfo(deviceSize, videoSize, crop, rotated);
    }

    @SuppressWarnings("checkstyle:MagicNumber")
    private static Size computeVideoSize(Size inputSize, int maxSize) {
        // Compute the video size and the padding of the content inside this video.
        // Principle:
        // - scale down the great side of the screen to maxSize (if necessary);
        // - scale down the other side so that the aspect ratio is preserved;
        // - round this value to the nearest multiple of 8 (H.264 only accepts multiples of 8)
        int w = inputSize.getWidth() & ~7; // in case it's not a multiple of 8
        int h = inputSize.getHeight() & ~7;
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

    public Size getDeviceSize() {
        return deviceSize;
    }

    public Size getVideoSize() {
        return videoSize;
    }

    public Rect getCrop() {
        return crop;
    }

    public Rect getContentRect() {
        Rect crop = getCrop();
        return crop != null ? crop : deviceSize.toRect();
    }

    private static Rect rotateCrop(Rect crop, Size rotatedSize) {
        int w = rotatedSize.getHeight(); // the size is already rotated
        return new Rect(crop.top, w - crop.right, crop.bottom, w - crop.left);
    }
}
