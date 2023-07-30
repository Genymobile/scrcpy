package com.genymobile.scrcpy;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.AudioTimestamp;
import android.media.MediaCodec;
import android.os.Build;
import android.os.SystemClock;

import java.nio.ByteBuffer;

public final class AudioCapture {

    public static final int SAMPLE_RATE = 48000;
    public static final int CHANNEL_CONFIG = AudioFormat.CHANNEL_IN_STEREO;
    public static final int CHANNELS = 2;
    public static final int CHANNEL_MASK = AudioFormat.CHANNEL_IN_LEFT | AudioFormat.CHANNEL_IN_RIGHT;
    public static final int ENCODING = AudioFormat.ENCODING_PCM_16BIT;
    public static final int BYTES_PER_SAMPLE = 2;

    private final int audioSource;

    private AudioRecord recorder;

    private final AudioTimestamp timestamp = new AudioTimestamp();
    private long previousPts = 0;
    private long nextPts = 0;

    public AudioCapture(AudioSource audioSource) {
        this.audioSource = audioSource.value();
    }

    public static int millisToBytes(int millis) {
        return SAMPLE_RATE * CHANNELS * BYTES_PER_SAMPLE * millis / 1000;
    }

    private static AudioFormat createAudioFormat() {
        AudioFormat.Builder builder = new AudioFormat.Builder();
        builder.setEncoding(ENCODING);
        builder.setSampleRate(SAMPLE_RATE);
        builder.setChannelMask(CHANNEL_CONFIG);
        return builder.build();
    }

    @TargetApi(Build.VERSION_CODES.M)
    @SuppressLint({"WrongConstant", "MissingPermission"})
    private static AudioRecord createAudioRecord(int audioSource) {
        AudioRecord.Builder builder = new AudioRecord.Builder();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            // On older APIs, Workarounds.fillAppInfo() must be called beforehand
            builder.setContext(FakeContext.get());
        }
        builder.setAudioSource(audioSource);
        builder.setAudioFormat(createAudioFormat());
        int minBufferSize = AudioRecord.getMinBufferSize(SAMPLE_RATE, CHANNEL_CONFIG, ENCODING);
        // This buffer size does not impact latency
        builder.setBufferSizeInBytes(8 * minBufferSize);
        return builder.build();
    }

    private void tryStartRecording(int attempts, int delayMs) throws CaptureForegroundException {
        while (attempts-- > 0) {
            // Wait for activity to start
            SystemClock.sleep(delayMs);
            try {
                startRecording();
                return; // it worked
            } catch (UnsupportedOperationException e) {
                if (attempts == 0) {
                    Ln.e("Failed to start audio capture");
                    Ln.e("On Android 11, audio capture must be started in the foreground, make sure that the device is unlocked when starting "
                            + "scrcpy.");
                    throw new CaptureForegroundException();
                } else {
                    Ln.d("Failed to start audio capture, retrying...");
                }
            }
        }
    }

    private void startRecording() {
        try {
            recorder = createAudioRecord(audioSource);
        } catch (NullPointerException e) {
            // Creating an AudioRecord using an AudioRecord.Builder does not work on Vivo phones:
            // - <https://github.com/Genymobile/scrcpy/issues/3805>
            // - <https://github.com/Genymobile/scrcpy/pull/3862>
            recorder = Workarounds.createAudioRecord(audioSource, SAMPLE_RATE, CHANNEL_CONFIG, CHANNELS, CHANNEL_MASK, ENCODING);
        }
        recorder.startRecording();
    }

    public void start() throws CaptureForegroundException {
        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.R) {
            Workarounds.startForegroundWorkaround();
            try {
                tryStartRecording(5, 100);
            } finally {
                Workarounds.stopForegroundWorkaround();
            }
        } else {
            startRecording();
        }
    }

    public void stop() {
        if (recorder != null) {
            // Will call .stop() if necessary, without throwing an IllegalStateException
            recorder.release();
        }
    }

    @TargetApi(Build.VERSION_CODES.N)
    public int read(ByteBuffer directBuffer, int size, MediaCodec.BufferInfo outBufferInfo) {
        int r = recorder.read(directBuffer, size);
        if (r <= 0) {
            return r;
        }

        long pts;

        int ret = recorder.getTimestamp(timestamp, AudioTimestamp.TIMEBASE_MONOTONIC);
        if (ret == AudioRecord.SUCCESS) {
            pts = timestamp.nanoTime / 1000;
        } else {
            if (nextPts == 0) {
                Ln.w("Could not get any audio timestamp");
            }
            // compute from previous timestamp and packet size
            pts = nextPts;
        }

        long durationUs = r * 1000000 / (CHANNELS * BYTES_PER_SAMPLE * SAMPLE_RATE);
        nextPts = pts + durationUs;

        if (previousPts != 0 && pts < previousPts) {
            // Audio PTS may come from two sources:
            //  - recorder.getTimestamp() if the call works;
            //  - an estimation from the previous PTS and the packet size as a fallback.
            //
            // Therefore, the property that PTS are monotonically increasing is no guaranteed in corner cases, so enforce it.
            pts = previousPts + 1;
        }
        previousPts = pts;

        outBufferInfo.set(0, r, pts, 0);
        return r;
    }
}
