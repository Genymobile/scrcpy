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
import android.os.Handler;
import android.os.HandlerThread;
import android.view.Surface;

import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.concurrent.Semaphore;

public class CameraEncoder extends SurfaceEncoder {

    private int maxSize;
    private String cameraId;

    private CameraDevice cameraDevice;

    private CameraManager cameraManager;
    private HandlerThread cameraThread;
    private Handler cameraHandler;

    public CameraEncoder(int maxSize, String cameraId, Streamer streamer, int videoBitRate, int maxFps,
            List<CodecOption> codecOptions, String encoderName, boolean downsizeOnError) {
        super(streamer, videoBitRate, maxFps, codecOptions, encoderName, downsizeOnError);

        this.maxSize = maxSize;
        this.cameraId = cameraId;

        cameraThread = new HandlerThread("camera");
        cameraThread.start();
        cameraHandler = new Handler(cameraThread.getLooper());
    }

    @SuppressLint("MissingPermission")
    private CameraDevice openCamera(String id)
            throws CameraAccessException, InterruptedException {
        Semaphore semaphore = new Semaphore(0);
        final CameraDevice[] result = new CameraDevice[1];
        cameraManager.openCamera(id, new CameraDevice.StateCallback() {
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

    @SuppressWarnings("deprecation")
    private CameraCaptureSession createCaptureSession(CameraDevice camera, Surface surface)
            throws CameraAccessException, InterruptedException {
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

    @Override
    protected void initialize() {
        try {
            Looper.prepare();
            cameraManager = (CameraManager) Workarounds.getSystemContext()
                    .getSystemService(Context.CAMERA_SERVICE);
        } catch (Exception e) {
            throw new RuntimeException("Can't access camera", e);
        }
    }

    @Override
    protected Size getSize() throws ConfigurationException {
        try {
            if (cameraId != null) {
                cameraDevice = openCamera(cameraId);
            } else {
                String[] cameraIds = cameraManager.getCameraIdList();
                cameraDevice = openCamera(cameraIds[0]);
            }

            CameraCharacteristics characteristics = cameraManager.getCameraCharacteristics(cameraDevice.getId());
            StreamConfigurationMap map = characteristics
                    .get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
            android.util.Size[] sizes = Arrays.stream(map.getOutputSizes(MediaCodec.class))
                    .sorted(Comparator.comparing(android.util.Size::getWidth).reversed())
                    .toArray(android.util.Size[]::new);

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

            return new Size(selectedSize.getWidth(), selectedSize.getHeight());
        } catch (IllegalArgumentException e) {
            Ln.e("Camera " + cameraId + " not found\n" + LogUtils.buildCameraListMessage());
            throw new ConfigurationException("Unknown camera id: " + cameraId);
        } catch (Exception e) {
            throw new RuntimeException("Can't access camera", e);
        }
    }

    @Override
    protected void setSize(int size) {
        maxSize = size;
    }

    @Override
    protected void setSurface(Surface surface) {
        try {
            CameraCaptureSession session = createCaptureSession(cameraDevice, surface);
            CaptureRequest.Builder requestBuilder = cameraDevice
                    .createCaptureRequest(CameraDevice.TEMPLATE_RECORD);
            requestBuilder.addTarget(surface);
            CaptureRequest request = requestBuilder.build();
            session.setRepeatingRequest(request, null, cameraHandler);
        } catch (Exception e) {
            throw new RuntimeException("Can't access camera", e);
        }
    }

    @Override
    protected void dispose() {
        if (cameraDevice != null) {
            cameraDevice.close();
        }
    }
}
