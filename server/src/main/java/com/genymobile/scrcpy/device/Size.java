package com.genymobile.scrcpy.device;

import android.graphics.Rect;

import java.util.Objects;

public final class Size {
    private final int width;
    private final int height;

    public Size(int width, int height) {
        this.width = width;
        this.height = height;
    }

    public int getWidth() {
        return width;
    }

    public int getHeight() {
        return height;
    }

    public int getMax() {
        return Math.max(width, height);
    }

    public Size rotate() {
        return new Size(height, width);
    }

    public Size limit(int maxSize) {
        assert maxSize >= 0 : "Max size may not be negative";
        assert maxSize % 8 == 0 : "Max size must be a multiple of 8";

        if (maxSize == 0) {
            // No limit
            return this;
        }

        boolean portrait = height > width;
        int major = portrait ? height : width;
        if (major <= maxSize) {
            return this;
        }

        int minor = portrait ? width : height;

        int newMajor = maxSize;
        int newMinor = maxSize * minor / major;

        int w = portrait ? newMinor : newMajor;
        int h = portrait ? newMajor : newMinor;
        return new Size(w, h);
    }

    /**
     * Round both dimensions of this size to be a multiple of 8 (as required by many encoders).
     *
     * @return The current size rounded.
     */
    public Size round8() {
        if (isMultipleOf8()) {
            // Already a multiple of 8
            return this;
        }

        boolean portrait = height > width;
        int major = portrait ? height : width;
        int minor = portrait ? width : height;

        major &= ~7; // round down to not exceed the initial size
        minor = (minor + 4) & ~7; // round to the nearest to minimize aspect ratio distortion
        if (minor > major) {
            minor = major;
        }

        int w = portrait ? minor : major;
        int h = portrait ? major : minor;
        return new Size(w, h);
    }

    public boolean isMultipleOf8() {
        return (width & 7) == 0 && (height & 7) == 0;
    }

    public Rect toRect() {
        return new Rect(0, 0, width, height);
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (o == null || getClass() != o.getClass()) {
            return false;
        }
        Size size = (Size) o;
        return width == size.width && height == size.height;
    }

    @Override
    public int hashCode() {
        return Objects.hash(width, height);
    }

    @Override
    public String toString() {
        return width + "x" + height;
    }
}
