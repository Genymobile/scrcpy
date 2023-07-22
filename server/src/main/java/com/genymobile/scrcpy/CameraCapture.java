package com.genymobile.scrcpy;

import com.genymobile.scrcpy.wrappers.ServiceManager;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CaptureFailure;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.params.OutputConfiguration;
import android.hardware.camera2.params.SessionConfiguration;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.view.Surface;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Executor;
import java.util.concurrent.atomic.AtomicBoolean;

public class CameraCapture extends SurfaceCapture {

    private final String explicitCameraId;
    private final CameraFacing cameraFacing;
    private final Size explicitSize;

    private HandlerThread cameraThread;
    private Handler cameraHandler;
    private CameraDevice cameraDevice;
    private Executor cameraExecutor;

    private final AtomicBoolean disconnected = new AtomicBoolean();

    public CameraCapture(String explicitCameraId, CameraFacing cameraFacing, Size explicitSize) {
        this.explicitCameraId = explicitCameraId;
        this.cameraFacing = cameraFacing;
        this.explicitSize = explicitSize;
    }

    @Override
    public void init() throws IOException {
        cameraThread = new HandlerThread("camera");
        cameraThread.start();
        cameraHandler = new Handler(cameraThread.getLooper());
        cameraExecutor = new HandlerExecutor(cameraHandler);

        try {
            String cameraId = selectCamera(explicitCameraId, cameraFacing);
            if (cameraId == null) {
                throw new IOException("No matching camera found");
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
        return explicitSize;
    }

    @Override
    public boolean setMaxSize(int maxSize) {
        return false;
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
        SessionConfiguration sessionConfig = new SessionConfiguration(SessionConfiguration.SESSION_REGULAR, outputs, cameraExecutor,
                new CameraCaptureSession.StateCallback() {
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
        return requestBuilder.build();
    }

    @TargetApi(Build.VERSION_CODES.S)
    private void setRepeatingRequest(CameraCaptureSession session, CaptureRequest request) throws CameraAccessException, InterruptedException {
        session.setRepeatingRequest(request, new CameraCaptureSession.CaptureCallback() {
            @Override
            public void onCaptureStarted(CameraCaptureSession session, CaptureRequest request, long timestamp, long frameNumber) {
                // Called for each frame captured, do nothing
            }

            @Override
            public void onCaptureFailed(CameraCaptureSession session, CaptureRequest request, CaptureFailure failure) {
                Ln.w("Camera capture failed: frame " + failure.getFrameNumber());
            }
        }, cameraHandler);
    }

    @Override
    public boolean isClosed() {
        return disconnected.get();
    }
}
