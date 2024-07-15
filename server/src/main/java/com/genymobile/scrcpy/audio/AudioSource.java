package com.genymobile.scrcpy.audio;

public enum AudioSource {
    OUTPUT("output"),
    MIC("mic");

    private final String name;

    AudioSource(String name) {
        this.name = name;
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
