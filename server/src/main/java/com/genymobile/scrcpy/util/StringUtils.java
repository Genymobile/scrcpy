package com.genymobile.scrcpy.util;

public final class StringUtils {
    private StringUtils() {
        // not instantiable
    }

    public static int getUtf8TruncationIndex(byte[] utf8, int maxLength) {
        int len = utf8.length;
        if (len <= maxLength) {
            return len;
        }
        len = maxLength;
        // see UTF-8 encoding <https://en.wikipedia.org/wiki/UTF-8#Description>
        while ((utf8[len] & 0x80) != 0 && (utf8[len] & 0xc0) != 0xc0) {
            // the next byte is not the start of a new UTF-8 codepoint
            // so if we would cut there, the character would be truncated
            len--;
        }
        return len;
    }
}
