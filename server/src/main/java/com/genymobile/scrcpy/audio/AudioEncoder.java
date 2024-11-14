package com.genymobile.scrcpy.audio;

import com.genymobile.scrcpy.AndroidVersions;
import com.genymobile.scrcpy.AsyncProcessor;
import com.genymobile.scrcpy.Options;
import com.genymobile.scrcpy.device.ConfigurationException;
import com.genymobile.scrcpy.device.Streamer;
import com.genymobile.scrcpy.util.Codec;
import com.genymobile.scrcpy.util.CodecOption;
import com.genymobile.scrcpy.util.CodecUtils;
import com.genymobile.scrcpy.util.IO;
import com.genymobile.scrcpy.util.Ln;
import com.genymobile.scrcpy.util.LogUtils;

import android.annotation.TargetApi;
import android.media.MediaCodec;
import android.media.MediaFormat;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.List;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;

public final class AudioEncoder implements AsyncProcessor {

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

    private static final int SAMPLE_RATE = AudioConfig.SAMPLE_RATE;
    private static final int CHANNELS = AudioConfig.CHANNELS;

    private final AudioCapture capture;
    private final Streamer streamer;
    private final int bitRate;
    private final List<CodecOption> codecOptions;
    private final String encoderName;

    // Capacity of 64 is in practice "infinite" (it is limited by the number of available MediaCodec buffers, typically 4).
    // So many pending tasks would lead to an unacceptable delay anyway.
    private final BlockingQueue<InputTask> inputTasks = new ArrayBlockingQueue<>(64);
    private final BlockingQueue<OutputTask> outputTasks = new ArrayBlockingQueue<>(64);

    private Thread thread;
    private HandlerThread mediaCodecThread;

    private Thread inputThread;
    private Thread outputThread;

    private boolean ended;

    public AudioEncoder(AudioCapture capture, Streamer streamer, Options options) {
        this.capture = capture;
        this.streamer = streamer;
        this.bitRate = options.getAudioBitRate();
        this.codecOptions = options.getAudioCodecOptions();
        this.encoderName = options.getAudioEncoder();
    }

    private static MediaFormat createFormat(String mimeType, int bitRate, List<CodecOption> codecOptions) {
        MediaFormat format = new MediaFormat();
        format.setString(MediaFormat.KEY_MIME, mimeType);
        format.setInteger(MediaFormat.KEY_BIT_RATE, bitRate);
        format.setInteger(MediaFormat.KEY_CHANNEL_COUNT, CHANNELS);
        format.setInteger(MediaFormat.KEY_SAMPLE_RATE, SAMPLE_RATE);

        if (codecOptions != null) {
            for (CodecOption option : codecOptions) {
                String key = option.getKey();
                Object value = option.getValue();
                CodecUtils.setCodecOption(format, key, value);
                Ln.d("Audio codec option set: " + key + " (" + value.getClass().getSimpleName() + ") = " + value);
            }
        }

        return format;
    }

    @TargetApi(AndroidVersions.API_24_ANDROID_7_0)
    private void inputThread(MediaCodec mediaCodec, AudioCapture capture) throws IOException, InterruptedException {
        final MediaCodec.BufferInfo bufferInfo = new MediaCodec.BufferInfo();

        while (!Thread.currentThread().isInterrupted()) {
            InputTask task = inputTasks.take();
            ByteBuffer buffer = mediaCodec.getInputBuffer(task.index);
            int r = capture.read(buffer, bufferInfo);
            if (r <= 0) {
                throw new IOException("Could not read audio: " + r);
            }

            mediaCodec.queueInputBuffer(task.index, bufferInfo.offset, bufferInfo.size, bufferInfo.presentationTimeUs, bufferInfo.flags);
        }
    }

    private void outputThread(MediaCodec mediaCodec) throws IOException, InterruptedException {
        streamer.writeAudioHeader();

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

    @Override
    public void start(TerminationListener listener) {
        thread = new Thread(() -> {
            boolean fatalError = false;
            try {
                encode();
            } catch (ConfigurationException e) {
                // Do not print stack trace, a user-friendly error-message has already been logged
                fatalError = true;
            } catch (AudioCaptureException e) {
                // Do not print stack trace, a user-friendly error-message has already been logged
            } catch (IOException e) {
                Ln.e("Audio encoding error", e);
                fatalError = true;
            } finally {
                Ln.d("Audio encoder stopped");
                listener.onTerminated(fatalError);
            }
        }, "audio-encoder");
        thread.start();
    }

    @Override
    public void stop() {
        if (thread != null) {
            // Just wake up the blocking wait from the thread, so that it properly releases all its resources and terminates
            end();
        }
    }

    @Override
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

    @TargetApi(AndroidVersions.API_23_ANDROID_6_0)
    private void encode() throws IOException, ConfigurationException, AudioCaptureException {
        if (Build.VERSION.SDK_INT < AndroidVersions.API_30_ANDROID_11) {
            Ln.w("Audio disabled: it is not supported before Android 11");
            streamer.writeDisableStream(false);
            return;
        }

        MediaCodec mediaCodec = null;

        boolean mediaCodecStarted = false;
        try {
            capture.checkCompatibility(); // throws an AudioCaptureException on error

            Codec codec = streamer.getCodec();
            mediaCodec = createMediaCodec(codec, encoderName);

            mediaCodecThread = new HandlerThread("media-codec");
            mediaCodecThread.start();

            MediaFormat format = createFormat(codec.getMimeType(), bitRate, codecOptions);
            mediaCodec.setCallback(new EncoderCallback(), new Handler(mediaCodecThread.getLooper()));
            mediaCodec.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);

            capture.start();

            final MediaCodec mediaCodecRef = mediaCodec;
            inputThread = new Thread(() -> {
                try {
                    inputThread(mediaCodecRef, capture);
                } catch (IOException | InterruptedException e) {
                    Ln.e("Audio capture error", e);
                } finally {
                    end();
                }
            }, "audio-in");

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
            }, "audio-out");

            mediaCodec.start();
            mediaCodecStarted = true;
            inputThread.start();
            outputThread.start();

            waitEnded();
        } catch (ConfigurationException e) {
            // Notify the error to make scrcpy exit
            streamer.writeDisableStream(true);
            throw e;
        } catch (Throwable e) {
            // Notify the client that the audio could not be captured
            streamer.writeDisableStream(false);
            throw e;
        } finally {
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
            if (capture != null) {
                capture.stop();
            }
        }
    }

    private static MediaCodec createMediaCodec(Codec codec, String encoderName) throws IOException, ConfigurationException {
        if (encoderName != null) {
            Ln.d("Creating audio encoder by name: '" + encoderName + "'");
            try {
                MediaCodec mediaCodec = MediaCodec.createByCodecName(encoderName);
                String mimeType = Codec.getMimeType(mediaCodec);
                if (!codec.getMimeType().equals(mimeType)) {
                    Ln.e("Audio encoder type for \"" + encoderName + "\" (" + mimeType + ") does not match codec type (" + codec.getMimeType() + ")");
                    throw new ConfigurationException("Incorrect encoder type: " + encoderName);
                }
                return mediaCodec;
            } catch (IllegalArgumentException e) {
                Ln.e("Audio encoder '" + encoderName + "' for " + codec.getName() + " not found\n" + LogUtils.buildAudioEncoderListMessage());
                throw new ConfigurationException("Unknown encoder: " + encoderName);
            } catch (IOException e) {
                Ln.e("Could not create audio encoder '" + encoderName + "' for " + codec.getName() + "\n" + LogUtils.buildAudioEncoderListMessage());
                throw e;
            }
        }

        try {
            MediaCodec mediaCodec = MediaCodec.createEncoderByType(codec.getMimeType());
            Ln.d("Using audio encoder: '" + mediaCodec.getName() + "'");
            return mediaCodec;
        } catch (IOException | IllegalArgumentException e) {
            Ln.e("Could not create default audio encoder for " + codec.getName() + "\n" + LogUtils.buildAudioEncoderListMessage());
            throw e;
        }
    }

    private final class EncoderCallback extends MediaCodec.Callback {
        @TargetApi(AndroidVersions.API_24_ANDROID_7_0)
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
