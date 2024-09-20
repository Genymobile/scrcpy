package com.genymobile.scrcpy.util;

import android.media.MediaCodec;

public interface Codec {

    enum Type {
        VIDEO,
        AUDIO,
    }

    Type getType();

    int getId();

    String getName();

    String getMimeType();

    static String getMimeType(MediaCodec codec) {
        String[] types = codec.getCodecInfo().getSupportedTypes();
        return types.length > 0 ? types[0] : null;
    }
}
