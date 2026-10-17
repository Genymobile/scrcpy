package com.genymobile.scrcpy.control;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Inverse of the validated Android direct-input orientation, not a second desktop calibration. */
public final class BooxMapping {
    final int rotation;
    final int width;
    final int height;

    private BooxMapping(int rotation, int width, int height) {
        this.rotation = rotation;
        this.width = width;
        this.height = height;
    }

    /** Logical display dimensions must agree; InputReader orientation is not DisplayInfo.rotation. */
    public void requireDisplaySize(int displayWidth, int displayHeight) {
        if (width != displayWidth || height != displayHeight) {
            throw new IllegalArgumentException("BOOX input viewport " + width + "x" + height
                    + " differs from captured display " + displayWidth + "x" + displayHeight + "; reconnect");
        }
    }

    private static Matcher unique(String text, String expression) {
        Pattern pattern = Pattern.compile(expression, Pattern.MULTILINE);
        Matcher match = pattern.matcher(text);
        if (!match.find()) {
            throw new IllegalArgumentException("Missing or unsupported BOOX input mapping: " + expression);
        }
        if (pattern.matcher(text).find(match.end())) {
            throw new IllegalArgumentException("Ambiguous BOOX input mapping");
        }
        return match;
    }

    private static int number(String text, String key, String suffix) {
        return Integer.parseInt(unique(text, "^      " + Pattern.quote(key) + ": (\\d+)" + suffix + "$").group(1));
    }

    public static BooxMapping parse(String text, String path, int minX, int maxX, int minY, int maxY) {
        unique(text, "^Input Reader State:$");
        unique(text, "^Input Dispatcher State:$");
        int readerStart = text.indexOf("Input Reader State:");
        int readerEnd = text.indexOf("Input Dispatcher State:");
        if (readerStart < 0 || readerEnd <= readerStart) {
            throw new IllegalArgumentException("Missing InputReader metadata");
        }
        String hub = text.substring(0, readerStart);
        String id = null;
        String name = null;
        for (String block : hub.split("(?m)(?=^    -?\\d+: )")) {
            if (Pattern.compile("(?m)^      Path: " + Pattern.quote(path) + "$").matcher(block).find()) {
                Matcher device = unique(block, "^    (\\d+): (onyx_emp[^\n]*Wacom[^\n]*)$");
                if (id != null) {
                    throw new IllegalArgumentException("Ambiguous digitizer path");
                }
                id = device.group(1);
                name = device.group(2);
            }
        }
        if (id == null) {
            throw new IllegalArgumentException("Digitizer not found in EventHub");
        }
        String reader = text.substring(readerStart, readerEnd);
        String selected = null;
        for (String block : reader.split("(?m)(?=^  Device )")) {
            if (block.startsWith("  Device " + id + ": " + name + "\n")) {
                if (selected != null) {
                    throw new IllegalArgumentException("Ambiguous InputReader device");
                }
                selected = block;
            }
        }
        if (selected == null) {
            throw new IllegalArgumentException("Digitizer not found in InputReader");
        }
        unique(selected, "^    Touch Input Mapper \\(mode - direct\\):$");
        unique(selected, "^        DeviceType: touchScreen$");
        unique(selected, "^        OrientationAware: true$");
        String[] affine = {"X scale: 1.000", "X ymix: 0.000", "X offset: 0.000",
            "Y xmix: 0.000", "Y scale: 1.000", "Y offset: 0.000"};
        unique(selected, "^      Affine Transformation:\n        " + Pattern.quote(String.join("\n        ", affine)) + "$");
        unique(selected, "^        X: min=" + minX + ", max=" + maxX + ", flat=0, fuzz=0, resolution=0$");
        unique(selected, "^        Y: min=" + minY + ", max=" + maxY + ", flat=0, fuzz=0, resolution=0$");
        Matcher viewport = unique(selected, "^      Viewport INTERNAL: displayId=0, uniqueId=[^,]+, port=\\d+, orientation=([0-3]), "
                + "logicalFrame=\\[0, 0, (\\d+), (\\d+)\\], physicalFrame=\\[0, 0, \\2, \\3\\], deviceSize=\\[\\2, \\3\\]$");
        int orientation = Integer.parseInt(viewport.group(1));
        int width = Integer.parseInt(viewport.group(2));
        int height = Integer.parseInt(viewport.group(3));
        int surfaceWidth = number(selected, "SurfaceWidth", "px");
        int surfaceHeight = number(selected, "SurfaceHeight", "px");
        if (width < 2 || height < 2 || number(selected, "SurfaceOrientation", "") != orientation
                || number(selected, "SurfaceLeft", "") != 0 || number(selected, "SurfaceTop", "") != 0
                || number(selected, "PhysicalLeft", "") != 0 || number(selected, "PhysicalTop", "") != 0
                || surfaceWidth != (orientation % 2 == 0 ? width : height)
                || surfaceHeight != (orientation % 2 == 0 ? height : width)
                || number(selected, "PhysicalWidth", "px") != surfaceWidth
                || number(selected, "PhysicalHeight", "px") != surfaceHeight) {
            throw new IllegalArgumentException("Unsupported BOOX viewport geometry");
        }
        return new BooxMapping((4 - orientation) % 4, width, height);
    }
}
