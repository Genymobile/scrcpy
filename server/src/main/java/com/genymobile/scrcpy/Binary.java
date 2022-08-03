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

    /**
     * Convert unsigned 16-bit fixed-point to a float between 0 and 1
     *
     * @param value encoded value
     * @return Float value between 0 and 1
     */
    public static float u16FixedPointToFloat(short value) {
        int unsignedShort = Binary.toUnsigned(value);
        // 0x1p16f is 2^16 as float
        return unsignedShort == 0xffff ? 1f : (unsignedShort / 0x1p16f);
    }

    /**
     * Convert signed 16-bit fixed-point to a float between -1 and 1
     *
     * @param value encoded value
     * @return Float value between -1 and 1
     */
    public static float i16FixedPointToFloat(short value) {
        // 0x1p15f is 2^15 as float
        return value == 0x7fff ? 1f : (value / 0x1p15f);
    }
}
