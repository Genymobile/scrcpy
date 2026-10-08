package com.genymobile.scrcpy.video;

import com.genymobile.scrcpy.AsyncProcessor;
import com.genymobile.scrcpy.Options;
import com.genymobile.scrcpy.device.Streamer;
import com.genymobile.scrcpy.model.CodecOption;
import com.genymobile.scrcpy.model.ConfigurationException;
import com.genymobile.scrcpy.model.Size;
import com.genymobile.scrcpy.util.IO;
import com.genymobile.scrcpy.util.Ln;

import android.graphics.Bitmap;
import android.graphics.PixelFormat;
import android.media.Image;
import android.media.ImageReader;
import android.os.SystemClock;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Software video "encoder" which does not use MediaCodec at all: it periodically polls a raw screen frame from an
 * {@link ImageReader} (fed by the same virtual display mechanism as {@link SurfaceEncoder}) and compresses it to
 * JPEG, so the result can be streamed as Motion JPEG. This is much less efficient than a real video codec, but it
 * works even on devices where MediaCodec is broken or unusable.
 */
public class JpegEncoder implements AsyncProcessor {

    private static final int DEFAULT_QUALITY = 80;
    private static final int DEFAULT_FPS = 4;

    private final SurfaceCapture capture;
    private final Streamer streamer;
    private final int maxSize;
    private final int minSizeAlignment;
    private final long frameIntervalMs;
    private final int quality;

    private final CaptureControl captureControl = new CaptureControl();

    private Thread thread;
    private final AtomicBoolean stopped = new AtomicBoolean();

    public JpegEncoder(SurfaceCapture capture, Streamer streamer, Options options) {
        this.capture = capture;
        this.streamer = streamer;
        this.maxSize = options.getMaxSize();
        this.minSizeAlignment = options.getMinSizeAlignment();
        float fps = options.getMaxFps() > 0 ? options.getMaxFps() : DEFAULT_FPS;
        this.frameIntervalMs = (long) (1000 / fps);
        this.quality = findQuality(options.getVideoCodecOptions());
    }

    private static int findQuality(List<CodecOption> codecOptions) {
        if (codecOptions != null) {
            for (CodecOption option : codecOptions) {
                if ("quality".equals(option.getKey()) && option.getValue() instanceof Integer) {
                    return (Integer) option.getValue();
                }
            }
        }
        return DEFAULT_QUALITY;
    }

    private void streamCapture() throws IOException, ConfigurationException {
        VideoConstraints videoConstraints = new VideoConstraints(maxSize, minSizeAlignment, null);
        capture.init(captureControl, videoConstraints);

        long startNanos = System.nanoTime();

        try {
            streamer.writeVideoHeader();

            boolean alive;
            do {
                int resetReasons = captureControl.consumeReset();
                if ((resetReasons & CaptureControl.RESET_REASON_TERMINATED) != 0) {
                    break;
                }

                capture.prepare();
                Size size = capture.getSize();
                ImageReader imageReader = ImageReader.newInstance(size.getWidth(), size.getHeight(), PixelFormat.RGBA_8888, 2);

                boolean captureStarted = false;
                try {
                    capture.start(imageReader.getSurface());
                    captureStarted = true;

                    boolean isClientResize = (resetReasons & CaptureControl.RESET_REASON_CLIENT_RESIZED) != 0
                            && (resetReasons & CaptureControl.RESET_REASON_DISPLAY_PROPERTIES_CHANGED) == 0;
                    streamer.writeSessionMeta(size.getWidth(), size.getHeight(), isClientResize);

                    while (!stopped.get() && !captureControl.isResetRequested()) {
                        Image image = imageReader.acquireLatestImage();
                        if (image != null) {
                            try {
                                sendFrame(image, System.nanoTime() - startNanos);
                            } finally {
                                image.close();
                            }
                        }
                        SystemClock.sleep(frameIntervalMs);
                    }

                    alive = !stopped.get() && !capture.isClosed();
                } finally {
                    if (captureStarted) {
                        capture.stop();
                    }
                    imageReader.close();
                }
            } while (alive);
        } finally {
            capture.release();
        }
    }

    private void sendFrame(Image image, long ptsNanos) throws IOException {
        byte[] jpeg = toJpeg(image, quality);
        // Every JPEG frame is fully self-contained, so it can always be considered a key frame, and there is no config packet
        streamer.writePacket(ByteBuffer.wrap(jpeg), ptsNanos / 1000, false, true);
    }

    private static byte[] toJpeg(Image image, int quality) {
        Image.Plane plane = image.getPlanes()[0];
        ByteBuffer buffer = plane.getBuffer();
        int pixelStride = plane.getPixelStride();
        int rowStride = plane.getRowStride();
        int width = image.getWidth();
        int height = image.getHeight();

        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        if (rowStride == pixelStride * width) {
            // Tightly packed: the buffer can be copied directly, without any intermediate allocation
            bitmap.copyPixelsFromBuffer(buffer);
        } else {
            // Padded rows: let Bitmap#setPixels() skip the padding directly via its stride parameter, instead of
            // allocating an oversized bitmap just to crop it right after
            int stride = rowStride / pixelStride;
            int[] pixels = new int[stride * height];
            buffer.asIntBuffer().get(pixels);
            bitmap.setPixels(pixels, 0, stride, 0, 0, width, height);
        }

        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        bitmap.compress(Bitmap.CompressFormat.JPEG, quality, baos);
        return baos.toByteArray();
    }

    @Override
    public void start(TerminationListener listener) {
        thread = new Thread(() -> {
            try {
                streamCapture();
            } catch (ConfigurationException e) {
                // Do not print stack trace, a user-friendly error-message has already been logged
            } catch (IOException e) {
                if (!IO.isBrokenPipe(e)) {
                    Ln.e("JPEG encoding error", e);
                }
            } finally {
                Ln.d("Screen streaming stopped");
                listener.onTerminated(true);
            }
        }, "jpeg-video");
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
