package com.genymobile.scrcpy.video;

import com.genymobile.scrcpy.AndroidVersions;
import com.genymobile.scrcpy.AsyncProcessor;
import com.genymobile.scrcpy.Options;
import com.genymobile.scrcpy.device.ConfigurationException;
import com.genymobile.scrcpy.device.Size;
import com.genymobile.scrcpy.device.Streamer;
import com.genymobile.scrcpy.util.Codec;
import com.genymobile.scrcpy.util.CodecOption;
import com.genymobile.scrcpy.util.CodecUtils;
import com.genymobile.scrcpy.util.IO;
import com.genymobile.scrcpy.util.Ln;
import com.genymobile.scrcpy.util.LogUtils;

import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.os.Build;
import android.os.Looper;
import android.os.SystemClock;
import android.view.Surface;

import java.io.FileDescriptor;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.nio.ByteBuffer;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

public class SurfaceEncoder implements AsyncProcessor {

    /**
     * Callback to re-establish a socket connection when the client disconnects during rotation.
     * Returns the new FileDescriptor for the video stream, or null if reconnection is not supported.
     */
    public interface SocketReconnector {
        FileDescriptor reconnect() throws IOException;
    }

    private static final int DEFAULT_I_FRAME_INTERVAL = 10; // seconds
    private static final int REPEAT_FRAME_DELAY_US = 100_000; // repeat after 100ms
    private static final long DEFAULT_FRAME_REFRESH_INTERVAL_MS = 500; // force a frame every 500ms
    private static final long DEQUEUE_TIMEOUT_US = 1_000_000; // 1 second
    private static final int PREPARE_RETRY_DELAY_MS = 200;
    private static final int PREPARE_RETRY_COUNT = 3;
    private static final int POST_RESET_SETTLE_DELAY_MS = 100;
    private static final String KEY_MAX_FPS_TO_ENCODER = "max-fps-to-encoder";

    // Keep the values in descending order
    private static final int[] MAX_SIZE_FALLBACK = {2560, 1920, 1600, 1280, 1024, 800};
    private static final int MAX_CONSECUTIVE_ERRORS = 3;

    private final SurfaceCapture capture;
    private final Streamer streamer;
    private final String encoderName;
    private final List<CodecOption> codecOptions;
    private final int videoBitRate;
    private final float maxFps;
    private final boolean downsizeOnError;
    private final long frameRefreshIntervalMs;

    private SocketReconnector socketReconnector;

    private boolean firstFrameSent;
    private int consecutiveErrors;

    private Thread thread;
    private final AtomicBoolean stopped = new AtomicBoolean();

    private final CaptureReset reset = new CaptureReset();

    // Tracks total packets written in encode() for diagnostics
    private long totalPacketsWritten;

    public SurfaceEncoder(SurfaceCapture capture, Streamer streamer, Options options) {
        this.capture = capture;
        this.streamer = streamer;
        this.videoBitRate = options.getVideoBitRate();
        this.maxFps = options.getMaxFps();
        this.codecOptions = options.getVideoCodecOptions();
        this.encoderName = options.getVideoEncoder();
        this.downsizeOnError = options.getDownsizeOnError();
        int interval = options.getFrameRefreshIntervalMs();
        this.frameRefreshIntervalMs = interval > 0 ? interval : DEFAULT_FRAME_REFRESH_INTERVAL_MS;
    }

    public void setSocketReconnector(SocketReconnector reconnector) {
        this.socketReconnector = reconnector;
    }

    private static String getStackTraceString(Throwable e) {
        StringWriter sw = new StringWriter();
        e.printStackTrace(new PrintWriter(sw));
        return sw.toString();
    }

    /**
     * Retry capture.prepare() with delays after a reset.
     * <p/>
     * On some devices (e.g. Samsung), the display may be temporarily unavailable during rotation,
     * causing prepare() to fail with ConfigurationException. Retrying after a short delay allows
     * the display to become available again.
     */
    private void prepareWithRetry() throws ConfigurationException, IOException {
        for (int attempt = 1; attempt <= PREPARE_RETRY_COUNT; attempt++) {
            try {
                capture.prepare();
                return;
            } catch (ConfigurationException e) {
                if (attempt < PREPARE_RETRY_COUNT) {
                    Ln.w("Prepare failed after reset (attempt " + attempt + "/" + PREPARE_RETRY_COUNT + "), retrying...");
                    SystemClock.sleep(PREPARE_RETRY_DELAY_MS);
                } else {
                    throw e;
                }
            }
        }
    }

    /**
     * Attempt to reconnect the socket after a broken pipe during rotation.
     * Returns true if reconnection succeeded, false otherwise.
     */
    private boolean tryReconnect() {
        if (socketReconnector == null) {
            Ln.w("[DIAG] No socket reconnector configured, cannot reconnect");
            return false;
        }
        try {
            Ln.i("Client disconnected during rotation. Waiting for new client connection...");
            FileDescriptor newFd = socketReconnector.reconnect();
            if (newFd == null) {
                Ln.w("[DIAG] Reconnector returned null fd");
                return false;
            }
            streamer.setFd(newFd);
            Ln.i("New client connected, resuming stream");
            return true;
        } catch (IOException e) {
            Ln.e("[DIAG] Reconnection failed: " + e.getMessage());
            return false;
        }
    }

    private void streamCapture() throws IOException, ConfigurationException {
        Codec codec = streamer.getCodec();
        MediaCodec mediaCodec = createMediaCodec(codec, encoderName);
        MediaFormat format = createFormat(codec.getMimeType(), videoBitRate, maxFps, codecOptions);

        capture.init(reset);

        try {
            boolean alive;
            boolean headerWritten = false;
            boolean rotationBrokenPipeRecovery = false;
            int loopIteration = 0;

            do {
                loopIteration++;
                boolean wasReset = reset.consumeReset(); // If a capture reset was requested, it is implicitly fulfilled
                Ln.d("[DIAG] streamCapture loop iteration=" + loopIteration
                        + " wasReset=" + wasReset
                        + " rotationBrokenPipeRecovery=" + rotationBrokenPipeRecovery
                        + " pendingReset=" + reset.hasPendingReset()
                        + " stopped=" + stopped.get());

                if (wasReset) {
                    Ln.d("Capture reset consumed, re-preparing capture");

                    // Recreate the MediaCodec to avoid potential encoder state issues after rotation.
                    // Some devices (e.g. Samsung Exynos) do not properly handle reconfiguration of
                    // the same MediaCodec instance with a different resolution after stop/reset.
                    MediaCodec oldCodec = mediaCodec;
                    mediaCodec = null;
                    oldCodec.release();
                    mediaCodec = createMediaCodec(codec, encoderName);

                    prepareWithRetry();

                    // Brief delay to let the display settle after rotation
                    SystemClock.sleep(POST_RESET_SETTLE_DELAY_MS);
                } else {
                    capture.prepare();
                }
                Size size = capture.getSize();
                Ln.d("[DIAG] Capture size: " + size.getWidth() + "x" + size.getHeight());

                if (!headerWritten) {
                    streamer.writeVideoHeader(size);
                    headerWritten = true;
                    Ln.d("[DIAG] Video header written");
                }

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

                    Ln.d("[DIAG] Encoder started successfully, size=" + size.getWidth() + "x" + size.getHeight());

                    // Reset error counter — successfully restarted after rotation
                    consecutiveErrors = 0;

                    // Set the MediaCodec instance to "interrupt" (by signaling an EOS) on reset
                    reset.setRunningMediaCodec(mediaCodec);

                    if (stopped.get()) {
                        Ln.d("Encoder stopped before encoding started");
                        alive = false;
                    } else {
                        boolean resetRequested = reset.consumeReset();
                        if (resetRequested) {
                            Ln.d("Reset requested before encoding, skipping encode and restarting");
                        } else {
                            Ln.d("[DIAG] Entering encode() loop, totalPacketsWritten so far=" + totalPacketsWritten);
                            // If a reset is requested during encode(), it will interrupt the encoding by an EOS
                            encode(mediaCodec, streamer);
                            Ln.d("[DIAG] encode() returned normally, totalPacketsWritten=" + totalPacketsWritten);
                        }
                        // Encoding succeeded (or exited cleanly via reset), clear recovery flag
                        rotationBrokenPipeRecovery = false;
                        // The capture might have been closed internally (for example if the camera is disconnected)
                        alive = !stopped.get() && !capture.isClosed();
                        if (!alive) {
                            Ln.d("Streaming loop ending: stopped=" + stopped.get() + ", closed=" + capture.isClosed());
                        }
                    }
                } catch (IllegalStateException | IllegalArgumentException | IOException e) {
                    Ln.w("[DIAG] Exception in streamCapture: " + e.getClass().getName() + ": " + e.getMessage()
                            + " | isBrokenPipe=" + IO.isBrokenPipe(e)
                            + " | wasReset=" + wasReset
                            + " | pendingReset=" + reset.hasPendingReset()
                            + " | rotationBrokenPipeRecovery=" + rotationBrokenPipeRecovery
                            + " | iteration=" + loopIteration);
                    Ln.w("[DIAG] Full stack trace:\n" + getStackTraceString(e));

                    if (IO.isBrokenPipe(e)) {
                        if (reset.hasPendingReset() || wasReset) {
                            if (rotationBrokenPipeRecovery) {
                                // Socket is permanently broken after a rotation recovery attempt.
                                // Try to accept a new client connection.
                                Ln.w("[DIAG] Broken pipe persists after rotation recovery, socket is dead. Attempting reconnect...");
                                if (tryReconnect()) {
                                    headerWritten = false; // must re-send video header to new client
                                    rotationBrokenPipeRecovery = false;
                                    alive = true;
                                    continue;
                                }
                                Ln.w("Client disconnected during rotation and no reconnection available");
                                alive = false;
                                continue;
                            }
                            // Broken pipe during or just after a rotation transition: the client
                            // may have been disrupted by trailing encoder output during rotation.
                            // Allow the rotation loop to complete one more iteration so the new
                            // encoder gets a chance to write to the socket cleanly.
                            Ln.w("Broken pipe during rotation transition, retrying (wasReset=" + wasReset
                                    + ", pendingReset=" + reset.hasPendingReset() + ")");
                            rotationBrokenPipeRecovery = true;
                            alive = true;
                            continue;
                        }
                        if (rotationBrokenPipeRecovery) {
                            // The retry after rotation also got a broken pipe, but no new reset
                            // is pending. Try to accept a new client connection.
                            Ln.w("[DIAG] Broken pipe after rotation recovery attempt (no pending reset). Attempting reconnect...");
                            if (tryReconnect()) {
                                headerWritten = false;
                                rotationBrokenPipeRecovery = false;
                                alive = true;
                                continue;
                            }
                            Ln.w("Client disconnected during rotation and no reconnection available");
                            alive = false;
                            continue;
                        }
                        // Client disconnected normally (no rotation context).
                        // Try to accept a new client connection instead of dying.
                        Ln.i("Client disconnected. Waiting for a new client...");
                        if (tryReconnect()) {
                            headerWritten = false; // must re-send video header to new client
                            firstFrameSent = false;
                            alive = true;
                            continue;
                        }
                        // No reconnector or reconnection failed, terminate
                        Ln.d("No reconnection available, stopping stream");
                        throw e;
                    }
                    Ln.e("Capture/encoding error (consecutiveErrors=" + consecutiveErrors + "): " + e.getClass().getName() + ": " + e.getMessage());
                    if (!prepareRetry(size)) {
                        Ln.e("Giving up after " + consecutiveErrors + " consecutive errors");
                        throw e;
                    }
                    Ln.d("Retrying capture/encoding (consecutiveErrors=" + consecutiveErrors + ")");
                    alive = true;
                } finally {
                    reset.setRunningMediaCodec(null);
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
                    try {
                        mediaCodec.reset();
                    } catch (IllegalStateException e) {
                        Ln.w("Could not reset MediaCodec: " + e.getMessage());
                    }
                    if (surface != null) {
                        surface.release();
                    }
                }
            } while (alive);
            Ln.d("[DIAG] streamCapture loop exited, alive=false");
        } finally {
            if (mediaCodec != null) {
                mediaCodec.release();
            }
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
            // Progressive delay: wait longer on each retry to let rotation transition complete
            SystemClock.sleep(100L * consecutiveErrors);
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

        boolean accepted = capture.setMaxSize(newMaxSize);
        if (!accepted) {
            return false;
        }

        // Retry with a smaller size
        Ln.i("Retrying with -m" + newMaxSize + "...");
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

    private void encode(MediaCodec codec, Streamer streamer) throws IOException {
        MediaCodec.BufferInfo bufferInfo = new MediaCodec.BufferInfo();

        long packetsThisSession = 0;
        long packetsSkipped = 0;

        // Periodically refresh the capture to force the encoder to produce output
        // even when the screen content hasn't changed
        AtomicBoolean encoding = new AtomicBoolean(true);
        Thread refreshThread = new Thread(() -> {
            Ln.d("[DIAG] Frame refresh thread running");
            while (encoding.get()) {
                try {
                    Thread.sleep(frameRefreshIntervalMs);
                } catch (InterruptedException e) {
                    break;
                }
                if (!encoding.get()) {
                    break;
                }
                try {
                    capture.requestRefresh();
                    Ln.d("[DIAG] Requested capture refresh");
                } catch (Exception e) {
                    Ln.d("[DIAG] Frame refresh thread stopping: " + e.getMessage());
                    break;
                }
            }
        }, "frame-refresh");
        refreshThread.setDaemon(true);
        refreshThread.start();
        Ln.d("[DIAG] Frame refresh thread started (interval=" + frameRefreshIntervalMs + "ms)");

        boolean eos = false;
        try {
        do {
            int outputBufferId = codec.dequeueOutputBuffer(bufferInfo, DEQUEUE_TIMEOUT_US);
            try {
                if (outputBufferId == MediaCodec.INFO_TRY_AGAIN_LATER) {
                    // No output available within the timeout
                    Ln.d("[DIAG] dequeueOutputBuffer: TRY_AGAIN_LATER (no output within " + DEQUEUE_TIMEOUT_US + "us)");
                    if (stopped.get() || reset.hasPendingReset()) {
                        // Either stopped or a reset is pending but the EOS might not have been delivered
                        Ln.d("Exiting encode loop: stopped=" + stopped.get() + ", pendingReset=" + reset.hasPendingReset()
                                + " (wrote " + packetsThisSession + " packets, skipped " + packetsSkipped + ")");
                        return;
                    }
                    continue;
                }
                eos = (bufferInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0;
                if (eos) {
                    // Do not write EOS buffer data to the stream: it may contain incomplete or
                    // corrupted NAL units (especially on Samsung Exynos encoders during rotation),
                    // which could cause the client to disconnect.
                    Ln.d("EOS received, exiting encode loop (wrote " + packetsThisSession + " packets, skipped " + packetsSkipped + ")");
                    break;
                }
                if (outputBufferId >= 0 && bufferInfo.size > 0) {
                    Ln.d("[DIAG] dequeueOutputBuffer: got buffer id=" + outputBufferId + " size=" + bufferInfo.size);
                    ByteBuffer codecBuffer = codec.getOutputBuffer(outputBufferId);

                    boolean isConfig = (bufferInfo.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0;
                    boolean isKeyFrame = (bufferInfo.flags & MediaCodec.BUFFER_FLAG_KEY_FRAME) != 0;

                    // Skip writing trailing output buffers when a reset is pending.
                    // After signalEndOfInputStream(), the encoder may flush remaining
                    // buffers that contain incomplete or corrupted NAL units (especially
                    // on Samsung Exynos encoders during rotation), which could cause the
                    // client to disconnect (broken pipe).
                    if (reset.hasPendingReset()) {
                        Ln.d("Skipping packet write during pending reset (size=" + bufferInfo.size
                                + " config=" + isConfig + " keyFrame=" + isKeyFrame
                                + " pts=" + bufferInfo.presentationTimeUs + ")");
                        packetsSkipped++;
                        continue;
                    }

                    if (!isConfig) {
                        // If this is not a config packet, then it contains a frame
                        firstFrameSent = true;
                        consecutiveErrors = 0;
                    }

                    try {
                        streamer.writePacket(codecBuffer, bufferInfo);
                        packetsThisSession++;
                        totalPacketsWritten++;
                    } catch (IOException e) {
                        boolean pendingNow = reset.hasPendingReset();
                        Ln.w("[DIAG] writePacket failed: " + e.getClass().getName() + ": " + e.getMessage()
                                + " | isBrokenPipe=" + IO.isBrokenPipe(e)
                                + " | pendingResetNow=" + pendingNow
                                + " | packetSize=" + bufferInfo.size
                                + " | isConfig=" + isConfig
                                + " | isKeyFrame=" + isKeyFrame
                                + " | pts=" + bufferInfo.presentationTimeUs
                                + " | packetsThisSession=" + packetsThisSession
                                + " | totalPacketsWritten=" + totalPacketsWritten);
                        if (IO.isBrokenPipe(e) && pendingNow) {
                            // Race condition: rotation was triggered between the hasPendingReset()
                            // check above and the write. The packet may have contained corrupted
                            // encoder output from the resolution change. Exit the encode loop
                            // cleanly so streamCapture() can handle the reset.
                            Ln.w("[DIAG] Broken pipe caught during encode with pending reset — race condition hit. Exiting encode loop cleanly.");
                            return;
                        }
                        Ln.w("[DIAG] writePacket broken pipe with NO pending reset — propagating exception");
                        throw e;
                    }
                }
            } finally {
                if (outputBufferId >= 0) {
                    codec.releaseOutputBuffer(outputBufferId, false);
                }
            }
        } while (!eos);
        } finally {
            encoding.set(false);
            refreshThread.interrupt();
        }
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

            boolean fatalError = true;
            try {
                streamCapture();
                fatalError = false; // ended normally (all clients disconnected, no reconnector)
            } catch (ConfigurationException e) {
                // Do not print stack trace, a user-friendly error-message has already been logged
                Ln.d("Streaming stopped due to configuration error");
            } catch (IOException e) {
                if (!IO.isBrokenPipe(e)) {
                    Ln.e("Video encoding error", e);
                } else {
                    // Broken pipe means client disconnected and reconnection was not available.
                    // This is not a fatal server error.
                    Ln.d("Streaming stopped due to broken pipe (client disconnected)");
                    fatalError = false;
                }
            } catch (RuntimeException e) {
                Ln.e("Unexpected runtime error during video encoding:\n" + getStackTraceString(e), e);
            } finally {
                Ln.d("Screen streaming stopped (totalPacketsWritten=" + totalPacketsWritten + ", fatalError=" + fatalError + ")");
                listener.onTerminated(fatalError);
            }
        }, "video");
        thread.start();
    }

    @Override
    public void stop() {
        if (thread != null) {
            stopped.set(true);
            reset.reset();
        }
    }

    @Override
    public void join() throws InterruptedException {
        if (thread != null) {
            thread.join();
        }
    }
}
