package com.genymobile.scrcpy;

import com.genymobile.scrcpy.wrappers.SurfaceControl;

import android.graphics.Rect;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.os.IBinder;
import android.view.Surface;

import java.io.IOException;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.util.concurrent.atomic.AtomicBoolean;

public class ScreenEncoder implements Device.RotationListener {

    private static final int DEFAULT_FRAME_RATE = 60; // fps
    private static final int DEFAULT_I_FRAME_INTERVAL = 10; // seconds

    private static final int REPEAT_FRAME_DELAY = 6; // repeat after 6 frames

    private static final int MICROSECONDS_IN_ONE_SECOND = 1_000_000;

    private final AtomicBoolean rotationChanged = new AtomicBoolean();

    private int bitRate;
    private int frameRate;
    private int iFrameInterval;

    public ScreenEncoder(int bitRate, int frameRate, int iFrameInterval) {
        this.bitRate = bitRate;
        this.frameRate = frameRate;
        this.iFrameInterval = iFrameInterval;
    }

    public ScreenEncoder(int bitRate) {
        this(bitRate, DEFAULT_FRAME_RATE, DEFAULT_I_FRAME_INTERVAL);
    }

    @Override
    public void onRotationChanged(int rotation) {
        rotationChanged.set(true);
    }

    public boolean consumeRotationChange() {
        return rotationChanged.getAndSet(false);
    }

    public void streamScreen(Device device, OutputStream outputStream) throws IOException {
        MediaFormat format = createFormat(bitRate, frameRate, iFrameInterval);
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
                    alive = encode(codec, outputStream);
                } finally {
                    codec.stop();
                    destroyDisplay(display);
                    codec.release();
                    surface.release();
                }
            } while (alive);
        } finally {
            device.setRotationListener(null);
        }
    }

    private boolean encode(MediaCodec codec, OutputStream outputStream) throws IOException {
        @SuppressWarnings("checkstyle:MagicNumber")
        byte[] buf = new byte[bitRate / 8]; // may contain up to 1 second of video
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
                    ByteBuffer outputBuffer = codec.getOutputBuffer(outputBufferId);
                    while (outputBuffer.hasRemaining()) {
                        int remaining = outputBuffer.remaining();
                        int len = Math.min(buf.length, remaining);
                        // the outputBuffer is probably direct (it has no underlying array), and LocalSocket does not expose channels,
                        // so we must copy the data locally to write them manually to the output stream
                        outputBuffer.get(buf, 0, len);
                        outputStream.write(buf, 0, len);
                    }
                }
            } finally {
                if (outputBufferId >= 0) {
                    codec.releaseOutputBuffer(outputBufferId, false);
                }
            }
        }

        return !eof;
    }

    private static MediaCodec createCodec() throws IOException {
        return MediaCodec.createEncoderByType("video/avc");
    }

    private static MediaFormat createFormat(int bitRate, int frameRate, int iFrameInterval) throws IOException {
        MediaFormat format = new MediaFormat();
        format.setString(MediaFormat.KEY_MIME, "video/avc");
        format.setInteger(MediaFormat.KEY_BIT_RATE, bitRate);
        format.setInteger(MediaFormat.KEY_FRAME_RATE, frameRate);
        format.setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);
        format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, iFrameInterval);
        // display the very first frame, and recover from bad quality when no new frames
        format.setLong(MediaFormat.KEY_REPEAT_PREVIOUS_FRAME_AFTER, MICROSECONDS_IN_ONE_SECOND * REPEAT_FRAME_DELAY / frameRate); // µs
        return format;
    }

    private static IBinder createDisplay() {
        return SurfaceControl.createDisplay("scrcpy", false);
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
