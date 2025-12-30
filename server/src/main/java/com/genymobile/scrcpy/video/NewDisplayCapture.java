package com.genymobile.scrcpy.video;

import com.genymobile.scrcpy.AndroidVersions;
import com.genymobile.scrcpy.Options;
import com.genymobile.scrcpy.control.PositionMapper;
import com.genymobile.scrcpy.device.DisplayInfo;
import com.genymobile.scrcpy.device.NewDisplay;
import com.genymobile.scrcpy.device.Orientation;
import com.genymobile.scrcpy.device.Size;
import com.genymobile.scrcpy.opengl.AffineOpenGLFilter;
import com.genymobile.scrcpy.opengl.OpenGLFilter;
import com.genymobile.scrcpy.opengl.OpenGLRunner;
import com.genymobile.scrcpy.util.AffineMatrix;
import com.genymobile.scrcpy.util.Ln;
import com.genymobile.scrcpy.wrappers.ServiceManager;
import com.genymobile.scrcpy.audio.AudioPlaybackCapture;

import android.graphics.Rect;
import android.hardware.display.VirtualDisplay;
import android.os.Build;
import android.view.Surface;

import java.io.IOException;

public class NewDisplayCapture extends SurfaceCapture {

    // Internal fields copied from android.hardware.display.DisplayManager
    private static final int VIRTUAL_DISPLAY_FLAG_PUBLIC = android.hardware.display.DisplayManager.VIRTUAL_DISPLAY_FLAG_PUBLIC;
    private static final int VIRTUAL_DISPLAY_FLAG_PRESENTATION = android.hardware.display.DisplayManager.VIRTUAL_DISPLAY_FLAG_PRESENTATION;
    private static final int VIRTUAL_DISPLAY_FLAG_OWN_CONTENT_ONLY = android.hardware.display.DisplayManager.VIRTUAL_DISPLAY_FLAG_OWN_CONTENT_ONLY;
    private static final int VIRTUAL_DISPLAY_FLAG_SUPPORTS_TOUCH = 1 << 6;
    private static final int VIRTUAL_DISPLAY_FLAG_ROTATES_WITH_CONTENT = 1 << 7;
    private static final int VIRTUAL_DISPLAY_FLAG_DESTROY_CONTENT_ON_REMOVAL = 1 << 8;
    private static final int VIRTUAL_DISPLAY_FLAG_SHOULD_SHOW_SYSTEM_DECORATIONS = 1 << 9;
    private static final int VIRTUAL_DISPLAY_FLAG_TRUSTED = 1 << 10;
    private static final int VIRTUAL_DISPLAY_FLAG_OWN_DISPLAY_GROUP = 1 << 11;
    private static final int VIRTUAL_DISPLAY_FLAG_ALWAYS_UNLOCKED = 1 << 12;
    private static final int VIRTUAL_DISPLAY_FLAG_TOUCH_FEEDBACK_DISABLED = 1 << 13;
    private static final int VIRTUAL_DISPLAY_FLAG_OWN_FOCUS = 1 << 14;
    private static final int VIRTUAL_DISPLAY_FLAG_DEVICE_DISPLAY_GROUP = 1 << 15;

    private final VirtualDisplayListener vdListener;
    private final NewDisplay newDisplay;

    private final DisplaySizeMonitor displaySizeMonitor = new DisplaySizeMonitor();

    private AffineMatrix displayTransform;
    private AffineMatrix eventTransform;
    private OpenGLRunner glRunner;

    private Size mainDisplaySize;
    private int mainDisplayDpi;
    private int maxSize;
    private int displayImePolicy;
    private final Rect crop;
    private final boolean captureOrientationLocked;
    private final Orientation captureOrientation;
    private final float angle;
    private final boolean vdDestroyContent;

    private final boolean vdSystemDecorations;
    private final java.util.List<String> audioIgnoreApps;
    private final java.util.List<String> audioFilterApps;

    private VirtualDisplay virtualDisplay;
    private Size videoSize;
    private Size displaySize; // the logical size of the display (including rotation)
    private Size physicalSize; // the physical size of the display (without rotation)

    private int dpi;

    public NewDisplayCapture(VirtualDisplayListener vdListener, Options options) {
        this.vdListener = vdListener;
        this.newDisplay = options.getNewDisplay();
        assert newDisplay != null;
        this.maxSize = options.getMaxSize();
        this.displayImePolicy = options.getDisplayImePolicy();
        this.crop = options.getCrop();
        assert options.getCaptureOrientationLock() != null;
        this.captureOrientationLocked = options.getCaptureOrientationLock() != Orientation.Lock.Unlocked;
        this.captureOrientation = options.getCaptureOrientation();
        assert captureOrientation != null;
        this.angle = options.getAngle();
        this.vdDestroyContent = options.getVDDestroyContent();
        this.vdSystemDecorations = options.getVDSystemDecorations();
        this.audioIgnoreApps = options.getAudioIgnoreApps();
        this.audioFilterApps = options.getAudioFilterApps();
    }

    @Override
    protected void init() {
        displaySize = newDisplay.getSize();
        dpi = newDisplay.getDpi();
        if (displaySize == null || dpi == 0) {
            DisplayInfo displayInfo = ServiceManager.getDisplayManager().getDisplayInfo(0);
            if (displayInfo != null) {
                mainDisplaySize = displayInfo.getSize();
                if ((displayInfo.getRotation() % 2) != 0) {
                    mainDisplaySize = mainDisplaySize.rotate(); // Use the natural device orientation (at rotation 0), not the current one
                }
                mainDisplayDpi = displayInfo.getDpi();
            } else {
                Ln.w("Main display not found, fallback to 1920x1080 240dpi");
                mainDisplaySize = new Size(1920, 1080);
                mainDisplayDpi = 240;
            }
        }
    }

    @Override
    public void prepare() {
        int displayRotation;
        if (virtualDisplay == null) {
            if (!newDisplay.hasExplicitSize()) {
                displaySize = mainDisplaySize;
            }
            if (!newDisplay.hasExplicitDpi()) {
                dpi = scaleDpi(mainDisplaySize, mainDisplayDpi, displaySize);
            }

            videoSize = displaySize;
            displayRotation = 0;
            // Set the current display size to avoid an unnecessary call to invalidate()
            displaySizeMonitor.setSessionDisplaySize(displaySize);
        } else {
            DisplayInfo displayInfo = ServiceManager.getDisplayManager().getDisplayInfo(virtualDisplay.getDisplay().getDisplayId());
            displaySize = displayInfo.getSize();
            dpi = displayInfo.getDpi();
            displayRotation = displayInfo.getRotation();
        }

        VideoFilter filter = new VideoFilter(displaySize);

        if (crop != null) {
            boolean transposed = (displayRotation % 2) != 0;
            filter.addCrop(crop, transposed);
        }

        filter.addOrientation(displayRotation, captureOrientationLocked, captureOrientation);
        filter.addAngle(angle);

        Size filteredSize = filter.getOutputSize();
        if (!filteredSize.isMultipleOf8() || (maxSize != 0 && filteredSize.getMax() > maxSize)) {
            if (maxSize != 0) {
                filteredSize = filteredSize.limit(maxSize);
            }
            filteredSize = filteredSize.round8();
            filter.addResize(filteredSize);
        }

        eventTransform = filter.getInverseTransform();

        // DisplayInfo gives the oriented size (so videoSize includes the display rotation)
        videoSize = filter.getOutputSize();

        // But the virtual display video always remains in the origin orientation (the video itself is not rotated, so it must rotated manually).
        // This additional display rotation must not be included in the input events transform (the expected coordinates are already in the
        // physical display size)
        if ((displayRotation % 2) == 0) {
            physicalSize = displaySize;
        } else {
            physicalSize = displaySize.rotate();
        }
        VideoFilter displayFilter = new VideoFilter(physicalSize);
        displayFilter.addRotation(displayRotation);
        AffineMatrix displayRotationMatrix = displayFilter.getInverseTransform();

        // Take care of multiplication order:
        //   displayTransform = (FILTER_MATRIX * DISPLAY_FILTER_MATRIX)⁻¹
        //                    = DISPLAY_FILTER_MATRIX⁻¹ * FILTER_MATRIX⁻¹
        //                    = displayRotationMatrix * eventTransform
        displayTransform = AffineMatrix.multiplyAll(displayRotationMatrix, eventTransform);
    }

    private AudioPlaybackCapture audioCapture;
    private Thread appMonitorThread;
    private volatile boolean monitorRunning;

    public void setAudioCapture(AudioPlaybackCapture audioCapture) {
        Ln.i("NewDisplayCapture: setAudioCapture called with " + audioCapture);
        this.audioCapture = audioCapture;
    }

    private void startAppMonitor(int displayId) {
        Ln.d("startAppMonitor called for displayId: " + displayId);
        if (audioCapture == null) {
            Ln.w("audioCapture is null in startAppMonitor, skipping monitor");
            return;
        }

        monitorRunning = true;
        appMonitorThread = new Thread(() -> {
            Ln.d("AppMonitor thread started");
            String lastPackageName = null;
            int lastUid = -2; // Force update on first detection
            com.genymobile.scrcpy.wrappers.ActivityManager am = ServiceManager.getActivityManager();
            com.genymobile.scrcpy.wrappers.PackageManager pm = ServiceManager.getPackageManager();

            while (monitorRunning) {
                try {
                    // Increase limit to 50 to ensure we find tasks even if many background tasks exist
                    java.util.List<android.app.ActivityManager.RunningTaskInfo> tasks = am.getRunningTasks(50);
                    boolean foundTopApp = false;
                    if (tasks != null) {
                        for (android.app.ActivityManager.RunningTaskInfo task : tasks) {
                            int taskDisplayId = getTaskDisplayId(task);
                            String paramPkg = task.topActivity != null ? task.topActivity.getPackageName() : "null";

                            if (audioFilterApps != null && audioFilterApps.contains(paramPkg)) {
                                continue;
                            }

                            if (taskDisplayId == displayId && task.topActivity != null) {
                                if (!foundTopApp) {
                                    // This is the FIRST task we found for this display -> It is the Top App
                                    foundTopApp = true;
                                    String packageName = paramPkg;
                                    if (!packageName.equals(lastPackageName)) {
                                        Ln.i("App changed on display " + displayId + ": " + packageName);
                                        lastPackageName = packageName;

                                        int newUid = -1;
                                        if (audioIgnoreApps != null && audioIgnoreApps.contains(packageName)) {
                                            Ln.i("App ignored (forcing global audio): " + packageName);
                                            newUid = -1;
                                        } else {
                                            // Resolve UID
                                            try {
                                                android.content.pm.ApplicationInfo info = pm.getApplicationInfo(packageName, 0, 0);
                                                if (info != null) {
                                                    newUid = info.uid;
                                                }
                                            } catch (Exception e) {
                                                Ln.e("Could not get UID for " + packageName, e);
                                            }
                                        }

                                        if (newUid != lastUid) {
                                            Ln.i("Updating audio capture target UID: " + newUid + " for package: " + packageName);
                                            audioCapture.setTargetUid(newUid);
                                            lastUid = newUid;
                                        }
                                    }
                                }
                                break;
                            }
                        }

                    }

                    if (!foundTopApp) {
                        if (!"SYSTEM".equals(lastPackageName)) {
                            Ln.i("No app detected on display " + displayId + ", switching to Global Audio");
                            lastPackageName = "SYSTEM";
                            if (lastUid != -1) {
                                audioCapture.setTargetUid(-1);
                                lastUid = -1;
                            }
                        }
                    }
                    Thread.sleep(500);
                } catch (InterruptedException e) {
                    break;
                } catch (Exception e) {
                    Ln.e("App monitor error", e);
                    try {
                        Thread.sleep(1000);
                    } catch (InterruptedException ignored) {
                        // ignored
                    }
                }
            }
        }, "AppMonitor");
        appMonitorThread.start();
    }

    @android.annotation.SuppressLint("BlockedPrivateApi")
    private static int getTaskDisplayId(android.app.ActivityManager.RunningTaskInfo task) {
        try {
            Class<?> taskInfoClass = Class.forName("android.app.TaskInfo");
            java.lang.reflect.Field displayIdField = taskInfoClass.getDeclaredField("displayId");
            displayIdField.setAccessible(true);
            return displayIdField.getInt(task);
        } catch (Exception e) {
            Ln.w("Could not get displayId from task", e);
            return -1;
        }
    }

    private void stopAppMonitor() {
        monitorRunning = false;
        if (appMonitorThread != null) {
            appMonitorThread.interrupt();
            appMonitorThread = null;
        }
    }

    public void startNew(Surface surface) {
        int virtualDisplayId;
        try {
            int flags = VIRTUAL_DISPLAY_FLAG_PUBLIC
                    | VIRTUAL_DISPLAY_FLAG_PRESENTATION
                    | VIRTUAL_DISPLAY_FLAG_OWN_CONTENT_ONLY
                    | VIRTUAL_DISPLAY_FLAG_SUPPORTS_TOUCH
                    | VIRTUAL_DISPLAY_FLAG_ROTATES_WITH_CONTENT;
            if (vdDestroyContent) {
                flags |= VIRTUAL_DISPLAY_FLAG_DESTROY_CONTENT_ON_REMOVAL;
            }
            if (vdSystemDecorations) {
                flags |= VIRTUAL_DISPLAY_FLAG_SHOULD_SHOW_SYSTEM_DECORATIONS;
            }
            if (Build.VERSION.SDK_INT >= AndroidVersions.API_33_ANDROID_13) {
                flags |= VIRTUAL_DISPLAY_FLAG_TRUSTED
                        | VIRTUAL_DISPLAY_FLAG_OWN_DISPLAY_GROUP
                        | VIRTUAL_DISPLAY_FLAG_ALWAYS_UNLOCKED
                        | VIRTUAL_DISPLAY_FLAG_TOUCH_FEEDBACK_DISABLED;
                if (Build.VERSION.SDK_INT >= AndroidVersions.API_34_ANDROID_14) {
                    flags |= VIRTUAL_DISPLAY_FLAG_OWN_FOCUS
                            | VIRTUAL_DISPLAY_FLAG_DEVICE_DISPLAY_GROUP;
                }
            }
            virtualDisplay = ServiceManager.getDisplayManager()
                    .createNewVirtualDisplay("scrcpy", displaySize.getWidth(), displaySize.getHeight(), dpi, surface, flags);
            virtualDisplayId = virtualDisplay.getDisplay().getDisplayId();
            Ln.i("New display: " + displaySize.getWidth() + "x" + displaySize.getHeight() + "/" + dpi + " (id=" + virtualDisplayId + ")");

            if (displayImePolicy != -1) {
                ServiceManager.getWindowManager().setDisplayImePolicy(virtualDisplayId, displayImePolicy);
            }

            displaySizeMonitor.start(virtualDisplayId, this::invalidate);

            startAppMonitor(virtualDisplayId);

        } catch (Exception e) {
            Ln.e("Could not create display", e);
            throw new AssertionError("Could not create display");
        }
    }

    @Override
    public void start(Surface surface) throws IOException {
        if (displayTransform != null) {
            assert glRunner == null;
            OpenGLFilter glFilter = new AffineOpenGLFilter(displayTransform);
            glRunner = new OpenGLRunner(glFilter);
            surface = glRunner.start(physicalSize, videoSize, surface);
        }

        if (virtualDisplay == null) {
            startNew(surface);
        } else {
            virtualDisplay.setSurface(surface);
            startAppMonitor(virtualDisplay.getDisplay().getDisplayId());
        }

        if (vdListener != null) {
            PositionMapper positionMapper = PositionMapper.create(videoSize, eventTransform, displaySize);
            vdListener.onNewVirtualDisplay(virtualDisplay.getDisplay().getDisplayId(), positionMapper);
        }
    }

    @Override
    public void stop() {
        stopAppMonitor();
        if (glRunner != null) {
            glRunner.stopAndRelease();
            glRunner = null;
        }
    }

    @Override
    public void release() {
        stopAppMonitor();
        displaySizeMonitor.stopAndRelease();

        if (virtualDisplay != null) {
            virtualDisplay.release();
            virtualDisplay = null;
        }
    }

    @Override
    public synchronized Size getSize() {
        return videoSize;
    }

    @Override
    public synchronized boolean setMaxSize(int newMaxSize) {
        maxSize = newMaxSize;
        return true;
    }

    private static int scaleDpi(Size initialSize, int initialDpi, Size size) {
        int den = initialSize.getMax();
        int num = size.getMax();
        return initialDpi * num / den;
    }

    @Override
    public void requestInvalidate() {
        invalidate();
    }
}
