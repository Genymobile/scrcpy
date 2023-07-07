package com.genymobile.scrcpy;

import android.annotation.SuppressLint;
import android.content.Context;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.SystemClock;
import android.view.Surface;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.Semaphore;
import java.util.concurrent.atomic.AtomicBoolean;

public class CameraEncoder implements AsyncProcessor {

    private static final int DEFAULT_I_FRAME_INTERVAL = 10; // seconds
    private static final int REPEAT_FRAME_DELAY_US = 100_000; // repeat after 100ms
    private static final String KEY_MAX_FPS_TO_ENCODER = "max-fps-to-encoder";

    // Keep the values in descending order
    private static final int[] MAX_SIZE_FALLBACK = {2560, 1920, 1600, 1280, 1024, 800};
    private static final int MAX_CONSECUTIVE_ERRORS = 3;

    private int maxSize;
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

    private HandlerThread cameraThread;
    private Handler cameraHandler;

    public CameraEncoder(int maxSize, Streamer streamer, int videoBitRate, int maxFps, List<CodecOption> codecOptions, String encoderName, boolean downsizeOnError) {
        this.maxSize = maxSize;
        this.streamer = streamer;
        this.videoBitRate = videoBitRate;
        this.maxFps = maxFps;
        this.codecOptions = codecOptions;
        this.encoderName = encoderName;
        this.downsizeOnError = downsizeOnError;

        cameraThread = new HandlerThread("camera");
        cameraThread.start();
        cameraHandler = new Handler(cameraThread.getLooper());
    }

    @SuppressLint("MissingPermission")
    private CameraDevice openCamera(CameraManager manager, String id) throws CameraAccessException, InterruptedException {
        Semaphore semaphore = new Semaphore(0);
        final CameraDevice[] result = new CameraDevice[1];
        manager.openCamera(id, new CameraDevice.StateCallback() {
            @Override
            public void onOpened(CameraDevice camera) {
                result[0] = camera;
                semaphore.release();
            }

            @Override
            public void onDisconnected(CameraDevice camera) {

            }

            @Override
            public void onError(CameraDevice camera, int error) {

            }
        }, cameraHandler);
        semaphore.acquire();
        return result[0];
    }

    private CameraCaptureSession createCaptureSession(CameraDevice camera, Surface surface) throws CameraAccessException, InterruptedException {
        Semaphore semaphore = new Semaphore(0);
        final CameraCaptureSession[] result = new CameraCaptureSession[1];
        camera.createCaptureSession(Collections.singletonList(surface), new CameraCaptureSession.StateCallback() {
            @Override
            public void onConfigured(CameraCaptureSession session) {
                result[0] = session;
                semaphore.release();
            }

            @Override
            public void onConfigureFailed(CameraCaptureSession session) {

            }
        }, cameraHandler);
        semaphore.acquire();
        return result[0];
    }

    @SuppressLint("MissingPermission")
    private void streamCamera() throws ConfigurationException, IOException {
        Looper.prepare();

        Codec codec = streamer.getCodec();
        MediaCodec mediaCodec = createMediaCodec(codec, encoderName);
        MediaFormat format = createFormat(codec.getMimeType(), videoBitRate, maxFps, codecOptions);

        try {
            CameraManager manager = (CameraManager) Workarounds.getSystemContext().getSystemService(Context.CAMERA_SERVICE);
            String[] ids = manager.getCameraIdList();

            String backCameraId = null;
            CameraCharacteristics backCameraCharacteristics = null;
            for (String id : ids) {
                CameraCharacteristics characteristics = manager.getCameraCharacteristics(id);
                Integer facing = characteristics.get(CameraCharacteristics.LENS_FACING);
                String facingName = null;
                switch (facing) {
                    case CameraCharacteristics.LENS_FACING_BACK:
                        facingName = "Back";
                        backCameraId = id;
                        backCameraCharacteristics = characteristics;
                        break;
                    case CameraCharacteristics.LENS_FACING_FRONT:
                        facingName = "Front";
                        break;
                    case CameraCharacteristics.LENS_FACING_EXTERNAL:
                        facingName = "External";
                        break;
                }
                Ln.i("Camera " + id + ", Facing: " + facingName);
            }

            if (backCameraId == null) {
                Ln.e("No back facing camera found.");
                return;
            }

            CameraDevice camera = openCamera(manager, backCameraId);

            StreamConfigurationMap map = backCameraCharacteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
            android.util.Size[] sizes = Arrays.stream(map.getOutputSizes(MediaCodec.class)).sorted((a, b) -> b.getWidth() - a.getWidth()).toArray(android.util.Size[]::new);

            for (android.util.Size size : sizes) {
                Ln.i("Supported Size: " + size.getWidth() + "x" + size.getHeight());
            }

            streamer.writeVideoHeader(new Size(sizes[0].getWidth(), sizes[0].getHeight()));

            boolean alive;
            try {
                do {
                    android.util.Size selectedSize = null;
                    if (maxSize == 0) {
                        selectedSize = sizes[0];
                    } else {
                        for (android.util.Size size : sizes) {
                            if (size.getWidth() < maxSize && size.getHeight() < maxSize) {
                                selectedSize = size;
                                break;
                            }
                        }
                        if (selectedSize == null) {
                            selectedSize = sizes[sizes.length - 1];
                        }
                    }

                    Ln.i("Selected Size: " + selectedSize.getWidth() + "x" + selectedSize.getHeight());

                    format.setInteger(MediaFormat.KEY_WIDTH, selectedSize.getWidth());
                    format.setInteger(MediaFormat.KEY_HEIGHT, selectedSize.getHeight());

                    Surface surface = null;
                    try {
                        mediaCodec.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
                        surface = mediaCodec.createInputSurface();

                        CameraCaptureSession session = createCaptureSession(camera, surface);
                        CaptureRequest.Builder requestBuilder = camera.createCaptureRequest(CameraDevice.TEMPLATE_RECORD);
                        requestBuilder.addTarget(surface);
                        CaptureRequest request = requestBuilder.build();
                        session.setRepeatingRequest(request, null, cameraHandler);

                        mediaCodec.start();

                        alive = encode(mediaCodec, streamer);
                        // do not call stop() on exception, it would trigger an IllegalStateException
                        mediaCodec.stop();
                    } catch (IllegalStateException | IllegalArgumentException e) {
                        Ln.e("Encoding error: " + e.getClass().getName() + ": " + e.getMessage());
                        if (!prepareRetry(new Size(selectedSize.getWidth(), selectedSize.getHeight()))) {
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
                camera.close();
            }
        } catch (ReflectiveOperationException | CameraAccessException | InterruptedException e) {
            Ln.e("Can't open camera.", e);
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
        Ln.i("newMaxSize = " + newMaxSize);
        if (newMaxSize == 0) {
            // Must definitively fail
            return false;
        }

        // Retry with a smaller device size
        Ln.i("Retrying with -m" + newMaxSize + "...");
        maxSize = newMaxSize;
        return true;
    }

    private int chooseMaxSizeFallback(Size failedSize) {
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

        while (!eof) {
            if (stopped.get()) {
                alive = false;
                break;
            }
            int outputBufferId = codec.dequeueOutputBuffer(bufferInfo, -1);
            try {
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


    @Override
    public void start(TerminationListener listener) {
        thread = new Thread(() -> {
            try {
                streamCamera();
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
