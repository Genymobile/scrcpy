package com.genymobile.scrcpy.audio;

import com.genymobile.scrcpy.AndroidVersions;
import com.genymobile.scrcpy.util.Ln;

import android.annotation.TargetApi;
import android.media.AudioRecord;
import android.media.AudioTimestamp;
import android.media.MediaCodec;

import java.nio.ByteBuffer;

public class AudioRecordReader {

    private static final long ONE_SAMPLE_US =
            (1000000 + AudioConfig.SAMPLE_RATE - 1) / AudioConfig.SAMPLE_RATE; // 1 sample in microseconds (used for fixing PTS)

    private final AudioRecord recorder;

    private final AudioTimestamp timestamp = new AudioTimestamp();
    private long previousRecorderTimestamp = -1;
    private long previousPts = 0;
    private long nextPts = 0;

    public AudioRecordReader(AudioRecord recorder) {
        this.recorder = recorder;
    }

    @TargetApi(AndroidVersions.API_24_ANDROID_7_0)
    public int read(ByteBuffer outDirectBuffer, MediaCodec.BufferInfo outBufferInfo) {
        int r = recorder.read(outDirectBuffer, AudioConfig.MAX_READ_SIZE);
        if (r <= 0) {
            return r;
        }

        long pts;

        int ret = recorder.getTimestamp(timestamp, AudioTimestamp.TIMEBASE_MONOTONIC);
        if (ret == AudioRecord.SUCCESS && timestamp.nanoTime != previousRecorderTimestamp) {
            pts = timestamp.nanoTime / 1000;
            previousRecorderTimestamp = timestamp.nanoTime;
        } else {
            if (nextPts == 0) {
                Ln.w("Could not get initial audio timestamp");
                nextPts = System.nanoTime() / 1000;
            }
            // compute from previous timestamp and packet size
            pts = nextPts;
        }

        long durationUs = r * 1000000L / (AudioConfig.CHANNELS * AudioConfig.BYTES_PER_SAMPLE * AudioConfig.SAMPLE_RATE);
        nextPts = pts + durationUs;

        if (previousPts != 0 && pts < previousPts + ONE_SAMPLE_US) {
            // Audio PTS may come from two sources:
            //  - recorder.getTimestamp() if the call works;
            //  - an estimation from the previous PTS and the packet size as a fallback.
            //
            // Therefore, the property that PTS are monotonically increasing is no guaranteed in corner cases, so enforce it.
            pts = previousPts + ONE_SAMPLE_US;
        }
        previousPts = pts;

        outBufferInfo.set(0, r, pts, 0);
        return r;
    }
}
