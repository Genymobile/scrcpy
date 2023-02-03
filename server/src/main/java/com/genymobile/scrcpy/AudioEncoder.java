package com.genymobile.scrcpy;

import com.genymobile.scrcpy.wrappers.ServiceManager;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.ComponentName;
import android.content.Intent;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.AudioTimestamp;
import android.media.MediaCodec;
import android.media.MediaFormat;
import android.media.MediaRecorder;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.SystemClock;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.concurrent.Semaphore;
import java.util.concurrent.atomic.AtomicBoolean;

public final class AudioEncoder {

    private static final String MIMETYPE = MediaFormat.MIMETYPE_AUDIO_OPUS;
    private static final int SAMPLE_RATE = 48000;
    private static final int CHANNELS = 2;
    private static final int BIT_RATE = 128000;

    private static int BUFFER_MS = 15; // milliseconds
    private static final int BUFFER_SIZE = SAMPLE_RATE * CHANNELS * BUFFER_MS / 1000;

    private AudioRecord recorder;
    private MediaCodec mediaCodec;
    private HandlerThread thread;
    private final AtomicBoolean interrupted = new AtomicBoolean();
    private final Semaphore endSemaphore = new Semaphore(0); // blocks until encoding is ended

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

    private static MediaFormat createFormat() {
        MediaFormat format = new MediaFormat();
        format.setString(MediaFormat.KEY_MIME, MIMETYPE);
        format.setInteger(MediaFormat.KEY_BIT_RATE, BIT_RATE);
        format.setInteger(MediaFormat.KEY_CHANNEL_COUNT, CHANNELS);
        format.setInteger(MediaFormat.KEY_SAMPLE_RATE, SAMPLE_RATE);
        return format;
    }


    @TargetApi(Build.VERSION_CODES.M)
    public void start() throws IOException {
        mediaCodec = MediaCodec.createEncoderByType(MIMETYPE); // may throw IOException

        recorder = createAudioRecord();

        MediaFormat format = createFormat();
        mediaCodec.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);

        recorder.startRecording();

        thread = new HandlerThread("AudioEncoder");
        thread.start();

        class AudioEncoderCallbacks extends MediaCodec.Callback {

            private final AudioTimestamp timestamp = new AudioTimestamp();
            private long nextPts;
            private boolean eofSignaled;
            private boolean ended;

            private void notifyEnded() {
                assert !ended;
                ended = true;
                endSemaphore.release();
            }

            @TargetApi(Build.VERSION_CODES.N)
            @Override
            public void onInputBufferAvailable(MediaCodec codec, int index) {
                if (eofSignaled) {
                    return;
                }

                ByteBuffer inputBuffer = codec.getInputBuffer(index);
                int r = recorder.read(inputBuffer, BUFFER_SIZE);

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

                long durationMs = r * 1000 / CHANNELS / SAMPLE_RATE;
                nextPts = pts + durationMs;

                int flags = 0;
                if (interrupted.get()) {
                    flags = flags | MediaCodec.BUFFER_FLAG_END_OF_STREAM;
                    eofSignaled = true;
                }

                codec.queueInputBuffer(index, 0, r, pts, flags);
            }

            @Override
            public void onOutputBufferAvailable(MediaCodec codec, int index, MediaCodec.BufferInfo bufferInfo) {
                if (ended) {
                    return;
                }

                ByteBuffer codecBuffer = codec.getOutputBuffer(index);
                try {
                    boolean isConfig = (bufferInfo.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0;
                    long pts = bufferInfo.presentationTimeUs;
                    Ln.i("Audio packet: pts=" + pts  + " " + codecBuffer.remaining() + " bytes");
                } finally {
                    codec.releaseOutputBuffer(index, false);
                }

                boolean eof = (bufferInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0;
                if (eof) {
                    notifyEnded();
                }
            }

            @Override
            public void onError(MediaCodec codec, MediaCodec.CodecException e) {
                Ln.e("MediaCodec error", e);
                if (!ended) {
                    notifyEnded();
                }
            }

            @Override
            public void onOutputFormatChanged(MediaCodec codec, MediaFormat format) {
                // ignore
            }
        }

        mediaCodec.setCallback(new AudioEncoderCallbacks(), new Handler(thread.getLooper()));
        mediaCodec.start();
    }

    private void waitEnded() {
        try {
            endSemaphore.acquire();
        } catch (InterruptedException e) {
            // ignore
        }
    }

    public void stop() {
        Ln.i("==== STOP");
        if (thread != null) {
            interrupted.set(true);
            waitEnded();
            thread.interrupt();
            thread = null;
            mediaCodec.stop();
            mediaCodec.release();
            recorder.stop();
            Ln.i("==== STOPPED");
        }
    }
}
