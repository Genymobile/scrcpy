package com.genymobile.scrcpy;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.AudioTimestamp;
import android.media.MediaCodec;
import android.media.MediaFormat;
import android.media.MediaRecorder;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;

public final class AudioEncoder {

    private static class InputTask {
        private final int index;

        InputTask(int index) {
            this.index = index;
        }
    }

    private static class OutputTask {
        private final int index;
        private final MediaCodec.BufferInfo bufferInfo;

        OutputTask(int index, MediaCodec.BufferInfo bufferInfo) {
            this.index = index;
            this.bufferInfo = bufferInfo;
        }
    }

    private static final int SAMPLE_RATE = 48000;
    private static final int CHANNELS = 2;

    private static final int BUFFER_MS = 10; // milliseconds
    private static final int BUFFER_SIZE = SAMPLE_RATE * CHANNELS * BUFFER_MS / 1000;

    private final Streamer streamer;
    private final int bitRate;

    // Capacity of 64 is in practice "infinite" (it is limited by the number of available MediaCodec buffers, typically 4).
    // So many pending tasks would lead to an unacceptable delay anyway.
    private final BlockingQueue<InputTask> inputTasks = new ArrayBlockingQueue<>(64);
    private final BlockingQueue<OutputTask> outputTasks = new ArrayBlockingQueue<>(64);

    private Thread thread;
    private HandlerThread mediaCodecThread;

    private Thread inputThread;
    private Thread outputThread;

    private boolean ended;

    public AudioEncoder(Streamer streamer, int bitRate) {
        this.streamer = streamer;
        this.bitRate = bitRate;
    }

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

    private static MediaFormat createFormat(String mimeType, int bitRate) {
        MediaFormat format = new MediaFormat();
        format.setString(MediaFormat.KEY_MIME, mimeType);
        format.setInteger(MediaFormat.KEY_BIT_RATE, bitRate);
        format.setInteger(MediaFormat.KEY_CHANNEL_COUNT, CHANNELS);
        format.setInteger(MediaFormat.KEY_SAMPLE_RATE, SAMPLE_RATE);
        return format;
    }

    @TargetApi(Build.VERSION_CODES.N)
    private void inputThread(MediaCodec mediaCodec, AudioRecord recorder) throws IOException, InterruptedException {
        final AudioTimestamp timestamp = new AudioTimestamp();
        long previousPts = 0;
        long nextPts = 0;

        while (!Thread.currentThread().isInterrupted()) {
            InputTask task = inputTasks.take();
            ByteBuffer buffer = mediaCodec.getInputBuffer(task.index);
            int r = recorder.read(buffer, BUFFER_SIZE);
            if (r < 0) {
                throw new IOException("Could not read audio: " + r);
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

            long durationMs = r * 1000 / CHANNELS / SAMPLE_RATE;
            nextPts = pts + durationMs;

            if (previousPts != 0 && pts < previousPts) {
                // Audio PTS may come from two sources:
                //  - recorder.getTimestamp() if the call works;
                //  - an estimation from the previous PTS and the packet size as a fallback.
                //
                // Therefore, the property that PTS are monotonically increasing is no guaranteed in corner cases, so enforce it.
                pts = previousPts + 1;
            }

            mediaCodec.queueInputBuffer(task.index, 0, r, pts, 0);

            previousPts = pts;
        }
    }

    private void outputThread(MediaCodec mediaCodec) throws IOException, InterruptedException {
        streamer.writeHeader();

        while (!Thread.currentThread().isInterrupted()) {
            OutputTask task = outputTasks.take();
            ByteBuffer buffer = mediaCodec.getOutputBuffer(task.index);
            try {
                streamer.writePacket(buffer, task.bufferInfo);
            } finally {
                mediaCodec.releaseOutputBuffer(task.index, false);
            }
        }
    }

    public void start() {
        thread = new Thread(() -> {
            try {
                encode();
            } catch (IOException e) {
                Ln.e("Audio encoding error", e);
            } finally {
                Ln.d("Audio encoder stopped");
            }
        });
        thread.start();
    }

    public void stop() {
        if (thread != null) {
            // Just wake up the blocking wait from the thread, so that it properly releases all its resources and terminates
            end();
        }
    }

    public void join() throws InterruptedException {
        if (thread != null) {
            thread.join();
        }
    }

    private synchronized void end() {
        ended = true;
        notify();
    }

    private synchronized void waitEnded() {
        try {
            while (!ended) {
                wait();
            }
        } catch (InterruptedException e) {
            // ignore
        }
    }

    @TargetApi(Build.VERSION_CODES.M)
    public void encode() throws IOException {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Ln.w("Audio disabled: it is not supported before Android 11");
            streamer.writeDisableStream();
            return;
        }

        MediaCodec mediaCodec = null;
        AudioRecord recorder = null;

        boolean mediaCodecStarted = false;
        boolean recorderStarted = false;
        try {
            String mimeType = streamer.getCodec().getMimeType();
            mediaCodec = MediaCodec.createEncoderByType(mimeType); // may throw IOException
            recorder = createAudioRecord();

            mediaCodecThread = new HandlerThread("AudioEncoder");
            mediaCodecThread.start();

            MediaFormat format = createFormat(mimeType, bitRate);
            mediaCodec.setCallback(new EncoderCallback(), new Handler(mediaCodecThread.getLooper()));
            mediaCodec.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);

            recorder.startRecording();
            recorderStarted = true;

            final MediaCodec mediaCodecRef = mediaCodec;
            final AudioRecord recorderRef = recorder;
            inputThread = new Thread(() -> {
                try {
                    inputThread(mediaCodecRef, recorderRef);
                } catch (IOException | InterruptedException e) {
                    Ln.e("Audio capture error", e);
                } finally {
                    end();
                }
            });

            outputThread = new Thread(() -> {
                try {
                    outputThread(mediaCodecRef);
                } catch (InterruptedException e) {
                    // this is expected on close
                } catch (IOException e) {
                    // Broken pipe is expected on close, because the socket is closed by the client
                    if (!IO.isBrokenPipe(e)) {
                        Ln.e("Audio encoding error", e);
                    }
                } finally {
                    end();
                }
            });

            mediaCodec.start();
            mediaCodecStarted = true;
            inputThread.start();
            outputThread.start();

            waitEnded();
        } finally {
            if (!recorderStarted) {
                // Notify the client that the audio could not be captured
                streamer.writeDisableStream();
            }

            // Cleanup everything (either at the end or on error at any step of the initialization)
            if (mediaCodecThread != null) {
                Looper looper = mediaCodecThread.getLooper();
                if (looper != null) {
                    looper.quitSafely();
                }
            }
            if (inputThread != null) {
                inputThread.interrupt();
            }
            if (outputThread != null) {
                outputThread.interrupt();
            }

            try {
                if (mediaCodecThread != null) {
                    mediaCodecThread.join();
                }
                if (inputThread != null) {
                    inputThread.join();
                }
                if (outputThread != null) {
                    outputThread.join();
                }
            } catch (InterruptedException e) {
                // Should never happen
                throw new AssertionError(e);
            }

            if (mediaCodec != null) {
                if (mediaCodecStarted) {
                    mediaCodec.stop();
                }
                mediaCodec.release();
            }
            if (recorder != null) {
                if (recorderStarted) {
                    recorder.stop();
                }
                recorder.release();
            }
        }
    }

    private class EncoderCallback extends MediaCodec.Callback {
        @TargetApi(Build.VERSION_CODES.N)
        @Override
        public void onInputBufferAvailable(MediaCodec codec, int index) {
            try {
                inputTasks.put(new InputTask(index));
            } catch (InterruptedException e) {
                end();
            }
        }

        @Override
        public void onOutputBufferAvailable(MediaCodec codec, int index, MediaCodec.BufferInfo bufferInfo) {
            try {
                outputTasks.put(new OutputTask(index, bufferInfo));
            } catch (InterruptedException e) {
                end();
            }
        }

        @Override
        public void onError(MediaCodec codec, MediaCodec.CodecException e) {
            Ln.e("MediaCodec error", e);
            end();
        }

        @Override
        public void onOutputFormatChanged(MediaCodec codec, MediaFormat format) {
            // ignore
        }
    }
}
