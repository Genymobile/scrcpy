package com.genymobile.scrcpy;

public final class Binary {
    private Binary() {
        // not instantiable
    }

    public static int toUnsigned(short value) {
        return value & 0xffff;
    }

    public static int toUnsigned(byte value) {
        return value & 0xff;
    }
}
