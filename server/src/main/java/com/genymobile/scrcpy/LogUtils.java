package com.genymobile.scrcpy;

import com.genymobile.scrcpy.wrappers.ServiceManager;

import java.util.List;

public final class LogUtils {

    private LogUtils() {
        // not instantiable
    }

    public static String buildVideoEncoderListMessage() {
        StringBuilder builder = new StringBuilder("List of video encoders:");
        List<CodecUtils.DeviceEncoder> videoEncoders = CodecUtils.listVideoEncoders();
        if (videoEncoders.isEmpty()) {
            builder.append("\n    (none)");
        } else {
            for (CodecUtils.DeviceEncoder encoder : videoEncoders) {
                builder.append("\n    --video-codec=").append(encoder.getCodec().getName());
                builder.append(" --video-encoder='").append(encoder.getInfo().getName()).append("'");
            }
        }
        return builder.toString();
    }

    public static String buildAudioEncoderListMessage() {
        StringBuilder builder = new StringBuilder("List of audio encoders:");
        List<CodecUtils.DeviceEncoder> audioEncoders = CodecUtils.listAudioEncoders();
        if (audioEncoders.isEmpty()) {
            builder.append("\n    (none)");
        } else {
            for (CodecUtils.DeviceEncoder encoder : audioEncoders) {
                builder.append("\n    --audio-codec=").append(encoder.getCodec().getName());
                builder.append(" --audio-encoder='").append(encoder.getInfo().getName()).append("'");
            }
        }
        return builder.toString();
    }

    public static String buildDisplayListMessage() {
        StringBuilder builder = new StringBuilder("List of displays:");
        int[] displayIds = ServiceManager.getDisplayManager().getDisplayIds();
        if (displayIds == null || displayIds.length == 0) {
            builder.append("\n    (none)");
        } else {
            for (int id : displayIds) {
                builder.append("\n    --display=").append(id);
            }
        }
        return builder.toString();
    }
}
