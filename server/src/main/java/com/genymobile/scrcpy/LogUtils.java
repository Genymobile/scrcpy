package com.genymobile.scrcpy;

import com.genymobile.scrcpy.wrappers.DisplayManager;
import com.genymobile.scrcpy.wrappers.ServiceManager;

import android.graphics.Rect;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.MediaCodec;
import android.util.Range;

import java.util.List;
import java.util.SortedSet;
import java.util.TreeSet;

public final class LogUtils {

    private LogUtils() {
        // not instantiable
    }

    public static String buildVideoEncoderListMessage() {
        StringBuilder builder = new StringBuilder("List of video encoders:");
        List<CodecUtils.DeviceEncoder> videoEncoders = CodecUtils.listVideoEncoders();
        if (videoEncoders.isEmpty()) {
            builder.append("\n    (none)");
        } else {
            for (CodecUtils.DeviceEncoder encoder : videoEncoders) {
                builder.append("\n    --video-codec=").append(encoder.getCodec().getName());
                builder.append(" --video-encoder='").append(encoder.getInfo().getName()).append("'");
            }
        }
        return builder.toString();
    }

    public static String buildAudioEncoderListMessage() {
        StringBuilder builder = new StringBuilder("List of audio encoders:");
        List<CodecUtils.DeviceEncoder> audioEncoders = CodecUtils.listAudioEncoders();
        if (audioEncoders.isEmpty()) {
            builder.append("\n    (none)");
        } else {
            for (CodecUtils.DeviceEncoder encoder : audioEncoders) {
                builder.append("\n    --audio-codec=").append(encoder.getCodec().getName());
                builder.append(" --audio-encoder='").append(encoder.getInfo().getName()).append("'");
            }
        }
        return builder.toString();
    }

    public static String buildDisplayListMessage() {
        StringBuilder builder = new StringBuilder("List of displays:");
        DisplayManager displayManager = ServiceManager.getDisplayManager();
        int[] displayIds = displayManager.getDisplayIds();
        if (displayIds == null || displayIds.length == 0) {
            builder.append("\n    (none)");
        } else {
            for (int id : displayIds) {
                builder.append("\n    --display-id=").append(id).append("    (");
                DisplayInfo displayInfo = displayManager.getDisplayInfo(id);
                if (displayInfo != null) {
                    Size size = displayInfo.getSize();
                    builder.append(size.getWidth()).append("x").append(size.getHeight());
                } else {
                    builder.append("size unknown");
                }
                builder.append(")");
            }
        }
        return builder.toString();
    }

    private static String getCameraFacingName(int facing) {
        switch (facing) {
            case CameraCharacteristics.LENS_FACING_FRONT:
                return "front";
            case CameraCharacteristics.LENS_FACING_BACK:
                return "back";
            case CameraCharacteristics.LENS_FACING_EXTERNAL:
                return "external";
            default:
                return "unknown";
        }
    }

    public static String buildCameraListMessage(boolean includeSizes) {
        StringBuilder builder = new StringBuilder("List of cameras:");
        CameraManager cameraManager = ServiceManager.getCameraManager();
        try {
            String[] cameraIds = cameraManager.getCameraIdList();
            if (cameraIds == null || cameraIds.length == 0) {
                builder.append("\n    (none)");
            } else {
                for (String id : cameraIds) {
                    builder.append("\n    --camera-id=").append(id);
                    CameraCharacteristics characteristics = cameraManager.getCameraCharacteristics(id);

                    int facing = characteristics.get(CameraCharacteristics.LENS_FACING);
                    builder.append("    (").append(getCameraFacingName(facing)).append(", ");

                    Rect activeSize = characteristics.get(CameraCharacteristics.SENSOR_INFO_ACTIVE_ARRAY_SIZE);
                    builder.append(activeSize.width()).append("x").append(activeSize.height());

                    try {
                        // Capture frame rates for low-FPS mode are the same for every resolution
                        Range<Integer>[] lowFpsRanges = characteristics.get(CameraCharacteristics.CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES);
                        SortedSet<Integer> uniqueLowFps = getUniqueSet(lowFpsRanges);
                        builder.append(", fps=").append(uniqueLowFps);
                    } catch (Exception e) {
                        // Some devices may provide invalid ranges, causing an IllegalArgumentException "lower must be less than or equal to upper"
                        Ln.w("Could not get available frame rates for camera " + id, e);
                    }

                    builder.append(')');

                    if (includeSizes) {
                        StreamConfigurationMap configs = characteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);

                        android.util.Size[] sizes = configs.getOutputSizes(MediaCodec.class);
                        if (sizes == null || sizes.length == 0) {
                            builder.append("\n        (none)");
                        } else {
                            for (android.util.Size size : sizes) {
                                builder.append("\n        - ").append(size.getWidth()).append('x').append(size.getHeight());
                            }
                        }

                        android.util.Size[] highSpeedSizes = configs.getHighSpeedVideoSizes();
                        if (highSpeedSizes != null && highSpeedSizes.length > 0) {
                            builder.append("\n      High speed capture (--camera-high-speed):");
                            for (android.util.Size size : highSpeedSizes) {
                                Range<Integer>[] highFpsRanges = configs.getHighSpeedVideoFpsRanges();
                                SortedSet<Integer> uniqueHighFps = getUniqueSet(highFpsRanges);
                                builder.append("\n        - ").append(size.getWidth()).append("x").append(size.getHeight());
                                builder.append(" (fps=").append(uniqueHighFps).append(')');
                            }
                        }
                    }
                }
            }
        } catch (CameraAccessException e) {
            builder.append("\n    (access denied)");
        }
        return builder.toString();
    }

    private static SortedSet<Integer> getUniqueSet(Range<Integer>[] ranges) {
        SortedSet<Integer> set = new TreeSet<>();
        for (Range<Integer> range : ranges) {
            set.add(range.getUpper());
        }
        return set;
    }
}
