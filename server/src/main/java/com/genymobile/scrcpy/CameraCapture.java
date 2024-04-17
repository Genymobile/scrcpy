package com.genymobile.scrcpy;

import com.genymobile.scrcpy.wrappers.ServiceManager;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.graphics.Rect;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraConstrainedHighSpeedCaptureSession;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CaptureFailure;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.params.OutputConfiguration;
import android.hardware.camera2.params.SessionConfiguration;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.MediaCodec;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Range;
import android.view.Surface;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;
import java.util.Optional;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Executor;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.stream.Stream;

public class CameraCapture extends SurfaceCapture {

    private final String explicitCameraId;
    private final CameraFacing cameraFacing;
    private final Size explicitSize;
    private int maxSize;
    private final CameraAspectRatio aspectRatio;
    private final int fps;
    private final boolean highSpeed;

    private String cameraId;
    private Size size;

    private HandlerThread cameraThread;
    private Handler cameraHandler;
    private CameraDevice cameraDevice;
    private Executor cameraExecutor;

    private final AtomicBoolean disconnected = new AtomicBoolean();

    public CameraCapture(String explicitCameraId, CameraFacing cameraFacing, Size explicitSize, int maxSize, CameraAspectRatio aspectRatio, int fps,
            boolean highSpeed) {
        this.explicitCameraId = explicitCameraId;
        this.cameraFacing = cameraFacing;
        this.explicitSize = explicitSize;
        this.maxSize = maxSize;
        this.aspectRatio = aspectRatio;
        this.fps = fps;
        this.highSpeed = highSpeed;
    }

    @Override
    public void init() throws IOException {
        cameraThread = new HandlerThread("camera");
        cameraThread.start();
        cameraHandler = new Handler(cameraThread.getLooper());
        cameraExecutor = new HandlerExecutor(cameraHandler);

        try {
            cameraId = selectCamera(explicitCameraId, cameraFacing);
            if (cameraId == null) {
                throw new IOException("No matching camera found");
            }

            size = selectSize(cameraId, explicitSize, maxSize, aspectRatio, highSpeed);
            if (size == null) {
                throw new IOException("Could not select camera size");
            }

            Ln.i("Using camera '" + cameraId + "'");
            cameraDevice = openCamera(cameraId);
        } catch (CameraAccessException | InterruptedException e) {
            throw new IOException(e);
        }
    }

    private static String selectCamera(String explicitCameraId, CameraFacing cameraFacing) throws CameraAccessException {
        if (explicitCameraId != null) {
            return explicitCameraId;
        }

        CameraManager cameraManager = ServiceManager.getCameraManager();

        String[] cameraIds = cameraManager.getCameraIdList();
        if (cameraFacing == null) {
            // Use the first one
            return cameraIds.length > 0 ? cameraIds[0] : null;
        }

        for (String cameraId : cameraIds) {
            CameraCharacteristics characteristics = cameraManager.getCameraCharacteristics(cameraId);

            int facing = characteristics.get(CameraCharacteristics.LENS_FACING);
            if (cameraFacing.value() == facing) {
                return cameraId;
            }
        }

        // Not found
        return null;
    }

    @TargetApi(Build.VERSION_CODES.N)
    private static Size selectSize(String cameraId, Size explicitSize, int maxSize, CameraAspectRatio aspectRatio, boolean highSpeed)
            throws CameraAccessException {
        if (explicitSize != null) {
            return explicitSize;
        }

        CameraManager cameraManager = ServiceManager.getCameraManager();
        CameraCharacteristics characteristics = cameraManager.getCameraCharacteristics(cameraId);

        StreamConfigurationMap configs = characteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
        android.util.Size[] sizes = highSpeed ? configs.getHighSpeedVideoSizes() : configs.getOutputSizes(MediaCodec.class);
        if (sizes == null) {
            return null;
        }

        Stream<android.util.Size> stream = Arrays.stream(sizes);
        if (maxSize > 0) {
            stream = stream.filter(it -> it.getWidth() <= maxSize && it.getHeight() <= maxSize);
        }

        Float targetAspectRatio = resolveAspectRatio(aspectRatio, characteristics);
        if (targetAspectRatio != null) {
            stream = stream.filter(it -> {
                float ar = ((float) it.getWidth() / it.getHeight());
                float arRatio = ar / targetAspectRatio;
                // Accept if the aspect ratio is the target aspect ratio + or - 10%
                return arRatio >= 0.9f && arRatio <= 1.1f;
            });
        }

        Optional<android.util.Size> selected = stream.max((s1, s2) -> {
            // Greater width is better
            int cmp = Integer.compare(s1.getWidth(), s2.getWidth());
            if (cmp != 0) {
                return cmp;
            }

            if (targetAspectRatio != null) {
                // Closer to the target aspect ratio is better
                float ar1 = ((float) s1.getWidth() / s1.getHeight());
                float arRatio1 = ar1 / targetAspectRatio;
                float distance1 = Math.abs(1 - arRatio1);

                float ar2 = ((float) s2.getWidth() / s2.getHeight());
                float arRatio2 = ar2 / targetAspectRatio;
                float distance2 = Math.abs(1 - arRatio2);

                // Reverse the order because lower distance is better
                cmp = Float.compare(distance2, distance1);
                if (cmp != 0) {
                    return cmp;
                }
            }

            // Greater height is better
            return Integer.compare(s1.getHeight(), s2.getHeight());
        });

        if (selected.isPresent()) {
            android.util.Size size = selected.get();
            return new Size(size.getWidth(), size.getHeight());
        }

        // Not found
        return null;
    }

    private static Float resolveAspectRatio(CameraAspectRatio ratio, CameraCharacteristics characteristics) {
        if (ratio == null) {
            return null;
        }

        if (ratio.isSensor()) {
            Rect activeSize = characteristics.get(CameraCharacteristics.SENSOR_INFO_ACTIVE_ARRAY_SIZE);
            return (float) activeSize.width() / activeSize.height();
        }

        return ratio.getAspectRatio();
    }

    @Override
    public void start(Surface surface) throws IOException {
        try {
            CameraCaptureSession session = createCaptureSession(cameraDevice, surface);
            CaptureRequest request = createCaptureRequest(surface);
            setRepeatingRequest(session, request);
        } catch (CameraAccessException | InterruptedException e) {
            throw new IOException(e);
        }
    }

    @Override
    public void release() {
        if (cameraDevice != null) {
            cameraDevice.close();
        }
        if (cameraThread != null) {
            cameraThread.quitSafely();
        }
    }

    @Override
    public Size getSize() {
        return size;
    }

    @Override
    public boolean setMaxSize(int maxSize) {
        if (explicitSize != null) {
            return false;
        }

        this.maxSize = maxSize;
        try {
            size = selectSize(cameraId, null, maxSize, aspectRatio, highSpeed);
            return size != null;
        } catch (CameraAccessException e) {
            Ln.w("Could not select camera size", e);
            return false;
        }
    }

    @SuppressLint("MissingPermission")
    @TargetApi(Build.VERSION_CODES.S)
    private CameraDevice openCamera(String id) throws CameraAccessException, InterruptedException {
        CompletableFuture<CameraDevice> future = new CompletableFuture<>();
        ServiceManager.getCameraManager().openCamera(id, new CameraDevice.StateCallback() {
            @Override
            public void onOpened(CameraDevice camera) {
                Ln.d("Camera opened successfully");
                future.complete(camera);
            }

            @Override
            public void onDisconnected(CameraDevice camera) {
                Ln.w("Camera disconnected");
                disconnected.set(true);
                requestReset();
            }

            @Override
            public void onError(CameraDevice camera, int error) {
                int cameraAccessExceptionErrorCode;
                switch (error) {
                    case CameraDevice.StateCallback.ERROR_CAMERA_IN_USE:
                        cameraAccessExceptionErrorCode = CameraAccessException.CAMERA_IN_USE;
                        break;
                    case CameraDevice.StateCallback.ERROR_MAX_CAMERAS_IN_USE:
                        cameraAccessExceptionErrorCode = CameraAccessException.MAX_CAMERAS_IN_USE;
                        break;
                    case CameraDevice.StateCallback.ERROR_CAMERA_DISABLED:
                        cameraAccessExceptionErrorCode = CameraAccessException.CAMERA_DISABLED;
                        break;
                    case CameraDevice.StateCallback.ERROR_CAMERA_DEVICE:
                    case CameraDevice.StateCallback.ERROR_CAMERA_SERVICE:
                    default:
                        cameraAccessExceptionErrorCode = CameraAccessException.CAMERA_ERROR;
                        break;
                }
                future.completeExceptionally(new CameraAccessException(cameraAccessExceptionErrorCode));
            }
        }, cameraHandler);

        try {
            return future.get();
        } catch (ExecutionException e) {
            throw (CameraAccessException) e.getCause();
        }
    }

    @TargetApi(Build.VERSION_CODES.S)
    private CameraCaptureSession createCaptureSession(CameraDevice camera, Surface surface) throws CameraAccessException, InterruptedException {
        CompletableFuture<CameraCaptureSession> future = new CompletableFuture<>();
        OutputConfiguration outputConfig = new OutputConfiguration(surface);
        List<OutputConfiguration> outputs = Arrays.asList(outputConfig);

        int sessionType = highSpeed ? SessionConfiguration.SESSION_HIGH_SPEED : SessionConfiguration.SESSION_REGULAR;
        SessionConfiguration sessionConfig = new SessionConfiguration(sessionType, outputs, cameraExecutor, new CameraCaptureSession.StateCallback() {
            @Override
            public void onConfigured(CameraCaptureSession session) {
                future.complete(session);
            }

            @Override
            public void onConfigureFailed(CameraCaptureSession session) {
                future.completeExceptionally(new CameraAccessException(CameraAccessException.CAMERA_ERROR));
            }
        });

        camera.createCaptureSession(sessionConfig);

        try {
            return future.get();
        } catch (ExecutionException e) {
            throw (CameraAccessException) e.getCause();
        }
    }

    private CaptureRequest createCaptureRequest(Surface surface) throws CameraAccessException {
        CaptureRequest.Builder requestBuilder = cameraDevice.createCaptureRequest(CameraDevice.TEMPLATE_RECORD);
        requestBuilder.addTarget(surface);

        if (fps > 0) {
            requestBuilder.set(CaptureRequest.CONTROL_AE_TARGET_FPS_RANGE, new Range<>(fps, fps));
        }

        return requestBuilder.build();
    }

    @TargetApi(Build.VERSION_CODES.S)
    private void setRepeatingRequest(CameraCaptureSession session, CaptureRequest request) throws CameraAccessException, InterruptedException {
        CameraCaptureSession.CaptureCallback callback = new CameraCaptureSession.CaptureCallback() {
            @Override
            public void onCaptureStarted(CameraCaptureSession session, CaptureRequest request, long timestamp, long frameNumber) {
                // Called for each frame captured, do nothing
            }

            @Override
            public void onCaptureFailed(CameraCaptureSession session, CaptureRequest request, CaptureFailure failure) {
                Ln.w("Camera capture failed: frame " + failure.getFrameNumber());
            }
        };

        if (highSpeed) {
            CameraConstrainedHighSpeedCaptureSession highSpeedSession = (CameraConstrainedHighSpeedCaptureSession) session;
            List<CaptureRequest> requests = highSpeedSession.createHighSpeedRequestList(request);
            highSpeedSession.setRepeatingBurst(requests, callback, cameraHandler);
        } else {
            session.setRepeatingRequest(request, callback, cameraHandler);
        }
    }

    @Override
    public boolean isClosed() {
        return disconnected.get();
    }
}
