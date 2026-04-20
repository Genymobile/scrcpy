package com.genymobile.scrcpy.util;

import com.fasterxml.jackson.databind.ObjectMapper;

import java.io.IOException;
import java.util.Map;

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

    public static String jsonToArgs(String json) throws IOException {
        // Parse the JSON string into a map
        ObjectMapper mapper = new ObjectMapper();
        @SuppressWarnings("unchecked")
        Map<String, Object> map = mapper.readValue(json, Map.class);

        // Convert to key=value arguments
        StringBuilder sb = new StringBuilder();
        for (Map.Entry<String, Object> entry : map.entrySet()) {
            if (sb.length() > 0) {
                sb.append(' ');
            }
            sb.append(entry.getKey()).append('=').append(entry.getValue());
        }

        return sb.toString();
    }
}
