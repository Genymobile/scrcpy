package com.genymobile.scrcpy.control;

import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;

/** Pre-open handoff decisions; no Android runtime, processes or evdev access. */
public final class BooxPenPreparationTest {
    private static final String[] IDENTITY = {"/dev/input/event2", "onyx_emp_Wacom I2C Digitizer", "0", "20966", "0", "15725", "4095", "255"};

    private static String caps(int pressure, int distance) {
        return "add device 1: /dev/input/event2\n name: \"onyx_emp_Wacom I2C Digitizer\"\n"
                + " ABS_X : value 77, min 0, max 20966\n ABS_Y : value 88, min 0, max 15725\n"
                + " ABS_PRESSURE : value " + pressure + ", min 0, max 4095\n"
                + " ABS_DISTANCE : value " + distance + ", min 0, max 255\n";
    }

    private static void check(boolean value, String message) {
        if (!value) {
            throw new AssertionError(message);
        }
    }

    private static BooxPenPreparation prepare(String first, String second, String metadata, int rotation) throws Exception {
        int[] probes = {0};
        return BooxPenPreparation.prepare(() -> probes[0]++ == 0 ? first : second, () -> metadata,
                rotation, 1404, 1872, IDENTITY);
    }

    private static void reject(String first, String second, String metadata, int rotation) throws Exception {
        try {
            prepare(first, second, metadata, rotation);
            throw new AssertionError("Invalid or replaced device must be fatal, not BUSY");
        } catch (IllegalArgumentException expected) {
            // Only a valid non-idle snapshot of the exact bound device may be retried.
        }
    }

    public static void main(String[] args) throws Exception {
        String metadata = new String(Files.readAllBytes(Paths.get(args[0])), StandardCharsets.UTF_8);
        String idle = caps(0, 56);
        check(BooxPenPreparation.prepare(() -> caps(120, 0),
                () -> { throw new AssertionError("Busy first probe must not query mapping"); },
                -2, 1404, 1872, IDENTITY) == null, "Physical contact on child first probe must defer");
        check(prepare(idle, caps(120, 0), metadata, -2) == null, "Physical contact after mapping must defer");
        check(prepare(idle, caps(0, 0), metadata, -2) == null, "Zero distance after mapping must defer");
        BooxPenPreparation ready = prepare(idle, idle, metadata, -2);
        check(ready.device.isIdle() && ready.rotation == 3 && ready.mapping != null, "Strict idle automatic mapping preserved");
        check(prepare(idle, idle, "unused", 1).rotation == 1, "Explicit rotation preserved");
        reject(caps(120, 0).replace("event2", "event3"), idle, metadata, -2);
        reject(idle, caps(120, 0).replace("max 255", "max 256"), metadata, -2);
        reject(idle, idle.replace("Wacom I2C", "Wacom Other"), metadata, -2);
        reject(caps(5000, 0), idle, metadata, -2);
        reject(idle, caps(120, 0), "invalid mapping", -2);
        reject("malformed discovery", idle, metadata, -2);
        System.out.println("BooxPreparation: first/final busy, strict idle, mapping and fatal identity/metadata checks passed");
    }
}
