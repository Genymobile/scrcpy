package com.genymobile.scrcpy.audio;

import android.media.MediaRecorder;

public enum AudioSource {
    OUTPUT("output", MediaRecorder.AudioSource.REMOTE_SUBMIX),
    MIC("mic", MediaRecorder.AudioSource.MIC),
    PLAYBACK("playback", -1);

    private final String name;
    private final int directAudioSource;

    AudioSource(String name, int directAudioSource) {
        this.name = name;
        this.directAudioSource = directAudioSource;
    }

    public boolean isDirect() {
        return this != PLAYBACK;
    }

    public int getDirectAudioSource() {
        return directAudioSource;
    }

    public static AudioSource findByName(String name) {
        for (AudioSource audioSource : AudioSource.values()) {
            if (name.equals(audioSource.name)) {
                return audioSource;
            }
        }

        return null;
    }
}
