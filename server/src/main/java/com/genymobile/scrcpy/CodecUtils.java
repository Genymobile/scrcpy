package com.genymobile.scrcpy;

import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.media.MediaFormat;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public final class CodecUtils {

    private CodecUtils() {
        // not instantiable
    }

    public static void setCodecOption(MediaFormat format, String key, Object value) {
        if (value instanceof Integer) {
            format.setInteger(key, (Integer) value);
        } else if (value instanceof Long) {
            format.setLong(key, (Long) value);
        } else if (value instanceof Float) {
            format.setFloat(key, (Float) value);
        } else if (value instanceof String) {
            format.setString(key, (String) value);
        }
    }
}
