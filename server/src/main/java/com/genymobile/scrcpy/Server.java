package com.genymobile.scrcpy;

import android.content.ComponentName;
import android.content.Intent;
import android.os.BatteryManager;
import android.os.Build;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import com.genymobile.scrcpy.wrappers.ServiceManager;

public final class Server {

    private static class Completion {
        private int running;
        private boolean fatalError;

        Completion(int running) {
            this.running = running;
        }

        synchronized void addCompleted(boolean fatalError) {
            --running;
            if (fatalError) {
                this.fatalError = true;
            }
            if (running == 0 || this.fatalError) {
                notify();
            }
        }

        synchronized void await() {
            try {
                while (running > 0 && !fatalError) {
                    wait();
                }
            } catch (InterruptedException e) {
                // ignore
            }
        }
    }

    private Server() {
        // not instantiable
    }

    private static void initAndCleanUp(Options options) {
        boolean mustDisableShowTouchesOnCleanUp = false;
        int restoreStayOn = -1;
        boolean restoreNormalPowerMode = options.getControl(); // only restore power mode if control is enabled
        if (options.getShowTouches() || options.getStayAwake()) {
            if (options.getShowTouches()) {
                try {
                    String oldValue = Settings.getAndPutValue(Settings.TABLE_SYSTEM, "show_touches", "1");
                    // If "show touches" was disabled, it must be disabled back on clean up
                    mustDisableShowTouchesOnCleanUp = !"1".equals(oldValue);
                } catch (SettingsException e) {
                    Ln.e("Could not change \"show_touches\"", e);
                }
            }

            if (options.getStayAwake()) {
                int stayOn = BatteryManager.BATTERY_PLUGGED_AC | BatteryManager.BATTERY_PLUGGED_USB | BatteryManager.BATTERY_PLUGGED_WIRELESS;
                try {
                    String oldValue = Settings.getAndPutValue(Settings.TABLE_GLOBAL, "stay_on_while_plugged_in", String.valueOf(stayOn));
                    try {
                        restoreStayOn = Integer.parseInt(oldValue);
                        if (restoreStayOn == stayOn) {
                            // No need to restore
                            restoreStayOn = -1;
                        }
                    } catch (NumberFormatException e) {
                        restoreStayOn = 0;
                    }
                } catch (SettingsException e) {
                    Ln.e("Could not change \"stay_on_while_plugged_in\"", e);
                }
            }
        }

        if (options.getCleanup()) {
            try {
                CleanUp.configure(options.getDisplayId(), restoreStayOn, mustDisableShowTouchesOnCleanUp, restoreNormalPowerMode,
                        options.getPowerOffScreenOnClose());
            } catch (IOException e) {
                Ln.e("Could not configure cleanup", e);
            }
        }
    }

    private static void startWorkaroundAndroid11() {
        // Android 11 requires Apps to be at foreground to record audio.
        // Normally, each App has its own user ID, so Android checks whether the requesting App has the user ID that's at the foreground.
        // But scrcpy server is NOT an App, it's a Java application started from Android shell, so it has the same user ID (2000) with Android
        // shell ("com.android.shell").
        // If there is an Activity from Android shell running at foreground, then the permission system will believe scrcpy is also in the
        // foreground.
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.setComponent(new ComponentName(FakeContext.PACKAGE_NAME, "com.android.shell.HeapDumpActivity"));
        ServiceManager.getActivityManager().startActivityAsUserWithFeature(intent);
    }

    private static void stopWorkaroundAndroid11() {
        ServiceManager.getActivityManager().forceStopPackage(FakeContext.PACKAGE_NAME);
    }

    private static void scrcpy(Options options) throws IOException, ConfigurationException {
        Ln.i("Device: [" + Build.MANUFACTURER + "] " + Build.BRAND + " " + Build.MODEL + " (Android " + Build.VERSION.RELEASE + ")");
        final Device device = new Device(options);

        Thread initThread = startInitThread(options);

        int scid = options.getScid();
        boolean tunnelForward = options.isTunnelForward();
        boolean control = options.getControl();
        boolean video = options.getVideo();
        boolean audio = options.getAudio();
        boolean sendDummyByte = options.getSendDummyByte();

        Workarounds.apply(audio);

        List<AsyncProcessor> asyncProcessors = new ArrayList<>();

        DesktopConnection connection = DesktopConnection.open(scid, tunnelForward, video, audio, control, sendDummyByte);

        boolean foregroundWorkaround = false;
        try {
            if (options.getSendDeviceMeta()) {
                connection.sendDeviceMeta(Device.getDeviceName());
            }

            if (control) {
                Controller controller = new Controller(device, connection, options.getClipboardAutosync(), options.getPowerOn());
                device.setClipboardListener(text -> controller.getSender().pushClipboardText(text));
                asyncProcessors.add(controller);
            }

            if (audio) {
                foregroundWorkaround |= Build.VERSION.SDK_INT == Build.VERSION_CODES.R;

                AudioCodec audioCodec = options.getAudioCodec();
                AudioCapture audioCapture = new AudioCapture(options.getAudioSource());
                Streamer audioStreamer = new Streamer(connection.getAudioFd(), audioCodec, options.getSendCodecMeta(), options.getSendFrameMeta());
                AsyncProcessor audioRecorder;
                if (audioCodec == AudioCodec.RAW) {
                    audioRecorder = new AudioRawRecorder(audioCapture, audioStreamer);
                } else {
                    audioRecorder = new AudioEncoder(audioCapture, audioStreamer, options.getAudioBitRate(), options.getAudioCodecOptions(),
                            options.getAudioEncoder());
                }
                asyncProcessors.add(audioRecorder);
            }

            if (video) {
                Streamer videoStreamer = new Streamer(connection.getVideoFd(), options.getVideoCodec(),
                        options.getSendCodecMeta(), options.getSendFrameMeta());
                if (options.getVideoSource() == VideoSource.DISPLAY) {
                    ScreenEncoder screenEncoder = new ScreenEncoder(device, videoStreamer, options.getVideoBitRate(),
                            options.getMaxFps(), options.getVideoCodecOptions(), options.getVideoEncoder(),
                            options.getDownsizeOnError());
                    asyncProcessors.add(screenEncoder);
                } else {
                    foregroundWorkaround |= Build.VERSION.SDK_INT == Build.VERSION_CODES.R;

                    CameraEncoder cameraEncoder = new CameraEncoder(options.getMaxSize(), options.getCameraId(),
                            options.getCameraPosition(), videoStreamer, options.getVideoBitRate(), options.getMaxFps(),
                            options.getVideoCodecOptions(), options.getVideoEncoder(), options.getDownsizeOnError());
                    asyncProcessors.add(cameraEncoder);
                }
            }

            if (foregroundWorkaround) {
                startWorkaroundAndroid11();
            }

            Completion started = new Completion(asyncProcessors.size());
            Completion completion = new Completion(asyncProcessors.size());
            for (AsyncProcessor asyncProcessor : asyncProcessors) {
                asyncProcessor.start(new AsyncProcessor.StatusListener() {
                    public void onStarted(){
                        started.addCompleted(false);
                    }

                    public void onTerminated(boolean fatalError) {
                        started.addCompleted(fatalError);
                        completion.addCompleted(fatalError);
                    }
                });
            }

            started.await();

            if (foregroundWorkaround) {
                stopWorkaroundAndroid11();
            }

            completion.await();
        } finally {
            initThread.interrupt();
            for (AsyncProcessor asyncProcessor : asyncProcessors) {
                asyncProcessor.stop();
            }

            try {
                initThread.join();
                for (AsyncProcessor asyncProcessor : asyncProcessors) {
                    asyncProcessor.join();
                }
            } catch (InterruptedException e) {
                // ignore
            }

            connection.close();
        }
    }

    private static Thread startInitThread(final Options options) {
        Thread thread = new Thread(() -> initAndCleanUp(options), "init-cleanup");
        thread.start();
        return thread;
    }

    public static void main(String... args) throws Exception {
        Thread.setDefaultUncaughtExceptionHandler((t, e) -> {
            Ln.e("Exception on thread " + t, e);
        });

        Options options = Options.parse(args);

        Ln.initLogLevel(options.getLogLevel());

        if (options.getListEncoders() || options.getListDisplays() || options.getListCameras()) {
            if (options.getCleanup()) {
                CleanUp.unlinkSelf();
            }

            if (options.getListEncoders()) {
                Ln.i(LogUtils.buildVideoEncoderListMessage());
                Ln.i(LogUtils.buildAudioEncoderListMessage());
            }
            if (options.getListDisplays()) {
                Ln.i(LogUtils.buildDisplayListMessage());
            }
            if (options.getListCameras()) {
                Ln.i(LogUtils.buildCameraListMessage());
            }
            // Just print the requested data, do not mirror
            return;
        }

        try {
            scrcpy(options);
        } catch (ConfigurationException e) {
            // Do not print stack trace, a user-friendly error-message has already been logged
        }
    }
}
