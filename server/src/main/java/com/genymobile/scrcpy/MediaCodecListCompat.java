package com.genymobile.scrcpy;

import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import static android.media.MediaCodecList.REGULAR_CODECS;
import static android.os.Build.VERSION.SDK_INT;

/**
 * Version of {@link MediaCodecList} that backports {@link #getCodecInfos()} to Kitkat.
 */
abstract class MediaCodecListCompat {
    MediaCodecListCompat() {
    }

    /**
     * Returns the list of codecs that are suitable
     * for regular (buffer-to-buffer) decoding or encoding.
     */
    static MediaCodecListCompat regular() {
        if (SDK_INT >= 21) {
            return new Platform();
        } else {
            return new Backport();
        }
    }

    /**
     * Returns the list of {@link MediaCodecInfo} objects for the list
     * of media-codecs.
     */
    @NonNull
    abstract MediaCodecInfo[] getCodecInfos();

    @NonNull
    final MediaCodecInfo[] getEncoderInfosForType(@NonNull String mimeType) {
        List<MediaCodecInfo> result = new ArrayList<>();
        MediaCodecInfo[] codecInfos = MediaCodecListCompat.regular().getCodecInfos();
        for (MediaCodecInfo codecInfo : codecInfos) {
            if (codecInfo.isEncoder() && Arrays.asList(codecInfo.getSupportedTypes()).contains(mimeType)) {
                result.add(codecInfo);
            }
        }
        return result.toArray(new MediaCodecInfo[result.size()]);
    }

    @SuppressWarnings("deprecation")
    private static class Backport extends MediaCodecListCompat {
        private final MediaCodecInfo[] mCodecInfos;

        Backport() {
            final int size = MediaCodecList.getCodecCount();
            final MediaCodecInfo[] codecInfos = new MediaCodecInfo[size];
            for (int i = 0; i < size; i++) {
                codecInfos[i] = MediaCodecList.getCodecInfoAt(i);
            }
            this.mCodecInfos = codecInfos;
        }

        @NonNull
        @Override
        public final MediaCodecInfo[] getCodecInfos() {
            return Arrays.copyOf(mCodecInfos, mCodecInfos.length);
        }
    }

    @RequiresApi(21)
    private static class Platform extends MediaCodecListCompat {
        private final MediaCodecList mDelegate;

        Platform() {
            this.mDelegate = new MediaCodecList(REGULAR_CODECS);
        }

        @NonNull
        @Override
        MediaCodecInfo[] getCodecInfos() {
            return mDelegate.getCodecInfos();
        }
    }
}
