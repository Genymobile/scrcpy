package com.genymobile.scrcpy.util;

import com.genymobile.scrcpy.audio.AudioCodec;
import com.genymobile.scrcpy.video.VideoCodec;

import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.media.MediaFormat;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public final class CodecUtils {

    public static final class DeviceEncoder {
        private final Codec codec;
        private final MediaCodecInfo info;

        DeviceEncoder(Codec codec, MediaCodecInfo info) {
            this.codec = codec;
            this.info = info;
        }

        public Codec getCodec() {
            return codec;
        }

        public MediaCodecInfo getInfo() {
            return info;
        }
    }

    private CodecUtils() {
        // not instantiable
    }

    public static void setCodecOption(MediaFormat format, String key, Object value) {
        if (value instanceof Integer) {
            format.setInteger(key, (Integer) value);
        } else if (value instanceof Long) {
            format.setLong(key, (Long) value);
        } else if (value instanceof Float) {
            format.setFloat(key, (Float) value);
        } else if (value instanceof String) {
            format.setString(key, (String) value);
        }
    }

    private static MediaCodecInfo[] getEncoders(MediaCodecList codecs, String mimeType) {
        List<MediaCodecInfo> result = new ArrayList<>();
        for (MediaCodecInfo codecInfo : codecs.getCodecInfos()) {
            if (codecInfo.isEncoder() && Arrays.asList(codecInfo.getSupportedTypes()).contains(mimeType)) {
                result.add(codecInfo);
            }
        }
        return result.toArray(new MediaCodecInfo[result.size()]);
    }

    public static List<DeviceEncoder> listVideoEncoders() {
        List<DeviceEncoder> encoders = new ArrayList<>();
        MediaCodecList codecs = new MediaCodecList(MediaCodecList.REGULAR_CODECS);
        for (VideoCodec codec : VideoCodec.values()) {
            for (MediaCodecInfo info : getEncoders(codecs, codec.getMimeType())) {
                encoders.add(new DeviceEncoder(codec, info));
            }
        }
        return encoders;
    }

    public static List<DeviceEncoder> listAudioEncoders() {
        List<DeviceEncoder> encoders = new ArrayList<>();
        MediaCodecList codecs = new MediaCodecList(MediaCodecList.REGULAR_CODECS);
        for (AudioCodec codec : AudioCodec.values()) {
            for (MediaCodecInfo info : getEncoders(codecs, codec.getMimeType())) {
                encoders.add(new DeviceEncoder(codec, info));
            }
        }
        return encoders;
    }
}
