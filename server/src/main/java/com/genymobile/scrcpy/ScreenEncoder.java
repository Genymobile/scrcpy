package com.genymobile.scrcpy;

import com.genymobile.scrcpy.wrappers.SurfaceControl;

import android.graphics.Rect;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.media.MediaFormat;
import android.os.Build;
import android.os.IBinder;
import android.view.Surface;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

public class ScreenEncoder implements Connection.StreamInvalidateListener, Runnable {

    private static final int DEFAULT_I_FRAME_INTERVAL = 10; // seconds
    private static final int REPEAT_FRAME_DELAY_US = 100_000; // repeat after 100ms
    private static final String KEY_MAX_FPS_TO_ENCODER = "max-fps-to-encoder";

    private static final int NO_PTS = -1;

    private final AtomicBoolean streamIsInvalide = new AtomicBoolean();
    private final ByteBuffer headerBuffer = ByteBuffer.allocate(12);
    private Thread selectorThread;

    private long ptsOrigin;
    private Device device;
    private Connection connection;
    private VideoSettings videoSettings;
    private MediaFormat format;
    private int timeout = -1;

    public ScreenEncoder(VideoSettings videoSettings) {
        this.videoSettings = videoSettings;
        updateFormat();
    }

    private void updateFormat() {
        format = createFormat(videoSettings);
        int maxFps = videoSettings.getMaxFps();
        if (maxFps > 0) {
            timeout = 1_000_000 / maxFps;
        } else {
            timeout = -1;
        }
    }

    public void setConnection(Connection connection) {
        this.connection = connection;
    }

    public void setDevice(Device device) {
        this.device = device;
    }

    @Override
    public void onStreamInvalidate() {
        Ln.d("invalidate stream");
        streamIsInvalide.set(true);
        updateFormat();
    }

    public boolean consumeStreamInvalidation() {
        return streamIsInvalide.getAndSet(false);
    }

    public boolean isAlive() {
        return selectorThread != null && selectorThread.isAlive();
    }

    public void streamScreen() throws IOException {
        Workarounds.prepareMainLooper();

        try {
            internalStreamScreen();
        } catch (NullPointerException e) {
            // Retry with workarounds enabled:
            // <https://github.com/Genymobile/scrcpy/issues/365>
            // <https://github.com/Genymobile/scrcpy/issues/940>
            Ln.d("Applying workarounds to avoid NullPointerException");
            Workarounds.fillAppInfo();
            internalStreamScreen();
        }
    }

    private void internalStreamScreen() throws IOException {
        updateFormat();
        connection.setStreamInvalidateListener(this);
        boolean alive;
        try {
            do {
                MediaCodec codec = createCodec(videoSettings.getEncoderName());
                IBinder display = createDisplay();
                ScreenInfo screenInfo = device.getScreenInfo();
                Rect contentRect = screenInfo.getContentRect();
                // include the locked video orientation
                Rect videoRect = screenInfo.getVideoSize().toRect();
                // does not include the locked video orientation
                Rect unlockedVideoRect = screenInfo.getUnlockedVideoSize().toRect();
                int videoRotation = screenInfo.getVideoRotation();
                int layerStack = device.getLayerStack();

                setSize(format, videoRect.width(), videoRect.height());
                configure(codec, format);
                Surface surface = codec.createInputSurface();
                setDisplaySurface(display, surface, videoRotation, contentRect, unlockedVideoRect, layerStack);
                codec.start();
                try {
                    alive = encode(codec);
                    // do not call stop() on exception, it would trigger an IllegalStateException
                    codec.stop();
                } finally {
                    destroyDisplay(display);
                    codec.release();
                    surface.release();
                }
            } while (alive);
        } finally {
            connection.setStreamInvalidateListener(null);
        }
    }

    private boolean encode(MediaCodec codec) throws IOException {
        boolean eof = false;
        MediaCodec.BufferInfo bufferInfo = new MediaCodec.BufferInfo();

        while (!consumeStreamInvalidation() && !eof && connection.hasConnections()) {
            int outputBufferId = codec.dequeueOutputBuffer(bufferInfo, timeout);
            eof = (bufferInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0;
            try {
                if (consumeStreamInvalidation()) {
                    // must restart encoding with new size
                    break;
                }
                if (outputBufferId >= 0) {
                    ByteBuffer codecBuffer = codec.getOutputBuffer(outputBufferId);

                    if (videoSettings.getSendFrameMeta()) {
                        writeFrameMeta(bufferInfo, codecBuffer.remaining());
                    }

                    connection.send(codecBuffer);
                }
            } finally {
                if (outputBufferId >= 0) {
                    codec.releaseOutputBuffer(outputBufferId, false);
                }
            }
        }

        return !eof && connection.hasConnections();
    }

    private void writeFrameMeta(MediaCodec.BufferInfo bufferInfo, int packetSize) throws IOException {
        headerBuffer.clear();

        long pts;
        if ((bufferInfo.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0) {
            pts = NO_PTS; // non-media data packet
        } else {
            if (ptsOrigin == 0) {
                ptsOrigin = bufferInfo.presentationTimeUs;
            }
            pts = bufferInfo.presentationTimeUs - ptsOrigin;
        }

        headerBuffer.putLong(pts);
        headerBuffer.putInt(packetSize);
        headerBuffer.flip();
        connection.send(headerBuffer);
    }

    public static MediaCodecInfo[] listEncoders() {
        List<MediaCodecInfo> result = new ArrayList<>();
        MediaCodecList list = new MediaCodecList(MediaCodecList.REGULAR_CODECS);
        for (MediaCodecInfo codecInfo : list.getCodecInfos()) {
            if (codecInfo.isEncoder() && Arrays.asList(codecInfo.getSupportedTypes()).contains(MediaFormat.MIMETYPE_VIDEO_AVC)) {
                result.add(codecInfo);
            }
        }
        return result.toArray(new MediaCodecInfo[result.size()]);
    }

    private static MediaCodec createCodec(String encoderName) throws IOException {
        if (encoderName != null) {
            Ln.d("Creating encoder by name: '" + encoderName + "'");
            try {
                return MediaCodec.createByCodecName(encoderName);
            } catch (IllegalArgumentException e) {
                MediaCodecInfo[] encoders = listEncoders();
                throw new InvalidEncoderException(encoderName, encoders);
            }
        }
        MediaCodec codec = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_VIDEO_AVC);
        Ln.d("Using encoder: '" + codec.getName() + "'");
        return codec;
    }

    private static void setCodecOption(MediaFormat format, CodecOption codecOption) {
        String key = codecOption.getKey();
        Object value = codecOption.getValue();

        if (value instanceof Integer) {
            format.setInteger(key, (Integer) value);
        } else if (value instanceof Long) {
            format.setLong(key, (Long) value);
        } else if (value instanceof Float) {
            format.setFloat(key, (Float) value);
        } else if (value instanceof String) {
            format.setString(key, (String) value);
        }

        Ln.d("Codec option set: " + key + " (" + value.getClass().getSimpleName() + ") = " + value);
    }

    private static MediaFormat createFormat(VideoSettings videoSettings) {
        int bitRate =  videoSettings.getBitRate();
        int maxFps = videoSettings.getMaxFps();
        int iFrameInterval = videoSettings.getIFrameInterval();
        List<CodecOption> codecOptions = videoSettings.getCodecOptions();
        MediaFormat format = new MediaFormat();
        format.setString(MediaFormat.KEY_MIME, MediaFormat.MIMETYPE_VIDEO_AVC);
        format.setInteger(MediaFormat.KEY_BIT_RATE, bitRate);
        // must be present to configure the encoder, but does not impact the actual frame rate, which is variable
        format.setInteger(MediaFormat.KEY_FRAME_RATE, 60);
        format.setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);
        format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, iFrameInterval);
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
                setCodecOption(format, option);
            }
        }

        return format;
    }

    private static IBinder createDisplay() {
        // Since Android 12 (preview), secure displays could not be created with shell permissions anymore.
        // On Android 12 preview, SDK_INT is still R (not S), but CODENAME is "S".
        boolean secure = Build.VERSION.SDK_INT < Build.VERSION_CODES.R || (Build.VERSION.SDK_INT == Build.VERSION_CODES.R && !"S"
                .equals(Build.VERSION.CODENAME));
        return SurfaceControl.createDisplay("scrcpy", secure);
    }

    private static void configure(MediaCodec codec, MediaFormat format) {
        codec.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
    }

    private static void setSize(MediaFormat format, int width, int height) {
        format.setInteger(MediaFormat.KEY_WIDTH, width);
        format.setInteger(MediaFormat.KEY_HEIGHT, height);
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

    private static void destroyDisplay(IBinder display) {
        SurfaceControl.destroyDisplay(display);
    }

    @Override
    public void run() {
        synchronized (this) {
            if (selectorThread != null && selectorThread.isAlive()) {
                throw new IllegalStateException(getClass().getName() + " can only be started once.");
            }
            selectorThread = Thread.currentThread();
        }
        try {
            this.streamScreen();
        } catch (IOException e) {
            Ln.e("Failed to start screen recorder", e);
        }
    }

    public void start(Device device, Connection connection) {
        this.device = device;
        this.connection = connection;
        if (selectorThread != null && selectorThread.isAlive()) {
            throw new IllegalStateException(getClass().getName() + " can only be started once.");
        }
        new Thread(this).start();
    }
}
