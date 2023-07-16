package com.genymobile.scrcpy;

import android.media.MediaRecorder;

public enum VideoSource {
    DISPLAY("display"),
    CAMERA("camera");

    private final String name;

    VideoSource(String name) {
        this.name = name;
    }

    static VideoSource findByName(String name) {
        for (VideoSource videoSource : VideoSource.values()) {
            if (name.equals(videoSource.name)) {
                return videoSource;
            }
        }

        return null;
    }
}
