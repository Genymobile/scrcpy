package com.genymobile.scrcpy;

import android.media.MediaCodecInfo;

public class InvalidEncoderException extends RuntimeException {

    private final String name;
    private final MediaCodecInfo[] availableEncoders;

    public InvalidEncoderException(String name, MediaCodecInfo[] availableEncoders) {
        super("There is no encoder having name '" + name + '"');
        this.name = name;
        this.availableEncoders = availableEncoders;
    }

    public String getName() {
        return name;
    }

    public MediaCodecInfo[] getAvailableEncoders() {
        return availableEncoders;
    }
}
