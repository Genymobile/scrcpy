package com.genymobile.scrcpy.audio;

import android.annotation.SuppressLint;
import android.media.MediaRecorder;

@SuppressLint("InlinedApi")
public enum AudioSource {
    OUTPUT("output", MediaRecorder.AudioSource.REMOTE_SUBMIX),
    MIC("mic", MediaRecorder.AudioSource.MIC),
    PLAYBACK("playback", -1),
    MIC_UNPROCESSED("mic-unprocessed", MediaRecorder.AudioSource.UNPROCESSED),
    MIC_CAMCORDER("mic-camcorder", MediaRecorder.AudioSource.CAMCORDER),
    MIC_VOICE_RECOGNITION("mic-voice-recognition", MediaRecorder.AudioSource.VOICE_RECOGNITION),
    MIC_VOICE_COMMUNICATION("mic-voice-communication", MediaRecorder.AudioSource.VOICE_COMMUNICATION),
    VOICE_CALL("voice-call", MediaRecorder.AudioSource.VOICE_CALL),
    VOICE_CALL_UPLINK("voice-call-uplink", MediaRecorder.AudioSource.VOICE_UPLINK),
    VOICE_CALL_DOWNLINK("voice-call-downlink", MediaRecorder.AudioSource.VOICE_DOWNLINK),
    VOICE_PERFORMANCE("voice-performance", MediaRecorder.AudioSource.VOICE_PERFORMANCE);

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
