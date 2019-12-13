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

import android.graphics.ImageFormat;
import android.graphics.YuvImage;
import android.media.Image;
import android.media.MediaExtractor;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.concurrent.LinkedBlockingQueue;
import java.io.OutputStream;
import java.io.ByteArrayOutputStream;

import android.media.ImageReader;
import android.graphics.Bitmap;
import android.os.Environment;
import android.graphics.PixelFormat;
import android.os.Handler;
import android.os.Message;

import java.util.Arrays;

import android.os.Looper;

import java.nio.ByteOrder;

import android.os.Process;

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

    private int quality;
    private int scale;
    private Handler mHandler;
    private ImageReader mImageReader;
    private ImageListener mImageListener;

    public ScreenEncoder(boolean sendFrameMeta, int bitRate, int maxFps, int iFrameInterval) {
        this.sendFrameMeta = sendFrameMeta;
        this.bitRate = bitRate;
        this.maxFps = maxFps;
        this.iFrameInterval = iFrameInterval;
    }

    public ScreenEncoder(boolean sendFrameMeta, int bitRate, int maxFps) {
        this(sendFrameMeta, bitRate, maxFps, DEFAULT_I_FRAME_INTERVAL);
    }

    public ScreenEncoder(int quality, int maxFps, int scale) {
        this.quality = quality;
        this.maxFps = maxFps;
        this.scale = scale;
    }

    @Override
    public void onRotationChanged(int rotation) {
        rotationChanged.set(true);
    }

    public boolean consumeRotationChange() {
        return rotationChanged.getAndSet(false);
    }

    private final class ImageListener implements ImageReader.OnImageAvailableListener {
        @Override
        public void onImageAvailable(ImageReader reader) {
            Ln.i("onImageAvailable !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
            Ln.i("onImageAvailable !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
            Ln.i("onImageAvailable !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
            Ln.i("onImageAvailable !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
            Ln.i("onImageAvailable !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        }
    }

    public void streamScreen(Device device, FileDescriptor fd) throws IOException {
        Workarounds.prepareMainLooper();
        Workarounds.fillAppInfo();

        device.setRotationListener(this);
        boolean alive;
        try {
            banner(device, fd);
            do {
//                mHandler = new Handler(Looper.getMainLooper());
                IBinder display = createDisplay();
                Rect contentRect = device.getScreenInfo().getContentRect();
//                Rect videoRect = device.getScreenInfo().getVideoSize().toRect();
                Rect videoRect = new Rect(0, 0, contentRect.width() / scale, contentRect.height() / scale);
                mImageReader = ImageReader.newInstance(videoRect.width(), videoRect.height(), PixelFormat.RGBA_8888, 2);
                mImageListener = new ImageListener();
                mImageReader.setOnImageAvailableListener(mImageListener, mHandler);
                Surface surface = mImageReader.getSurface();
                setDisplaySurface(display, surface, contentRect, videoRect);
                try {
                    alive = encode(mImageReader, fd);
                } finally {
                    destroyDisplay(display);
                    surface.release();
                }
            } while (alive);
        } finally {
            device.setRotationListener(null);
        }
    }

    private boolean encode(ImageReader imageReader, FileDescriptor fd) throws IOException {
        int count = 0;
        long current = System.currentTimeMillis();
        int type = 0;// 0:libjpeg-turbo 1:bitmap
        int frameRate = this.maxFps;
        int quality = this.quality;
        int framePeriodMs = (int) (1000 / frameRate);
        while (!consumeRotationChange()) {
            long timeA = System.currentTimeMillis();
            Image image = null;
            int loop = 0;
            int wait = 1;
            // TODO onImageAvailable这个方法不回调，未找到原因，暂时写成while
            while ((image = imageReader.acquireNextImage()) == null && ++loop < 10) {
                try {
                    Thread.sleep(wait++);
                } catch (InterruptedException e) {
                }
            }
            if (image == null) {
                continue;
            }
            int width = image.getWidth();
            int height = image.getHeight();
            int format = image.getFormat();//RGBA_8888 0x00000001
            final Image.Plane[] planes = image.getPlanes();
            final ByteBuffer buffer = planes[0].getBuffer();
            int pixelStride = planes[0].getPixelStride();
            int rowStride = planes[0].getRowStride();
            int rowPadding = rowStride - pixelStride * width;
            int pitch = width + rowPadding / pixelStride;
            byte[] jpegData = null;
            byte[] jpegSize = null;
            if (type == 0) {
                jpegData = JpegEncoder.compress(buffer, width, pitch, height, quality);
            } else if (type == 1) {
                ByteArrayOutputStream stream = new ByteArrayOutputStream();
                Bitmap bitmap = Bitmap.createBitmap(pitch, height, Bitmap.Config.ARGB_8888);
                bitmap.copyPixelsFromBuffer(buffer);
                bitmap.compress(Bitmap.CompressFormat.JPEG, quality, stream);
                jpegData = stream.toByteArray();
                bitmap.recycle();
            }
            image.close();
            if (jpegData == null) {
                Ln.e("jpegData is null");
                continue;
            }
            ByteBuffer b = ByteBuffer.allocate(4);
            b.order(ByteOrder.LITTLE_ENDIAN);
            b.putInt(jpegData.length);
            jpegSize = b.array();
            IO.writeFully(fd, jpegSize, 0, jpegSize.length);
            IO.writeFully(fd, jpegData, 0, jpegData.length);

            count++;
            long timeB = System.currentTimeMillis();
            if (timeB - current >= 1000) {
                current = timeB;
                Ln.i("frame rate: " + count + ", jpeg size: " + jpegData.length);
                count = 0;
            }

            if (framePeriodMs > 0) {
                long ms = framePeriodMs - timeB + timeA;
                if (ms > 0) {
                    try {
                        Thread.sleep(ms);
                    } catch (InterruptedException e) {
                    }
                }
            }
        }
        return true;
    }

    // minicap banner
    private void banner(Device device, FileDescriptor fd) throws IOException {
        final byte BANNER_SIZE = 24;
        Rect videoRect = device.getScreenInfo().getVideoSize().toRect();
        int width = videoRect.width();
        int height = videoRect.height();
        int pid = Process.myPid();

        ByteBuffer b = ByteBuffer.allocate(BANNER_SIZE);
        b.order(ByteOrder.LITTLE_ENDIAN);
        b.put((byte) 1);//version
        b.put(BANNER_SIZE);//banner size
        b.putInt(pid);//pid
        b.putInt(width);//real width
        b.putInt(height);//real height
        b.putInt(width);//desired width
        b.putInt(height);//desired height
        b.put((byte) 0);//orientation
        b.put((byte) 2);//quirks
        byte[] array = b.array();
        IO.writeFully(fd, array, 0, array.length);
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
