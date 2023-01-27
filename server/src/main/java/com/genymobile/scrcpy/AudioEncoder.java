package com.genymobile.scrcpy;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.os.Build;

import java.io.IOException;

public final class AudioEncoder {

    private static final int SAMPLE_RATE = 48000;
    private static final int CHANNELS = 2;

    private Thread thread;

    private static AudioFormat createAudioFormat() {
        AudioFormat.Builder builder = new AudioFormat.Builder();
        builder.setEncoding(AudioFormat.ENCODING_PCM_16BIT);
        builder.setSampleRate(SAMPLE_RATE);
        builder.setChannelMask(CHANNELS == 2 ? AudioFormat.CHANNEL_IN_STEREO : AudioFormat.CHANNEL_IN_MONO);
        return builder.build();
    }

    @TargetApi(Build.VERSION_CODES.M)
    @SuppressLint({"WrongConstant", "MissingPermission"})
    private static AudioRecord createAudioRecord() {
        AudioRecord.Builder builder = new AudioRecord.Builder();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            // On older APIs, Workarounds.fillAppInfo() must be called beforehand
            builder.setContext(FakeContext.get());
        }
        builder.setAudioSource(MediaRecorder.AudioSource.REMOTE_SUBMIX);
        builder.setAudioFormat(createAudioFormat());
        builder.setBufferSizeInBytes(1024 * 1024);
        return builder.build();
    }

    public void start() {
        AudioRecord recorder = createAudioRecord();

        thread = new Thread(() -> {
            recorder.startRecording();
            try {
                int BUFFER_MS = 15; // do not buffer more than BUFFER_MS milliseconds
                byte[] buf = new byte[SAMPLE_RATE * CHANNELS * BUFFER_MS / 1000];
                while (!Thread.currentThread().isInterrupted()) {
                    int r = recorder.read(buf, 0, buf.length);
                    if (r > 0) {
                        Ln.i("Audio captured: " + r + " bytes");
                    } else {
                        Ln.e("Audio capture error: " + r);
                    }
                }
            } finally {
                recorder.stop();
            }
        });
        thread.start();
    }

    public void stop() {
        if (thread != null) {
            thread.interrupt();
        }
    }

    public void join() throws InterruptedException {
        if (thread != null) {
            thread.join();
        }
    }
}
