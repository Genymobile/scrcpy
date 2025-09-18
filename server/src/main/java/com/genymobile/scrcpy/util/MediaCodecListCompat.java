package com.genymobile.scrcpy.util;

import com.genymobile.scrcpy.AndroidVersions;

import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.os.Build;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;

import java.util.Arrays;

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
        if (Build.VERSION.SDK_INT >= AndroidVersions.API_21_ANDROID_5_0) {
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

    @SuppressWarnings("deprecation")
    private static final class Backport extends MediaCodecListCompat {
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
        public MediaCodecInfo[] getCodecInfos() {
            return Arrays.copyOf(mCodecInfos, mCodecInfos.length);
        }
    }

    @RequiresApi(AndroidVersions.API_21_ANDROID_5_0)
    private static final class Platform extends MediaCodecListCompat {
        private final MediaCodecList mDelegate;

        Platform() {
            this.mDelegate = new MediaCodecList(MediaCodecList.REGULAR_CODECS);
        }

        @NonNull
        @Override
        MediaCodecInfo[] getCodecInfos() {
            return mDelegate.getCodecInfos();
        }
    }
}
