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
        return constrain(constraints, true);
    }

    public Size constrain(VideoConstraints constraints, boolean preserveAspectRatio) {
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
        maxWidth = align(maxWidth, alignment);
        maxHeight = align(maxHeight, alignment);

        int w, h;
        if (preserveAspectRatio) {
            if (width > maxWidth || height > maxHeight) {
                if (width * maxHeight > height * maxWidth) {
                    w = maxWidth;
                    h = height * maxWidth / width;
                } else {
                    w = width * maxHeight / height;
                    h = maxHeight;
                }
            } else {
                w = width;
                h = height;
            }
        } else {
            w = Math.min(width, maxWidth);
            h = Math.min(height, maxHeight);
        }

        w = align(w, alignment);
        h = align(h, alignment);

        assert w <= maxWidth : "The width cannot exceed maxWidth";
        assert h <= maxHeight : "The height cannot exceed maxHeight";

        // Minimum codec size must be respected (regardless of requested maxSize)
        int minCodecSize = alignUp(constraints.getMinCodecSize(), alignment);
        if (w < minCodecSize) {
            w = minCodecSize;
        }
        if (h < minCodecSize) {
            h = minCodecSize;
        }

        assert w % alignment == 0 : "The width must be a multiple of alignment";
        assert h % alignment == 0 : "The height must be a multiple of alignment";

        return new Size(w, h);
    }

    public Size align(int alignment) {
        int w = align(width, alignment);
        int h = align(height, alignment);
        if (w == width && h == height) {
            return this;
        }
        return new Size(w, h);
    }

    private static int align(int value, int alignment) {
        return value / alignment * alignment;
    }

    private static int alignUp(int value, int alignment) {
        return (value + alignment - 1) / alignment * alignment;
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
