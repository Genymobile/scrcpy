package com.genymobile.scrcpy;

import com.genymobile.scrcpy.wrappers.SurfaceControl;

import android.graphics.Rect;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.os.Build;
import android.os.IBinder;
import android.os.Looper;
import android.os.SystemClock;
import android.view.Surface;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

public class SurfaceEncoder implements AsyncProcessor {

    private static final int DEFAULT_I_FRAME_INTERVAL = 10; // seconds
    private static final int REPEAT_FRAME_DELAY_US = 100_000; // repeat after 100ms
    private static final String KEY_MAX_FPS_TO_ENCODER = "max-fps-to-encoder";

    // Keep the values in descending order
    private static final int[] MAX_SIZE_FALLBACK = {2560, 1920, 1600, 1280, 1024, 800};
    private static final int MAX_CONSECUTIVE_ERRORS = 3;

    private final SurfaceCapture capture;
    private final Streamer streamer;
    private final String encoderName;
    private final List<CodecOption> codecOptions;
    private final int videoBitRate;
    private final int maxFps;
    private final boolean downsizeOnError;

    private boolean firstFrameSent;
    private int consecutiveErrors;

    private Thread thread;
    private final AtomicBoolean stopped = new AtomicBoolean();

    public SurfaceEncoder(SurfaceCapture capture, Streamer streamer, int videoBitRate, int maxFps, List<CodecOption> codecOptions, String encoderName,
            boolean downsizeOnError) {
        this.capture = capture;
        this.streamer = streamer;
        this.videoBitRate = videoBitRate;
        this.maxFps = maxFps;
        this.codecOptions = codecOptions;
        this.encoderName = encoderName;
        this.downsizeOnError = downsizeOnError;
    }

    private void streamScreen() throws IOException, ConfigurationException {
        Codec codec = streamer.getCodec();
        MediaCodec mediaCodec = createMediaCodec(codec, encoderName);
        MediaFormat format = createFormat(codec.getMimeType(), videoBitRate, maxFps, codecOptions);

        capture.init();

        try {
            streamer.writeVideoHeader(capture.getSize());

            boolean alive;

            do {
                Size size = capture.getSize();
                format.setInteger(MediaFormat.KEY_WIDTH, size.getWidth());
                format.setInteger(MediaFormat.KEY_HEIGHT, size.getHeight());

                Surface surface = null;
                try {
                    mediaCodec.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
                    surface = mediaCodec.createInputSurface();

                    capture.setSurface(surface);

                    mediaCodec.start();

                    alive = encode(mediaCodec, streamer);
                    // do not call stop() on exception, it would trigger an IllegalStateException
                    mediaCodec.stop();
                } catch (IllegalStateException | IllegalArgumentException e) {
                    Ln.e("Encoding error: " + e.getClass().getName() + ": " + e.getMessage());
                    if (!prepareRetry(size)) {
                        throw e;
                    }
                    Ln.i("Retrying...");
                    alive = true;
                } finally {
                    mediaCodec.reset();
                    if (surface != null) {
                        surface.release();
                    }
                }
            } while (alive);
        } finally {
            mediaCodec.release();
            capture.release();
        }
    }

    private boolean prepareRetry(Size currentSize) {
        if (firstFrameSent) {
            ++consecutiveErrors;
            if (consecutiveErrors >= MAX_CONSECUTIVE_ERRORS) {
                // Definitively fail
                return false;
            }

            // Wait a bit to increase the probability that retrying will fix the problem
            SystemClock.sleep(50);
            return true;
        }

        if (!downsizeOnError) {
            // Must fail immediately
            return false;
        }

        // Downsizing on error is only enabled if an encoding failure occurs before the first frame (downsizing later could be surprising)

        int newMaxSize = chooseMaxSizeFallback(currentSize);
        if (newMaxSize == 0) {
            // Must definitively fail
            return false;
        }

        // Retry with a smaller device size
        Ln.i("Retrying with -m" + newMaxSize + "...");
        capture.setMaxSize(newMaxSize);
        return true;
    }

    private static int chooseMaxSizeFallback(Size failedSize) {
        int currentMaxSize = Math.max(failedSize.getWidth(), failedSize.getHeight());
        for (int value : MAX_SIZE_FALLBACK) {
            if (value < currentMaxSize) {
                // We found a smaller value to reduce the video size
                return value;
            }
        }
        // No fallback, fail definitively
        return 0;
    }

    private boolean encode(MediaCodec codec, Streamer streamer) throws IOException {
        boolean eof = false;
        boolean alive = true;
        MediaCodec.BufferInfo bufferInfo = new MediaCodec.BufferInfo();

        while (!capture.consumeReset() && !eof) {
            if (stopped.get()) {
                alive = false;
                break;
            }
            int outputBufferId = codec.dequeueOutputBuffer(bufferInfo, -1);
            try {
                if (capture.consumeReset()) {
                    // must restart encoding with new size
                    break;
                }

                eof = (bufferInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0;
                if (outputBufferId >= 0) {
                    ByteBuffer codecBuffer = codec.getOutputBuffer(outputBufferId);

                    boolean isConfig = (bufferInfo.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0;
                    if (!isConfig) {
                        // If this is not a config packet, then it contains a frame
                        firstFrameSent = true;
                        consecutiveErrors = 0;
                    }

                    streamer.writePacket(codecBuffer, bufferInfo);
                }
            } finally {
                if (outputBufferId >= 0) {
                    codec.releaseOutputBuffer(outputBufferId, false);
                }
            }
        }

        return !eof && alive;
    }

    private static MediaCodec createMediaCodec(Codec codec, String encoderName) throws IOException, ConfigurationException {
        if (encoderName != null) {
            Ln.d("Creating encoder by name: '" + encoderName + "'");
            try {
                return MediaCodec.createByCodecName(encoderName);
            } catch (IllegalArgumentException e) {
                Ln.e("Video encoder '" + encoderName + "' for " + codec.getName() + " not found\n" + LogUtils.buildVideoEncoderListMessage());
                throw new ConfigurationException("Unknown encoder: " + encoderName);
            } catch (IOException e) {
                Ln.e("Could not create video encoder '" + encoderName + "' for " + codec.getName() + "\n" + LogUtils.buildVideoEncoderListMessage());
                throw e;
            }
        }

        try {
            MediaCodec mediaCodec = MediaCodec.createEncoderByType(codec.getMimeType());
            Ln.d("Using video encoder: '" + mediaCodec.getName() + "'");
            return mediaCodec;
        } catch (IOException | IllegalArgumentException e) {
            Ln.e("Could not create default video encoder for " + codec.getName() + "\n" + LogUtils.buildVideoEncoderListMessage());
            throw e;
        }
    }

    private static MediaFormat createFormat(String videoMimeType, int bitRate, int maxFps, List<CodecOption> codecOptions) {
        MediaFormat format = new MediaFormat();
        format.setString(MediaFormat.KEY_MIME, videoMimeType);
        format.setInteger(MediaFormat.KEY_BIT_RATE, bitRate);
        // must be present to configure the encoder, but does not impact the actual frame rate, which is variable
        format.setInteger(MediaFormat.KEY_FRAME_RATE, 60);
        format.setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);
        format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, DEFAULT_I_FRAME_INTERVAL);
        // display the very first frame, and recover from bad quality when no new frames
        format.setLong(MediaFormat.KEY_REPEAT_PREVIOUS_FRAME_AFTER, REPEAT_FRAME_DELAY_US); // µs
        if (maxFps > 0) {
            // The key existed privately before Android 10:
            // <https://android.googlesource.com/platform/frameworks/base/+/625f0aad9f7a259b6881006ad8710adce57d1384%5E%21/>
            // <https://github.com/Genymobile/scrcpy/issues/488#issuecomment-567321437>
            format.setFloat(KEY_MAX_FPS_TO_ENCODER, maxFps);
        }

        if (codecOptions != null) {
            for (CodecOption option : codecOptions) {
                String key = option.getKey();
                Object value = option.getValue();
                CodecUtils.setCodecOption(format, key, value);
                Ln.d("Video codec option set: " + key + " (" + value.getClass().getSimpleName() + ") = " + value);
            }
        }

        return format;
    }

    private static IBinder createDisplay() {
        // Since Android 12 (preview), secure displays could not be created with shell permissions anymore.
        // On Android 12 preview, SDK_INT is still R (not S), but CODENAME is "S".
        boolean secure = Build.VERSION.SDK_INT < Build.VERSION_CODES.R || (Build.VERSION.SDK_INT == Build.VERSION_CODES.R && !"S".equals(
                Build.VERSION.CODENAME));
        return SurfaceControl.createDisplay("scrcpy", secure);
    }

    private static void setDisplaySurface(IBinder display, Surface surface, int orientation, Rect deviceRect, Rect displayRect, int layerStack) {
        SurfaceControl.openTransaction();
        try {
            SurfaceControl.setDisplaySurface(display, surface);
            SurfaceControl.setDisplayProjection(display, orientation, deviceRect, displayRect);
            SurfaceControl.setDisplayLayerStack(display, layerStack);
        } finally {
            SurfaceControl.closeTransaction();
        }
    }

    @Override
    public void start(TerminationListener listener) {
        thread = new Thread(() -> {
            // Some devices (Meizu) deadlock if the video encoding thread has no Looper
            // <https://github.com/Genymobile/scrcpy/issues/4143>
            Looper.prepare();

            try {
                streamScreen();
            } catch (ConfigurationException e) {
                // Do not print stack trace, a user-friendly error-message has already been logged
            } catch (IOException e) {
                // Broken pipe is expected on close, because the socket is closed by the client
                if (!IO.isBrokenPipe(e)) {
                    Ln.e("Video encoding error", e);
                }
            } finally {
                Ln.d("Screen streaming stopped");
                listener.onTerminated(true);
            }
        }, "video");
        thread.start();
    }

    @Override
    public void stop() {
        if (thread != null) {
            stopped.set(true);
        }
    }

    @Override
    public void join() throws InterruptedException {
        if (thread != null) {
            thread.join();
        }
    }
}
