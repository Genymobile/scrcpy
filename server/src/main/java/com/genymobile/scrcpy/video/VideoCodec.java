package com.genymobile.scrcpy.video;

import com.genymobile.scrcpy.util.Codec;

import android.annotation.SuppressLint;
import android.media.MediaFormat;

public enum VideoCodec implements Codec {
    H264(0x68_32_36_34, "h264", MediaFormat.MIMETYPE_VIDEO_AVC),
    H265(0x68_32_36_35, "h265", MediaFormat.MIMETYPE_VIDEO_HEVC),
    @SuppressLint("InlinedApi") // introduced in API 29
    AV1(0x00_61_76_31, "av1", MediaFormat.MIMETYPE_VIDEO_AV1),
    VP8(0x00_76_70_38, "vp8", MediaFormat.MIMETYPE_VIDEO_VP8),
    VP9(0x00_76_70_39, "vp9", MediaFormat.MIMETYPE_VIDEO_VP9);

    private final int id; // 4-byte ASCII representation of the name
    private final String name;
    private final String mimeType;

    VideoCodec(int id, String name, String mimeType) {
        this.id = id;
        this.name = name;
        this.mimeType = mimeType;
    }

    @Override
    public Type getType() {
        return Type.VIDEO;
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

    public static VideoCodec findByName(String name) {
        for (VideoCodec codec : values()) {
            if (codec.name.equals(name)) {
                return codec;
            }
        }
        return null;
    }
}
