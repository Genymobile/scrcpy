package com.genymobile.scrcpy.audio;

import com.genymobile.scrcpy.util.Codec;

import android.media.MediaFormat;

public enum AudioCodec implements Codec {
    OPUS(0x6f_70_75_73, "opus", MediaFormat.MIMETYPE_AUDIO_OPUS),
    AAC(0x00_61_61_63, "aac", MediaFormat.MIMETYPE_AUDIO_AAC),
    FLAC(0x66_6c_61_63, "flac", MediaFormat.MIMETYPE_AUDIO_FLAC),
    RAW(0x00_72_61_77, "raw", MediaFormat.MIMETYPE_AUDIO_RAW);

    private final int id; // 4-byte ASCII representation of the name
    private final String name;
    private final String mimeType;

    AudioCodec(int id, String name, String mimeType) {
        this.id = id;
        this.name = name;
        this.mimeType = mimeType;
    }

    @Override
    public Type getType() {
        return Type.AUDIO;
    }

    @Override
    public int getId() {
        return id;
    }

    @Override
    public String getName() {
        return name;
    }

    @Override
    public String getMimeType() {
        return mimeType;
    }

    public static AudioCodec findByName(String name) {
        for (AudioCodec codec : values()) {
            if (codec.name.equals(name)) {
                return codec;
            }
        }
        return null;
    }
}
