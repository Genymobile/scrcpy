package com.genymobile.scrcpy.video;

import com.genymobile.scrcpy.AndroidVersions;
import com.genymobile.scrcpy.AsyncProcessor;
import com.genymobile.scrcpy.Options;
import com.genymobile.scrcpy.device.Streamer;
import com.genymobile.scrcpy.model.Codec;
import com.genymobile.scrcpy.model.CodecOption;
import com.genymobile.scrcpy.model.ConfigurationException;
import com.genymobile.scrcpy.model.Size;
import com.genymobile.scrcpy.util.CodecUtils;
import com.genymobile.scrcpy.util.IO;
import com.genymobile.scrcpy.util.Ln;
import com.genymobile.scrcpy.util.LogUtils;

import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.os.Build;
import android.os.Looper;
import android.view.Surface;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

public class SurfaceEncoder implements AsyncProcessor {

    private static final int DEFAULT_I_FRAME_INTERVAL = 10; // seconds
    private static final int REPEAT_FRAME_DELAY_US = 100_000; // repeat after 100ms
    private static final String KEY_MAX_FPS_TO_ENCODER = "max-fps-to-encoder";

    private final SurfaceCapture capture;
    private final Streamer streamer;
    private final String encoderName;
    private final List<CodecOption> codecOptions;
    private final int videoBitRate;
    private final int maxSize;
    private final float maxFps;
    private final int minSizeAlignment;

    private Thread thread;
    private final AtomicBoolean stopped = new AtomicBoolean();

    private final CaptureControl captureControl = new CaptureControl();

    public SurfaceEncoder(SurfaceCapture capture, Streamer streamer, Options options) {
        this.capture = capture;
        this.streamer = streamer;
        this.videoBitRate = options.getVideoBitRate();
        this.maxSize = options.getMaxSize();
        this.maxFps = options.getMaxFps();
        this.codecOptions = options.getVideoCodecOptions();
        this.encoderName = options.getVideoEncoder();
        this.minSizeAlignment = options.getMinSizeAlignment();
    }

    private static VideoConstraints createVideoConstraints(int maxSize, int minSizeAlignment, MediaCodecInfo.VideoCapabilities caps) {
        assert caps != null;
        int alignment = Math.max(caps.getWidthAlignment(), caps.getHeightAlignment());
        Ln.d("Video codec size alignment requirement: " + alignment + "px");
        if (alignment < minSizeAlignment) {
            alignment = minSizeAlignment;
            Ln.d("Actual video size alignment: " + alignment + "px");
        }

        int maxLandscapeWidth = caps.getSupportedWidths().getUpper();
        int maxLandscapeHeight = caps.getSupportedHeightsFor(maxLandscapeWidth).getUpper();
        Size maxLandscapeSize = new Size(maxLandscapeWidth, maxLandscapeHeight);

        int maxPortraitHeight = caps.getSupportedHeights().getUpper();
        int maxPortraitWidth = caps.getSupportedWidthsFor(maxPortraitHeight).getUpper();
        Size maxPortraitSize = new Size(maxPortraitWidth, maxPortraitHeight);

        int minWidth = caps.getSupportedWidths().getLower();
        int minHeight = caps.getSupportedHeights().getLower();
        int minSize = Math.max(minWidth, minHeight);

        return new VideoConstraints(maxSize, alignment, maxLandscapeSize, maxPortraitSize, minSize);
    }

    private void streamCapture() throws IOException, ConfigurationException {
        Codec codec = streamer.getCodec();
        MediaCodec mediaCodec = createMediaCodec(codec, encoderName);
        MediaFormat format = createFormat(codec.getMimeType(), videoBitRate, maxFps, codecOptions);

        MediaCodecInfo.VideoCapabilities caps = mediaCodec.getCodecInfo().getCapabilitiesForType(codec.getMimeType()).getVideoCapabilities();
        assert caps != null; // caps cannot be null for a video codec
        VideoConstraints constraints = createVideoConstraints(maxSize, minSizeAlignment, caps);

        capture.init(captureControl, constraints);

        try {
            boolean alive;

            streamer.writeVideoHeader();

            do {
                int resetReasons = captureControl.consumeReset();
                if ((resetReasons & CaptureControl.RESET_REASON_TERMINATED) != 0) {
                    break;
                }
                capture.prepare();
                Size size = capture.getSize();

                format.setInteger(MediaFormat.KEY_WIDTH, size.getWidth());
                format.setInteger(MediaFormat.KEY_HEIGHT, size.getHeight());

                Surface surface = null;
                boolean mediaCodecStarted = false;
                boolean captureStarted = false;
                try {
                    mediaCodec.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
                    surface = mediaCodec.createInputSurface();

                    capture.start(surface);
                    captureStarted = true;

                    mediaCodec.start();
                    mediaCodecStarted = true;

                    // Set the MediaCodec instance to "interrupt" (by signaling an EOS) on reset
                    captureControl.setRunningMediaCodec(mediaCodec);

                    if (stopped.get()) {
                        alive = false;
                    } else {
                        if (!captureControl.isResetRequested()) {
                            // If a reset is requested during encode(), it will interrupt the encoding by an EOS
                            streamer.writeSessionMeta(size.getWidth(), size.getHeight(), false);
                            encode(mediaCodec, streamer);
                        }

                        // The capture might have been closed internally (for example if the camera is disconnected)
                        alive = !stopped.get() && !capture.isClosed();
                    }
                } finally {
                    captureControl.setRunningMediaCodec(null);
                    if (captureStarted) {
                        capture.stop();
                    }
                    if (mediaCodecStarted) {
                        try {
                            mediaCodec.stop();
                        } catch (IllegalStateException e) {
                            // ignore (just in case)
                        }
                    }
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

    private void encode(MediaCodec codec, Streamer streamer) throws IOException {
        MediaCodec.BufferInfo bufferInfo = new MediaCodec.BufferInfo();

        boolean eos;
        do {
            int outputBufferId = codec.dequeueOutputBuffer(bufferInfo, -1);
            try {
                eos = (bufferInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0;
                // On EOS, there might be data or not, depending on bufferInfo.size
                if (outputBufferId >= 0 && bufferInfo.size > 0) {
                    ByteBuffer codecBuffer = codec.getOutputBuffer(outputBufferId);
                    streamer.writePacket(codecBuffer, bufferInfo);
                }
            } finally {
                if (outputBufferId >= 0) {
                    codec.releaseOutputBuffer(outputBufferId, false);
                }
            }
        } while (!eos);
    }

    private static MediaCodec createMediaCodec(Codec codec, String encoderName) throws IOException, ConfigurationException {
        if (encoderName != null) {
            Ln.d("Creating encoder by name: '" + encoderName + "'");
            try {
                MediaCodec mediaCodec = MediaCodec.createByCodecName(encoderName);
                String mimeType = Codec.getMimeType(mediaCodec);
                if (!codec.getMimeType().equals(mimeType)) {
                    Ln.e("Video encoder type for \"" + encoderName + "\" (" + mimeType + ") does not match codec type (" + codec.getMimeType() + ")");
                    throw new ConfigurationException("Incorrect encoder type: " + encoderName);
                }
                return mediaCodec;
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

    private static MediaFormat createFormat(String videoMimeType, int bitRate, float maxFps, List<CodecOption> codecOptions) {
        MediaFormat format = new MediaFormat();
        format.setString(MediaFormat.KEY_MIME, videoMimeType);
        format.setInteger(MediaFormat.KEY_BIT_RATE, bitRate);
        // must be present to configure the encoder, but does not impact the actual frame rate, which is variable
        format.setInteger(MediaFormat.KEY_FRAME_RATE, 60);
        format.setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);
        if (Build.VERSION.SDK_INT >= AndroidVersions.API_24_ANDROID_7_0) {
            format.setInteger(MediaFormat.KEY_COLOR_RANGE, MediaFormat.COLOR_RANGE_LIMITED);
        }
        format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, DEFAULT_I_FRAME_INTERVAL);
        // display the very first frame, and recover from bad quality when no new frames
        format.setLong(MediaFormat.KEY_REPEAT_PREVIOUS_FRAME_AFTER, REPEAT_FRAME_DELAY_US); // µs
        if (Build.VERSION.SDK_INT >= AndroidVersions.API_23_ANDROID_6_0) {
            // real-time priority
            format.setInteger(MediaFormat.KEY_PRIORITY, 0);
        }
        if (Build.VERSION.SDK_INT >= AndroidVersions.API_26_ANDROID_8_0) {
            // output 1 frame as soon as 1 frame is queued
            format.setInteger(MediaFormat.KEY_LATENCY, 1);
        }
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

    @Override
    public void start(TerminationListener listener) {
        thread = new Thread(() -> {
            // Some devices (Meizu) deadlock if the video encoding thread has no Looper
            // <https://github.com/Genymobile/scrcpy/issues/4143>
            Looper.prepare();

            try {
                streamCapture();
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
            captureControl.reset(CaptureControl.RESET_REASON_TERMINATED);
        }
    }

    @Override
    public void join() throws InterruptedException {
        if (thread != null) {
            thread.join();
        }
    }
}
