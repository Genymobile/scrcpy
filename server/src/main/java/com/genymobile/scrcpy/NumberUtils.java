package com.genymobile.scrcpy;

public class NumberUtils {

    public static int tryParseInt(final String str) {
        return tryParseInt(str, 0);
    }

    public static int tryParseInt(final String str, int defaultValue) {
        try {
            return Integer.parseInt(str);
        } catch (NumberFormatException e) {
            return defaultValue;
        }
    }
}
