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

    public Size constrain(int maxSize, int alignment) {
        assert maxSize >= 0 : "Max size may not be negative";
        assert alignment > 0 : "Alignment must be positive";
        assert (alignment & (alignment - 1)) == 0 : "Alignment must be a power-of-two";

        int alignedMaxSize = maxSize / alignment * alignment; // round to a multiple of alignment
        int w, h;

        if (maxSize > 0 && (width > alignedMaxSize || height > alignedMaxSize)) {
            if (width > height) {
                w = alignedMaxSize;
                h = round(height * alignedMaxSize / width, alignment);
            } else {
                w = round(width * alignedMaxSize / height, alignment);
                h = alignedMaxSize;
            }
        } else {
            w = round(width, alignment);
            h = round(height, alignment);
        }

        assert maxSize == 0 || w <= maxSize : "The width cannot exceed maxSize if maxSize is aligned";
        assert maxSize == 0 || h <= maxSize : "The height cannot exceed maxSize if maxSize is aligned";

        return new Size(w, h);
    }

    private static int round(int value, int alignment) {
        return (value + (alignment / 2)) / alignment * alignment;
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
