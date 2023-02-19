package com.genymobile.scrcpy;

import android.media.MediaCodecInfo;
import android.media.MediaCodecList;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public final class CodecUtils {

    private CodecUtils() {
        // not instantiable
    }

    public static String buildUnknownEncoderMessage(Codec codec, String encoderName) {
        StringBuilder msg = new StringBuilder("Encoder '").append(encoderName).append("' for ").append(codec.getName()).append(" not found");
        MediaCodecInfo[] encoders = listEncoders(codec.getMimeType());
        if (encoders != null && encoders.length > 0) {
            msg.append("\nTry to use one of the available encoders:");
            String codecOption = codec.getType() == Codec.Type.VIDEO ? "codec" : "audio-codec";
            for (MediaCodecInfo encoder : encoders) {
                msg.append("\n    scrcpy --").append(codecOption).append("=").append(codec.getName());
                msg.append(" --encoder='").append(encoder.getName()).append("'");
            }
        }
        return msg.toString();
    }

    private static MediaCodecInfo[] listEncoders(String mimeType) {
        List<MediaCodecInfo> result = new ArrayList<>();
        MediaCodecList list = new MediaCodecList(MediaCodecList.REGULAR_CODECS);
        for (MediaCodecInfo codecInfo : list.getCodecInfos()) {
            if (codecInfo.isEncoder() && Arrays.asList(codecInfo.getSupportedTypes()).contains(mimeType)) {
                result.add(codecInfo);
            }
        }
        return result.toArray(new MediaCodecInfo[result.size()]);
    }
}
