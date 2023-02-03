package com.genymobile.scrcpy;

import android.media.MediaFormat;

public enum VideoCodec {
    H264(0x68_32_36_34, "h264", MediaFormat.MIMETYPE_VIDEO_AVC),
    H265(0x68_32_36_35, "h265", MediaFormat.MIMETYPE_VIDEO_HEVC);

    private final int id; // 4-byte ASCII representation of the name
    private final String name;
    private final String mimeType;

    VideoCodec(int id, String name, String mimeType) {
        this.id = id;
        this.name = name;
        this.mimeType = mimeType;
    }

    public int getId() {
        return id;
    }

    public String getMimeType() {
        return mimeType;
    }

    public static VideoCodec findByName(String name) {
        for (VideoCodec codec : values()) {
            if (codec.name.equals(name)) {
                return codec;
            }
        }
        return null;
    }
}
