package com.genymobile.scrcpy.model;

import com.genymobile.scrcpy.video.VideoConstraints;

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

    public Size constrain(VideoConstraints constraints) {
        int maxSize = constraints.getMaxSize();
        int alignment = constraints.getAlignment();

        assert maxSize >= 0 : "Max size may not be negative";
        assert alignment > 0 : "Alignment must be positive";
        assert (alignment & (alignment - 1)) == 0 : "Alignment must be a power-of-two";

        boolean portrait = width < height;
        Size maxCodecSize = portrait ? constraints.getMaxCodecPortraitSize() : constraints.getMaxCodecLandscapeSize();

        int maxWidth = maxCodecSize.width;
        int maxHeight = maxCodecSize.height;
        if (maxSize > 0) {
            if (maxWidth > maxSize) {
                maxWidth = maxSize;
            }
            if (maxHeight > maxSize) {
                maxHeight = maxSize;
            }
        }
        maxWidth = maxWidth / alignment * alignment;
        maxHeight = maxHeight / alignment * alignment;

        int w, h;
        if (width > maxWidth || height > maxHeight) {
            if (width * maxHeight > height * maxWidth) {
                w = maxWidth;
                h = round(height * maxWidth / width, alignment);
            } else {
                w = round(width * maxHeight / height, alignment);
                h = maxHeight;
            }
        } else {
            w = round(width, alignment);
            h = round(height, alignment);
        }

        assert w <= maxWidth : "The width cannot exceed maxWidth";
        assert h <= maxHeight : "The height cannot exceed maxHeight";

        // Minimum codec size must be respected (regardless of requested maxSize)
        int minCodecSize = constraints.getMinCodecSize();
        if (w < minCodecSize) {
            w = minCodecSize;
        }
        if (h < minCodecSize) {
            h = minCodecSize;
        }

        return new Size(w, h);
    }

    public Size align(int alignment) {
        int w = width / alignment * alignment;
        int h = height / alignment * alignment;
        if (w == width && h == height) {
            return this;
        }
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
