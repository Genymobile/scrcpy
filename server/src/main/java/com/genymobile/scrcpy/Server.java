package com.genymobile.scrcpy;

import com.genymobile.scrcpy.audio.AudioCapture;
import com.genymobile.scrcpy.audio.AudioCodec;
import com.genymobile.scrcpy.audio.AudioDirectCapture;
import com.genymobile.scrcpy.audio.AudioEncoder;
import com.genymobile.scrcpy.audio.AudioPlaybackCapture;
import com.genymobile.scrcpy.audio.AudioRawRecorder;
import com.genymobile.scrcpy.audio.AudioSource;
import com.genymobile.scrcpy.control.ControlChannel;
import com.genymobile.scrcpy.control.Controller;
import com.genymobile.scrcpy.control.DeviceMessage;
import com.genymobile.scrcpy.device.ConfigurationException;
import com.genymobile.scrcpy.device.DesktopConnection;
import com.genymobile.scrcpy.device.Device;
import com.genymobile.scrcpy.device.Streamer;
import com.genymobile.scrcpy.util.Ln;
import com.genymobile.scrcpy.util.LogUtils;
import com.genymobile.scrcpy.util.Settings;
import com.genymobile.scrcpy.util.SettingsException;
import com.genymobile.scrcpy.video.CameraCapture;
import com.genymobile.scrcpy.video.ScreenCapture;
import com.genymobile.scrcpy.video.SurfaceCapture;
import com.genymobile.scrcpy.video.SurfaceEncoder;
import com.genymobile.scrcpy.video.VideoSource;
import com.genymobile.scrcpy.wrappers.MediaManager;

import android.media.MediaMetadata;
import android.media.session.PlaybackState;
import android.os.BatteryManager;
import android.os.Build;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public final class Server {

    public static final String SERVER_PATH;

    static {
        String[] classPaths = System.getProperty("java.class.path").split(File.pathSeparator);
        // By convention, scrcpy is always executed with the absolute path of scrcpy-server.jar as the first item in the classpath
        SERVER_PATH = classPaths[0];
    }

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

    private static void initAndCleanUp(Options options, CleanUp cleanUp) {
        // This method is called from its own thread, so it may only configure cleanup actions which are NOT dynamic (i.e. they are configured once
        // and for all, they cannot be changed from another thread)

        if (options.getShowTouches()) {
            try {
                String oldValue = Settings.getAndPutValue(Settings.TABLE_SYSTEM, "show_touches", "1");
                // If "show touches" was disabled, it must be disabled back on clean up
                if (!"1".equals(oldValue)) {
                    if (!cleanUp.setDisableShowTouches(true)) {
                        Ln.e("Could not disable show touch on exit");
                    }
                }
            } catch (SettingsException e) {
                Ln.e("Could not change \"show_touches\"", e);
            }
        }

        if (options.getStayAwake()) {
            int stayOn = BatteryManager.BATTERY_PLUGGED_AC | BatteryManager.BATTERY_PLUGGED_USB | BatteryManager.BATTERY_PLUGGED_WIRELESS;
            try {
                String oldValue = Settings.getAndPutValue(Settings.TABLE_GLOBAL, "stay_on_while_plugged_in", String.valueOf(stayOn));
                try {
                    int restoreStayOn = Integer.parseInt(oldValue);
                    if (restoreStayOn != stayOn) {
                        // Restore only if the current value is different
                        if (!cleanUp.setRestoreStayOn(restoreStayOn)) {
                            Ln.e("Could not restore stay on on exit");
                        }
                    }
                } catch (NumberFormatException e) {
                    // ignore
                }
            } catch (SettingsException e) {
                Ln.e("Could not change \"stay_on_while_plugged_in\"", e);
            }
        }

        if (options.getPowerOffScreenOnClose()) {
            if (!cleanUp.setPowerOffScreen(true)) {
                Ln.e("Could not power off screen on exit");
            }
        }
    }

    private static void scrcpy(Options options) throws IOException, ConfigurationException {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S && options.getVideoSource() == VideoSource.CAMERA) {
            Ln.e("Camera mirroring is not supported before Android 12");
            throw new ConfigurationException("Camera mirroring is not supported");
        }

        CleanUp cleanUp = null;
        Thread initThread = null;

        if (options.getCleanup()) {
            cleanUp = CleanUp.configure(options.getDisplayId());
            initThread = startInitThread(options, cleanUp);
        }

        int scid = options.getScid();
        boolean tunnelForward = options.isTunnelForward();
        boolean control = options.getControl();
        boolean video = options.getVideo();
        boolean audio = options.getAudio();
        boolean media = options.getMediaControls();
        boolean sendDummyByte = options.getSendDummyByte();
        boolean camera = video && options.getVideoSource() == VideoSource.CAMERA;

        final Device device = camera ? null : new Device(options);

        Workarounds.apply();

        List<AsyncProcessor> asyncProcessors = new ArrayList<>();

        DesktopConnection connection = DesktopConnection.open(scid, tunnelForward, video, audio, control, sendDummyByte);
        try {
            if (options.getSendDeviceMeta()) {
                connection.sendDeviceMeta(Device.getDeviceName());
            }

            if (control) {
                ControlChannel controlChannel = connection.getControlChannel();
                Controller controller = new Controller(device, controlChannel, cleanUp, options.getClipboardAutosync(), options.getPowerOn());
                device.setClipboardListener(text -> {
                    DeviceMessage msg = DeviceMessage.createClipboard(text);
                    controller.getSender().send(msg);
                });
                asyncProcessors.add(controller);

                if (media) {
                    MediaManager mediaManager = MediaManager.create();

                    mediaManager.setMediaChangeListener(new MediaManager.MediaChange() {
                        @Override
                        public void onMetadataChange(int id, MediaMetadata metadata) {
                            Ln.i("onMetadataChange " + id);
                            byte[] data = MediaManager.mediaMetadataSerialize(metadata);
                            DeviceMessage msg = DeviceMessage.createMediaUpdate(id, data);
                            controller.getSender().send(msg);
                        }

                        @Override
                        public void onPlaybackStateChange(int id, PlaybackState playbackState) {
                            Ln.i("onPlaybackStateChange " + id);
                            int state = MediaManager.create().playbackStateSerialize(playbackState);
                            if(state < 0) {
                                return;
                            }
                            DeviceMessage msg = DeviceMessage.createMediaState(id, state);
                            controller.getSender().send(msg);
                        }

                        @Override
                        public void onRemove(int id) {
                            Ln.i("onRemove " + id);
                            DeviceMessage msg = DeviceMessage.createMediaRemove(id);
                            controller.getSender().send(msg);
                        }
                    });

                    mediaManager.start();
                }
            }

            if (audio) {
                AudioCodec audioCodec = options.getAudioCodec();
                AudioSource audioSource = options.getAudioSource();
                AudioCapture audioCapture;
                if (audioSource.isDirect()) {
                    audioCapture = new AudioDirectCapture(audioSource);
                } else {
                    audioCapture = new AudioPlaybackCapture(options.getAudioDup());
                }

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
                Streamer videoStreamer = new Streamer(connection.getVideoFd(), options.getVideoCodec(), options.getSendCodecMeta(),
                        options.getSendFrameMeta());
                SurfaceCapture surfaceCapture;
                if (options.getVideoSource() == VideoSource.DISPLAY) {
                    surfaceCapture = new ScreenCapture(device);
                } else {
                    surfaceCapture = new CameraCapture(options.getCameraId(), options.getCameraFacing(), options.getCameraSize(),
                            options.getMaxSize(), options.getCameraAspectRatio(), options.getCameraFps(), options.getCameraHighSpeed());
                }
                SurfaceEncoder surfaceEncoder = new SurfaceEncoder(surfaceCapture, videoStreamer, options.getVideoBitRate(), options.getMaxFps(),
                        options.getVideoCodecOptions(), options.getVideoEncoder(), options.getDownsizeOnError());
                asyncProcessors.add(surfaceEncoder);
            }




            Completion completion = new Completion(asyncProcessors.size());
            for (AsyncProcessor asyncProcessor : asyncProcessors) {
                asyncProcessor.start((fatalError) -> {
                    completion.addCompleted(fatalError);
                });
            }

            completion.await();
        } finally {
            if (initThread != null) {
                initThread.interrupt();
            }
            for (AsyncProcessor asyncProcessor : asyncProcessors) {
                asyncProcessor.stop();
            }

            connection.shutdown();

            try {
                if (initThread != null) {
                    initThread.join();
                }
                for (AsyncProcessor asyncProcessor : asyncProcessors) {
                    asyncProcessor.join();
                }
            } catch (InterruptedException e) {
                // ignore
            }

            connection.close();
        }
    }

    private static Thread startInitThread(final Options options, final CleanUp cleanUp) {
        Thread thread = new Thread(() -> initAndCleanUp(options, cleanUp), "init-cleanup");
        thread.start();
        return thread;
    }

    public static void main(String... args) {
        int status = 0;
        try {
            internalMain(args);
        } catch (Throwable t) {
            Ln.e(t.getMessage(), t);
            status = 1;
        } finally {
            // By default, the Java process exits when all non-daemon threads are terminated.
            // The Android SDK might start some non-daemon threads internally, preventing the scrcpy server to exit.
            // So force the process to exit explicitly.
            System.exit(status);
        }
    }

    private static void internalMain(String... args) throws Exception {
        Thread.setDefaultUncaughtExceptionHandler((t, e) -> {
            Ln.e("Exception on thread " + t, e);
        });

        Options options = Options.parse(args);

        Ln.disableSystemStreams();
        Ln.initLogLevel(options.getLogLevel());

        Ln.i("Device: [" + Build.MANUFACTURER + "] " + Build.BRAND + " " + Build.MODEL + " (Android " + Build.VERSION.RELEASE + ")");

        if (options.getList()) {
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
            if (options.getListCameras() || options.getListCameraSizes()) {
                Workarounds.apply();
                Ln.i(LogUtils.buildCameraListMessage(options.getListCameraSizes()));
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
