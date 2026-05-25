package com.genymobile.scrcpy.model;

import com.genymobile.scrcpy.util.BinarySearch;
import com.genymobile.scrcpy.util.Ln;
import com.genymobile.scrcpy.video.VideoConstraints;

import android.graphics.Rect;
import android.media.MediaCodecInfo;
import android.util.Range;

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
        MediaCodecInfo.VideoCapabilities caps = constraints.getEncoderCapabilities();

        assert maxSize >= 0 : "Max size may not be negative";
        assert alignment > 0 : "Alignment must be positive";
        assert (alignment & (alignment - 1)) == 0 : "Alignment must be a power-of-two";

        if (caps == null) {
            int w, h;
            if (maxSize > 0 && (width > maxSize || height > maxSize)) {
                if (preserveAspectRatio) {
                    if (width > height) {
                        w = maxSize;
                        h = height * maxSize / width;
                    } else {
                        w = width * maxSize / height;
                        h = maxSize;
                    }
                } else {
                    w = Math.min(width, maxSize);
                    h = Math.min(height, maxSize);
                }
            } else {
                // No constraints
                w = width;
                h = height;
            }

            w = Math.max(align(w, alignment), alignment);
            h = Math.max(align(h, alignment), alignment);

            return new Size(w, h);
        }

        boolean landscape = width >= height;
        int major = landscape ? width : height;
        int minor = landscape ? height : width;

        Range<Integer> majorRange = landscape ? caps.getSupportedWidths() : caps.getSupportedHeights();
        int minMajor = majorRange.getLower();
        int maxMajor = majorRange.getUpper();
        if (maxMajor > major) {
            // Never increase the size
            maxMajor = major;
        }
        if (maxSize > 0 && maxMajor > maxSize) {
            maxMajor = maxSize;
        }

        int minBlock = (minMajor + alignment - 1) / alignment;
        int maxBlock = maxMajor / alignment;

        int bestBlock = BinarySearch.findHighestTrue(
                minBlock, maxBlock, block -> {
                    int pixels = block * alignment;
                    int w = align(width * pixels / major, alignment);
                    int h = align(height * pixels / major, alignment);
                    return caps.isSizeSupported(w, h);
                });

        if (bestBlock < minBlock) {
            Ln.d("No matching size found, ignore encoder size validation");
            bestBlock = maxBlock;
        }

        int bestMajor = bestBlock * alignment;
        int bestMinor;

        if (preserveAspectRatio) {
            bestMinor = align(minor * bestMajor / major, alignment);
        } else {
            // The minor dimension can potentially be extended
            int maxMinor = landscape ? caps.getSupportedHeightsFor(bestMajor).getUpper() : caps.getSupportedWidthsFor(bestMajor).getUpper();
            if (maxMinor > minor) {
                maxMinor = minor;
            }
            if (maxSize > 0 && maxMinor > maxSize) {
                maxMinor = maxSize;
            }
            bestMinor = align(maxMinor, alignment);

            // The major dimension can potentially be extended
            maxMajor = landscape ? caps.getSupportedWidthsFor(bestMinor).getUpper() : caps.getSupportedHeightsFor(bestMinor).getUpper();
            if (maxMajor > major) {
                maxMajor = major;
            }
            if (maxSize > 0 && maxMajor > maxSize) {
                maxMajor = maxSize;
            }
            bestMajor = align(maxMajor, alignment);
        }

        minMajor = alignUp(minMajor, alignment);
        if (bestMajor < minMajor) {
            bestMajor = minMajor;
        }

        int minMinor = landscape ? caps.getSupportedHeights().getLower() : caps.getSupportedWidths().getLower();
        minMinor = alignUp(minMinor, alignment);
        if (bestMinor < minMinor) {
            bestMinor = minMinor;
        }

        int w = landscape ? bestMajor : bestMinor;
        int h = landscape ? bestMinor : bestMajor;

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
