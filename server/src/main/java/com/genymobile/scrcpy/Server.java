package com.genymobile.scrcpy;

import java.util.Objects;
import android.content.Context;
import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.media.MediaRecorder;
import android.net.LocalSocket;
import java.io.InputStream;
import java.io.BufferedInputStream;

import com.genymobile.scrcpy.audio.AudioCapture;
import com.genymobile.scrcpy.audio.AudioCodec;
import com.genymobile.scrcpy.audio.AudioDirectCapture;
import com.genymobile.scrcpy.audio.AudioEncoder;
import com.genymobile.scrcpy.audio.AudioPlaybackCapture;
import com.genymobile.scrcpy.audio.AudioRawRecorder;
import com.genymobile.scrcpy.audio.AudioSource;
import com.genymobile.scrcpy.control.ControlChannel;
import com.genymobile.scrcpy.control.Controller;
import com.genymobile.scrcpy.device.DesktopConnection;
import com.genymobile.scrcpy.device.Device;
import com.genymobile.scrcpy.device.Streamer;
import com.genymobile.scrcpy.model.ConfigurationException;
import com.genymobile.scrcpy.model.NewDisplay;
import com.genymobile.scrcpy.opengl.OpenGLRunner;
import com.genymobile.scrcpy.util.Ln;
import com.genymobile.scrcpy.util.LogUtils;
import com.genymobile.scrcpy.video.CameraCapture;
import com.genymobile.scrcpy.video.NewDisplayCapture;
import com.genymobile.scrcpy.video.ScreenCapture;
import com.genymobile.scrcpy.video.SurfaceCapture;
import com.genymobile.scrcpy.video.SurfaceEncoder;
import com.genymobile.scrcpy.video.VideoSource;

import android.annotation.SuppressLint;
import android.os.Build;
import android.os.Looper;
import android.system.Os;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.IOException;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
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
                Looper.getMainLooper().quitSafely();
            }
        }
    }

    private Server() {
        // not instantiable
    }

    private static AudioAttributes createAudioAttributes(int capturePreset) throws Exception {
        AudioAttributes.Builder audioAttributesBuilder = new AudioAttributes.Builder();
        Method setCapturePresetMethod =
            audioAttributesBuilder.getClass().getDeclaredMethod("setCapturePreset", int.class);
        setCapturePresetMethod.invoke(audioAttributesBuilder, capturePreset);
        return audioAttributesBuilder.build();
    }

    //https://github.com/Genymobile/scrcpy/issues/3880#issuecomment-1595722119
    public static void poc(Object systemMain, BufferedInputStream bis) throws Exception {
        Objects.requireNonNull(systemMain);

        // var systemContext = systemMain.getSystemContext();
        Method getSystemContextMethod = systemMain.getClass().getDeclaredMethod("getSystemContext");
        Context systemContext = (Context) getSystemContextMethod.invoke(systemMain);
        Objects.requireNonNull(systemContext);

        // var audioMixRuleBuilder = new AudioMixingRule.Builder();
        @SuppressLint("PrivateApi")
        Class<?> audioMixRuleBuilderClass =
                        Class.forName("android.media.audiopolicy.AudioMixingRule$Builder");
        Object audioMixRuleBuilder = audioMixRuleBuilderClass.newInstance();

        try {
            // Added in Android 13, but previous versions don't work because lack of permission.
            // audioMixRuleBuilder.setTargetMixRole(MIX_ROLE_INJECTOR);
            Method setTargetMixRoleMethod =
                            audioMixRuleBuilder.getClass().getDeclaredMethod("setTargetMixRole", int.class);
            int MIX_ROLE_INJECTOR = 1;
            setTargetMixRoleMethod.invoke(audioMixRuleBuilder, MIX_ROLE_INJECTOR);
        } catch (Exception ignored) {
        }

        Method addMixRuleMethod = audioMixRuleBuilder.getClass()
                        .getDeclaredMethod("addMixRule", int.class, Object.class);
        int RULE_MATCH_ATTRIBUTE_CAPTURE_PRESET = 0x1 << 1;

        // audioMixRuleBuilder.addMixRuleMethod(RULE_MATCH_ATTRIBUTE_CAPTURE_PRESET,
        //     createAudioAttributes(MediaRecorder.AudioSource.DEFAULT));
        addMixRuleMethod.invoke(audioMixRuleBuilder, RULE_MATCH_ATTRIBUTE_CAPTURE_PRESET,
                                                        createAudioAttributes(MediaRecorder.AudioSource.DEFAULT));
        // audioMixRuleBuilder.addMixRuleMethod(RULE_MATCH_ATTRIBUTE_CAPTURE_PRESET,
        //     createAudioAttributes(MediaRecorder.AudioSource.MIC));
        addMixRuleMethod.invoke(audioMixRuleBuilder, RULE_MATCH_ATTRIBUTE_CAPTURE_PRESET,
                                                        createAudioAttributes(MediaRecorder.AudioSource.MIC));
        // audioMixRuleBuilder.addMixRuleMethod(RULE_MATCH_ATTRIBUTE_CAPTURE_PRESET,
        //     createAudioAttributes(MediaRecorder.AudioSource.VOICE_COMMUNICATION));
        addMixRuleMethod.invoke(audioMixRuleBuilder, RULE_MATCH_ATTRIBUTE_CAPTURE_PRESET,
                                                        createAudioAttributes(
                                                                        MediaRecorder.AudioSource.VOICE_COMMUNICATION));
        // audioMixRuleBuilder.addMixRuleMethod(RULE_MATCH_ATTRIBUTE_CAPTURE_PRESET,
        //     createAudioAttributes(MediaRecorder.AudioSource.UNPROCESSED));
        addMixRuleMethod.invoke(audioMixRuleBuilder, RULE_MATCH_ATTRIBUTE_CAPTURE_PRESET,
                                                        createAudioAttributes(MediaRecorder.AudioSource.UNPROCESSED));

        // var audioMixingRule = audioMixRuleBuilder.build();
        Method audioMixRuleBuildMethod = audioMixRuleBuilder.getClass().getDeclaredMethod("build");
        Object audioMixingRule = audioMixRuleBuildMethod.invoke(audioMixRuleBuilder);
        Objects.requireNonNull(audioMixingRule);

        // var audioMixBuilder = new AudioMix.Builder(audioMixingRule);
        @SuppressLint("PrivateApi")
        Class<?> audioMixBuilderClass = Class.forName("android.media.audiopolicy.AudioMix$Builder");
        Constructor audioMixBuilderConstructor =
                        audioMixBuilderClass.getDeclaredConstructor(audioMixingRule.getClass());
        Object audioMixBuilder = audioMixBuilderConstructor.newInstance(audioMixingRule);

        Object audioFormat = new AudioFormat.Builder().setEncoding(AudioFormat.ENCODING_PCM_16BIT)
            .setSampleRate(48000)
            .setChannelMask(AudioFormat.CHANNEL_IN_STEREO)
            .build();

        // audioMixBuilder.setFormat(audioFormat);
        Method setFormatMethod =
            audioMixBuilder.getClass().getDeclaredMethod("setFormat", AudioFormat.class);
        setFormatMethod.invoke(audioMixBuilder, audioFormat);

        // audioMixBuilder.setRouteFlags(ROUTE_FLAG_LOOP_BACK);
        Method setRouteFlagsMethod =
            audioMixBuilder.getClass().getDeclaredMethod("setRouteFlags", int.class);
        int ROUTE_FLAG_LOOP_BACK = 0x1 << 1;
        setRouteFlagsMethod.invoke(audioMixBuilder, ROUTE_FLAG_LOOP_BACK);

        // var audioMix = audioMixBuilder.build();
        Method audioMixBuildMethod = audioMixBuilder.getClass().getDeclaredMethod("build");
        Object audioMix = audioMixBuildMethod.invoke(audioMixBuilder);
        Objects.requireNonNull(audioMix);

        // var audioPolicyBuilder = new AudioPolicy.Builder(systemContext);
        @SuppressLint("PrivateApi")
        Class<?> audioPolicyBuilderClass =
                        Class.forName("android.media.audiopolicy.AudioPolicy$Builder");
        Constructor audioPolicyBuilderConstructor =
                        audioPolicyBuilderClass.getDeclaredConstructor(Context.class);
        Object audioPolicyBuilder = audioPolicyBuilderConstructor.newInstance(systemContext);

        // audioPolicyBuilder.addMix(audioMix);
        Method addMixMethod =
                        audioPolicyBuilder.getClass().getDeclaredMethod("addMix", audioMix.getClass());
        addMixMethod.invoke(audioPolicyBuilder, audioMix);

        // var audioPolicy = audioPolicyBuilder.build();
        Method audioPolicyBuildMethod = audioPolicyBuilder.getClass().getDeclaredMethod("build");
        Object audioPolicy = audioPolicyBuildMethod.invoke(audioPolicyBuilder);
        Objects.requireNonNull(audioPolicy);

        Object audioManager = (AudioManager) systemContext.getSystemService(AudioManager.class);

        // audioManager.registerAudioPolicy(audioPolicy);
        Method registerAudioPolicyMethod = audioManager.getClass()
                        .getDeclaredMethod("registerAudioPolicy", audioPolicy.getClass());
        // noinspection DataFlowIssue
        int result = (int) registerAudioPolicyMethod.invoke(audioManager, audioPolicy);

        if (result != 0) {
            Ln.d("registerAudioPolicy failed");
            return;
        }

        // var audioTrack = audioPolicy.createAudioTrackSource(audioMix);
        Method createAudioTrackSourceMethod = audioPolicy.getClass()
                        .getDeclaredMethod("createAudioTrackSource", audioMix.getClass());
        AudioTrack audioTrack = (AudioTrack) createAudioTrackSourceMethod.invoke(audioPolicy, audioMix);
        Objects.requireNonNull(audioTrack);

        audioTrack.play();

        /*
        byte[] samples = new byte[440 * 100];
        for (int i = 0; i < samples.length; i += 1) {
                samples[i] = (byte)((i / 40) % 2 == 0 ? 0xff:0x00);
        }
        */

        new Thread(() -> {
            while (true) {
                byte[] micData = new byte[50000];
                try {
                    int readlen = bis.read(micData);
                    int written = audioTrack.write(micData, 0, readlen);
                    Ln.d("written " + written);
                } catch (Exception e) {
                    Ln.e(e.toString());
                }
            }
        }).start();
    }

    private static void scrcpy(Options options) throws IOException, ConfigurationException {
        if (Build.VERSION.SDK_INT < AndroidVersions.API_31_ANDROID_12 && options.getVideoSource() == VideoSource.CAMERA) {
            Ln.e("Camera mirroring is not supported before Android 12");
            throw new ConfigurationException("Camera mirroring is not supported");
        }

        if (Build.VERSION.SDK_INT < AndroidVersions.API_29_ANDROID_10) {
            if (options.getNewDisplay() != null) {
                Ln.e("New virtual display is not supported before Android 10");
                throw new ConfigurationException("New virtual display is not supported");
            }
            if (options.getDisplayImePolicy() != -1) {
                Ln.e("Display IME policy is not supported before Android 10");
                throw new ConfigurationException("Display IME policy is not supported");
            }
        }

        CleanUp cleanUp = null;

        if (options.getCleanup()) {
            cleanUp = CleanUp.start(options);
        }

        int scid = options.getScid();
        boolean tunnelForward = options.isTunnelForward();
        boolean control = options.getControl();
        boolean video = options.getVideo();
        boolean audio = options.getAudio();
        boolean microphone = options.getMicrophone();
        boolean sendDummyByte = options.getSendDummyByte();

        Object systemMain = null;
        try {
            @SuppressLint("PrivateApi")
            Class<?> activityThreadClass = Class.forName("android.app.ActivityThread");
            @SuppressLint("DiscouragedPrivateApi")
            Method systemMainMethod = activityThreadClass.getDeclaredMethod("systemMain");
            systemMain = systemMainMethod.invoke(null);
            Objects.requireNonNull(systemMain);
        } catch (Exception e) {

        }

        Workarounds.apply();

        List<AsyncProcessor> asyncProcessors = new ArrayList<>();

        DesktopConnection connection = DesktopConnection.open(scid, tunnelForward, video, audio, control, microphone, sendDummyByte);
        try {
            if (options.getSendDeviceMeta()) {
                connection.sendDeviceMeta(Device.getDeviceName());
            }

            Controller controller = null;

            if (control) {
                ControlChannel controlChannel = connection.getControlChannel();
                controller = new Controller(controlChannel, cleanUp, options);
                asyncProcessors.add(controller);
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

                Streamer audioStreamer = new Streamer(connection.getAudioFd(), audioCodec, options.getSendStreamMeta(), options.getSendFrameMeta());
                AsyncProcessor audioRecorder;
                if (audioCodec == AudioCodec.RAW) {
                    audioRecorder = new AudioRawRecorder(audioCapture, audioStreamer);
                } else {
                    audioRecorder = new AudioEncoder(audioCapture, audioStreamer, options);
                }
                asyncProcessors.add(audioRecorder);
            }

            if (video) {
                Streamer videoStreamer = new Streamer(connection.getVideoFd(), options.getVideoCodec(), options.getSendStreamMeta(),
                        options.getSendFrameMeta());
                SurfaceCapture surfaceCapture;
                if (options.getVideoSource() == VideoSource.DISPLAY) {
                    NewDisplay newDisplay = options.getNewDisplay();
                    if (newDisplay != null) {
                        surfaceCapture = new NewDisplayCapture(controller, options);
                    } else {
                        assert options.getDisplayId() != Device.DISPLAY_ID_NONE;
                        surfaceCapture = new ScreenCapture(controller, options);
                    }
                } else {
                    surfaceCapture = new CameraCapture(options);
                }
                SurfaceEncoder surfaceEncoder = new SurfaceEncoder(surfaceCapture, videoStreamer, options);
                asyncProcessors.add(surfaceEncoder);

                if (controller != null) {
                    controller.setSurfaceCapture(surfaceCapture);
                }
            }

            if (microphone) {
                try {
                    LocalSocket s = connection.getMicSocket();
                    InputStream is = s.getInputStream();
                    BufferedInputStream bis = new BufferedInputStream(is);
                    poc(systemMain, bis);
                } catch (Exception e) {}
            }

            Completion completion = new Completion(asyncProcessors.size());
            for (AsyncProcessor asyncProcessor : asyncProcessors) {
                asyncProcessor.start((fatalError) -> {
                    completion.addCompleted(fatalError);
                });
            }

            Looper.loop(); // interrupted by the Completion implementation
        } finally {
            if (cleanUp != null) {
                cleanUp.interrupt();
            }
            for (AsyncProcessor asyncProcessor : asyncProcessors) {
                asyncProcessor.stop();
            }

            OpenGLRunner.quit(); // quit the OpenGL thread, if any

            connection.shutdown();

            try {
                if (cleanUp != null) {
                    cleanUp.join();
                }
                for (AsyncProcessor asyncProcessor : asyncProcessors) {
                    asyncProcessor.join();
                }
                OpenGLRunner.join();
            } catch (InterruptedException e) {
                // ignore
            }

            connection.close();
        }
    }

    private static void prepareMainLooper() {
        // Like Looper.prepareMainLooper(), but with quitAllowed set to true
        Looper.prepare();
        synchronized (Looper.class) {
            try {
                @SuppressLint("DiscouragedPrivateApi")
                Field field = Looper.class.getDeclaredField("sMainLooper");
                field.setAccessible(true);
                field.set(null, Looper.myLooper());
            } catch (ReflectiveOperationException e) {
                throw new AssertionError(e);
            }
        }
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
        Thread.UncaughtExceptionHandler defaultHandler = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler((t, e) -> {
            Ln.e("Exception on thread " + t, e);
            if (defaultHandler != null) {
                defaultHandler.uncaughtException(t, e);
            }
        });

        dropRootPrivileges();

        prepareMainLooper();

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
            if (options.getListApps()) {
                Workarounds.apply();
                Ln.i("Processing Android apps... (this may take some time)");
                Ln.i(LogUtils.buildAppListMessage());
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

    @SuppressWarnings("deprecation")
    private static void dropRootPrivileges() {
        try {
            if (Os.getuid() == 0) {
                // Copy-paste does not work with root user
                // <https://github.com/Genymobile/scrcpy/issues/6224>
                Os.setuid(2000);
            }
        } catch (Exception e) {
            Ln.w("Cannot set UID", e);
        }
    }
}
