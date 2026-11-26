package com.genymobile.scrcpy.control;

import java.util.Arrays;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Strict parser for getevent -lp; never guess missing capabilities. */
public final class BooxDevice {
    final String path;
    final String name;
    final int pressure;
    final int maxDistance;
    final int minX;
    final int maxX;
    final int minY;
    final int maxY;
    final int maxPressure;
    final int x;
    final int y;
    final int distance;

    private BooxDevice(String path, String name, String block) {
        this.path = path;
        this.name = name;
        int[] ax = axis(block, "ABS_X");
        int[] ay = axis(block, "ABS_Y");
        int[] ap = axis(block, "ABS_PRESSURE");
        int[] ad = axis(block, "ABS_DISTANCE");
        x = ax[0];
        y = ay[0];
        minX = ax[1];
        maxX = ax[2];
        minY = ay[1];
        maxY = ay[2];
        maxPressure = ap[2];
        distance = ad[0];
        pressure = ap[0];
        maxDistance = ad[2];
        if (minX < 0 || minY < 0 || maxX <= minX || maxY <= minY || x < minX || x > maxX || y < minY || y > maxY
                || pressure < 0 || pressure > maxPressure || ap[1] != 0 || maxPressure <= 0
                || ad[1] != 0 || maxDistance <= 0 || distance < 0 || distance > maxDistance) {
            throw new IllegalArgumentException("BOOX digitizer must be idle with valid absolute ranges");
        }
    }

    private static int[] axis(String block, String name) {
        Matcher m = Pattern.compile("\\b" + name + "\\s*:\\s*value (-?\\d+), min (-?\\d+), max (-?\\d+)").matcher(block);
        if (!m.find()) {
            throw new IllegalArgumentException("Missing digitizer axis " + name);
        }
        return new int[] {Integer.parseInt(m.group(1)), Integer.parseInt(m.group(2)), Integer.parseInt(m.group(3))};
    }

    static final class NotIdleException extends IllegalArgumentException {
        private static final long serialVersionUID = 1L;
        final transient BooxDevice device;

        NotIdleException(BooxDevice device) {
            super("BOOX digitizer is not idle: pressure=" + device.pressure + ", distance=" + device.distance);
            this.device = device;
        }
    }

    String[] identity() {
        return new String[] {path, name, String.valueOf(minX), String.valueOf(maxX), String.valueOf(minY),
                String.valueOf(maxY), String.valueOf(maxPressure), String.valueOf(maxDistance)};
    }

    void requireIdentity(String[] expected) {
        if (!Arrays.equals(identity(), expected)) {
            throw new IllegalArgumentException("BOOX digitizer changed during writer handoff");
        }
    }

    public static BooxDevice parse(String text) {
        BooxDevice result = snapshot(text);
        if (!result.isIdle()) {
            throw new NotIdleException(result);
        }
        return result;
    }

    boolean isIdle() {
        return pressure == 0 && distance > 0;
    }

    boolean sameDevice(BooxDevice other) {
        return path.equals(other.path) && name.equals(other.name) && minX == other.minX && maxX == other.maxX
                && minY == other.minY && maxY == other.maxY && maxPressure == other.maxPressure && maxDistance == other.maxDistance;
    }

    static BooxDevice snapshot(String text) {
        BooxDevice result = null;
        for (String block : text.split("(?m)(?=^add device )")) {
            Matcher name = Pattern.compile("(?m)^\\s*name:\\s*\"(onyx_emp[^\"\\n]*Wacom[^\"\\n]*)\"\\s*$").matcher(block);
            if (!name.find()) {
                continue;
            }
            Matcher node = Pattern.compile("^add device \\d+: (/dev/input/event\\d+)\\s", Pattern.MULTILINE).matcher(block);
            if (!node.find() || result != null) {
                throw new IllegalArgumentException("Ambiguous BOOX digitizer");
            }
            result = new BooxDevice(node.group(1), name.group(1), block);
        }
        if (result == null) {
            throw new IllegalArgumentException("No supported onyx_emp Wacom digitizer");
        }
        return result;
    }
}
