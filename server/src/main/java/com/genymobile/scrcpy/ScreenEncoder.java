package com.genymobile.scrcpy;

import com.genymobile.scrcpy.wrappers.SurfaceControl;

import android.graphics.Rect;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.os.Build;
import android.os.IBinder;
import android.view.Surface;

import java.io.FileDescriptor;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.concurrent.atomic.AtomicBoolean;

public class ScreenEncoder implements Device.RotationListener {

    private static final int DEFAULT_I_FRAME_INTERVAL = 10; // seconds
    private static final int REPEAT_FRAME_DELAY_US = 100_000; // repeat after 100ms

    private static final int NO_PTS = -1;

    private final AtomicBoolean rotationChanged = new AtomicBoolean();
    private final ByteBuffer headerBuffer = ByteBuffer.allocate(12);

    private int bitRate;
    private int maxFps;
    private int iFrameInterval;
    private boolean sendFrameMeta;
    private long ptsOrigin;

    public ScreenEncoder(boolean sendFrameMeta, int bitRate, int maxFps, int iFrameInterval) {
        this.sendFrameMeta = sendFrameMeta;
        this.bitRate = bitRate;
        this.maxFps = maxFps;
        this.iFrameInterval = iFrameInterval;
    }

    public ScreenEncoder(boolean sendFrameMeta, int bitRate, int maxFps) {
        this(sendFrameMeta, bitRate, maxFps, DEFAULT_I_FRAME_INTERVAL);
    }

    @Override
    public void onRotationChanged(int rotation) {
        rotationChanged.set(true);
    }

    public boolean consumeRotationChange() {
        return rotationChanged.getAndSet(false);
    }

    public void streamScreen(Device device, FileDescriptor fd) throws IOException {
        Workarounds.prepareMainLooper();
        Workarounds.fillAppInfo();

        MediaFormat format = createFormat(bitRate, maxFps, iFrameInterval);
        device.setRotationListener(this);
        boolean alive;
        try {
            do {
                MediaCodec codec = createCodec();
                IBinder display = createDisplay();
                Rect contentRect = device.getScreenInfo().getContentRect();
                Rect videoRect = device.getScreenInfo().getVideoSize().toRect();
                setSize(format, videoRect.width(), videoRect.height());
                configure(codec, format);
                Surface surface = codec.createInputSurface();
                setDisplaySurface(display, surface, contentRect, videoRect);
                codec.start();
                try {
                    alive = encode(codec, fd);
                    // do not call stop() on exception, it would trigger an IllegalStateException
                    codec.stop();
                } finally {
                    destroyDisplay(display);
                    codec.release();
                    surface.release();
                }
            } while (alive);
        } finally {
            device.setRotationListener(null);
        }
    }

    private boolean encode(MediaCodec codec, FileDescriptor fd) throws IOException {
        boolean eof = false;
        MediaCodec.BufferInfo bufferInfo = new MediaCodec.BufferInfo();

        while (!consumeRotationChange() && !eof) {
            int outputBufferId = codec.dequeueOutputBuffer(bufferInfo, -1);
            eof = (bufferInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0;
            try {
                if (consumeRotationChange()) {
                    // must restart encoding with new size
                    break;
                }
                if (outputBufferId >= 0) {
                    ByteBuffer codecBuffer = codec.getOutputBuffer(outputBufferId);

                    if (sendFrameMeta) {
                        writeFrameMeta(fd, bufferInfo, codecBuffer.remaining());
                    }

                    IO.writeFully(fd, codecBuffer);
                }
            } finally {
                if (outputBufferId >= 0) {
                    codec.releaseOutputBuffer(outputBufferId, false);
                }
            }
        }

        return !eof;
    }

    private void writeFrameMeta(FileDescriptor fd, MediaCodec.BufferInfo bufferInfo, int packetSize) throws IOException {
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
        IO.writeFully(fd, headerBuffer);
    }

    private static MediaCodec createCodec() throws IOException {
        return MediaCodec.createEncoderByType("video/avc");
    }

    @SuppressWarnings("checkstyle:MagicNumber")
    private static MediaFormat createFormat(int bitRate, int maxFps, int iFrameInterval) {
        MediaFormat format = new MediaFormat();
        format.setString(MediaFormat.KEY_MIME, "video/avc");
        format.setInteger(MediaFormat.KEY_BIT_RATE, bitRate);
        // must be present to configure the encoder, but does not impact the actual frame rate, which is variable
        format.setInteger(MediaFormat.KEY_FRAME_RATE, 60);
        format.setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);
        format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, iFrameInterval);
        // display the very first frame, and recover from bad quality when no new frames
        format.setLong(MediaFormat.KEY_REPEAT_PREVIOUS_FRAME_AFTER, REPEAT_FRAME_DELAY_US); // µs
        if (maxFps > 0) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                format.setFloat(MediaFormat.KEY_MAX_FPS_TO_ENCODER, maxFps);
            } else {
                Ln.w("Max FPS is only supported since Android 10, the option has been ignored");
            }
        }
        return format;
    }

    private static IBinder createDisplay() {
        return SurfaceControl.createDisplay("scrcpy", true);
    }

    private static void configure(MediaCodec codec, MediaFormat format) {
        codec.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
    }

    private static void setSize(MediaFormat format, int width, int height) {
        format.setInteger(MediaFormat.KEY_WIDTH, width);
        format.setInteger(MediaFormat.KEY_HEIGHT, height);
    }

    private static void setDisplaySurface(IBinder display, Surface surface, Rect deviceRect, Rect displayRect) {
        SurfaceControl.openTransaction();
        try {
            SurfaceControl.setDisplaySurface(display, surface);
            SurfaceControl.setDisplayProjection(display, 0, deviceRect, displayRect);
            SurfaceControl.setDisplayLayerStack(display, 0);
        } finally {
            SurfaceControl.closeTransaction();
        }
    }

    private static void destroyDisplay(IBinder display) {
        SurfaceControl.destroyDisplay(display);
    }
}
