package com.genymobile.scrcpy.control;

/** Host fixtures for strict discovery; no device access. */
public final class BooxDeviceTest {
    private static final String DEVICE = "add device 1: /dev/input/event7\n"
            + "  name: \"onyx_emp_Wacom I2C Digitizer\"\n"
            + " ABS_X : value 77, min 10, max 210, fuzz 0\n"
            + " ABS_Y : value 88, min 20, max 420, fuzz 0\n"
            + " ABS_PRESSURE : value 0, min 0, max 4095, fuzz 0\n"
            + " ABS_DISTANCE : value 48, min 0, max 255, fuzz 0\n";

    private static void reject(String text) {
        try {
            BooxDevice.parse(text);
            throw new AssertionError("Unsafe discovery accepted");
        } catch (IllegalArgumentException expected) {
            // Expected fail-closed configuration.
        }
    }

    public static void main(String[] args) {
        BooxDevice d = BooxDevice.parse(DEVICE);
        if (!"/dev/input/event7".equals(d.path) || d.maxPressure != 4095 || d.distance != 48 || d.x != 77) {
            throw new AssertionError("Discovery must use actual node and capabilities");
        }
        reject("");
        reject(DEVICE.replace("onyx_emp_Wacom", "generic_Wacom"));
        reject(DEVICE.replace("value 0, min 0, max 4095", "value 1, min 0, max 4095"));
        reject(DEVICE.replace("value 48", "value 0"));
        reject(DEVICE.replace("ABS_DISTANCE", "MISSING"));
        reject(DEVICE.replace("max 4095", "max 0"));
        reject(DEVICE + DEVICE.replace("event7", "event8"));
        reject(DEVICE.replace("max 210", "max 10"));
        System.out.println("BooxDevice: 9 discovery fixtures passed");
    }
}
