package com.genymobile.scrcpy.util;

import com.genymobile.scrcpy.AndroidVersions;
import com.genymobile.scrcpy.audio.AudioCodec;
import com.genymobile.scrcpy.device.Device;
import com.genymobile.scrcpy.device.DeviceApp;
import com.genymobile.scrcpy.device.DisplayInfo;
import com.genymobile.scrcpy.device.Size;
import com.genymobile.scrcpy.video.VideoCodec;
import com.genymobile.scrcpy.wrappers.DisplayManager;
import com.genymobile.scrcpy.wrappers.ServiceManager;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.graphics.Rect;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.os.Build;
import android.util.Range;

import java.util.Collections;
import java.util.List;
import java.util.Objects;
import java.util.SortedSet;
import java.util.TreeSet;

public final class LogUtils {

    private LogUtils() {
        // not instantiable
    }

    private static String buildEncoderListMessage(String type, Codec[] codecs) {
        StringBuilder builder = new StringBuilder("List of ").append(type).append(" encoders:");
        MediaCodecList codecList = new MediaCodecList(MediaCodecList.REGULAR_CODECS);
        for (Codec codec : codecs) {
            MediaCodecInfo[] encoders = CodecUtils.getEncoders(codecList, codec.getMimeType());
            for (MediaCodecInfo info : encoders) {
                int lineStart = builder.length();
                builder.append("\n    --").append(type).append("-codec=").append(codec.getName());
                builder.append(" --").append(type).append("-encoder=").append(info.getName());
                if (Build.VERSION.SDK_INT >= AndroidVersions.API_29_ANDROID_10) {
                    int lineLength = builder.length() - lineStart;
                    final int column = 70;
                    if (lineLength < column) {
                        int padding = column - lineLength;
                        builder.append(String.format("%" + padding + "s", " "));
                    }
                    builder.append(" (").append(getHwCodecType(info)).append(')');
                    if (info.isVendor()) {
                        builder.append(" [vendor]");
                    }
                    if (info.isAlias()) {
                        builder.append(" (alias for ").append(info.getCanonicalName()).append(')');
                    }
                }

            }
        }

        return builder.toString();
    }

    public static String buildVideoEncoderListMessage() {
        return buildEncoderListMessage("video", VideoCodec.values());
    }

    public static String buildAudioEncoderListMessage() {
        return buildEncoderListMessage("audio", AudioCodec.values());
    }

    @TargetApi(AndroidVersions.API_29_ANDROID_10)
    private static String getHwCodecType(MediaCodecInfo info) {
        if (info.isSoftwareOnly()) {
            return "sw";
        }
        if (info.isHardwareAccelerated()) {
            return "hw";
        }
        return "hybrid";
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

    private static boolean isCameraBackwardCompatible(CameraCharacteristics characteristics) {
        int[] capabilities = characteristics.get(CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES);
        if (capabilities == null) {
            return false;
        }

        for (int capability : capabilities) {
            if (capability == CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_BACKWARD_COMPATIBLE) {
                return true;
            }
        }

        return false;
    }

    public static String buildCameraListMessage(boolean includeSizes) {
        StringBuilder builder = new StringBuilder("List of cameras:");
        CameraManager cameraManager = ServiceManager.getCameraManager();
        try {
            String[] cameraIds = cameraManager.getCameraIdList();
            if (cameraIds.length == 0) {
                builder.append("\n    (none)");
            } else {
                for (String id : cameraIds) {
                    CameraCharacteristics characteristics = cameraManager.getCameraCharacteristics(id);

                    if (!isCameraBackwardCompatible(characteristics)) {
                        // Ignore depth cameras as suggested by official documentation
                        // <https://developer.android.com/media/camera/camera2/camera-enumeration>
                        continue;
                    }

                    builder.append("\n    --camera-id=").append(id);

                    int facing = characteristics.get(CameraCharacteristics.LENS_FACING);
                    builder.append("    (").append(getCameraFacingName(facing)).append(", ");

                    Rect activeSize = characteristics.get(CameraCharacteristics.SENSOR_INFO_ACTIVE_ARRAY_SIZE);
                    builder.append(activeSize.width()).append("x").append(activeSize.height());

                    try {
                        // Capture frame rates for low-FPS mode are the same for every resolution
                        Range<Integer>[] lowFpsRanges = characteristics.get(CameraCharacteristics.CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES);
                        if (lowFpsRanges != null) {
                            SortedSet<Integer> uniqueLowFps = getUniqueSet(lowFpsRanges);
                            builder.append(", fps=").append(uniqueLowFps);
                        }
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


    public static String buildAppListMessage() {
        List<DeviceApp> apps = Device.listApps();
        return buildAppListMessage("List of apps:", apps);
    }

    @SuppressLint("QueryPermissionsNeeded")
    public static String buildAppListMessage(String title, List<DeviceApp> apps) {
        StringBuilder builder = new StringBuilder(title);

        // Sort by:
        //  1. system flag (system apps are before non-system apps)
        //  2. name
        //  3. package name
        // Comparator.comparing() was introduced in API 24, so it cannot be used here to simplify the code
        Collections.sort(apps, (thisApp, otherApp) -> {
            // System apps first
            int cmp = -Boolean.compare(thisApp.isSystem(), otherApp.isSystem());
            if (cmp != 0) {
                return cmp;
            }

            cmp = Objects.compare(thisApp.getName(), otherApp.getName(), String::compareTo);
            if (cmp != 0) {
                return cmp;
            }

            return Objects.compare(thisApp.getPackageName(), otherApp.getPackageName(), String::compareTo);
        });

        final int column = 30;
        for (DeviceApp app : apps) {
            String name = app.getName();
            int padding = column - name.length();
            builder.append("\n ");
            if (app.isSystem()) {
                builder.append("* ");
            } else {
                builder.append("- ");

            }
            builder.append(name);
            if (padding > 0) {
                builder.append(String.format("%" + padding + "s", " "));
            } else {
                builder.append("\n   ").append(String.format("%" + column + "s", " "));
            }
            builder.append(" ").append(app.getPackageName());
        }

        return builder.toString();
    }
}
