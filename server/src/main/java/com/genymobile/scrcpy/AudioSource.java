package com.genymobile.scrcpy;

import android.media.MediaRecorder;

public enum AudioSource {
    OUTPUT("output", MediaRecorder.AudioSource.REMOTE_SUBMIX),
    MIC("mic", MediaRecorder.AudioSource.MIC);

    private final String name;
    private final int value;

    AudioSource(String name, int value) {
        this.name = name;
        this.value = value;
    }

    int value() {
        return value;
    }

    static AudioSource findByName(String name) {
        for (AudioSource audioSource : AudioSource.values()) {
            if (name.equals(audioSource.name)) {
                return audioSource;
            }
        }

        return null;
    }
}
