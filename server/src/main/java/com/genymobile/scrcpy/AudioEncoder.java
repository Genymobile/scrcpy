package com.genymobile.scrcpy;

import java.io.IOException;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;

import com.genymobile.scrcpy.wrappers.ServiceManager;

import android.app.Application;
import android.content.AttributionSource;
import android.content.ComponentName;
import android.content.ContextWrapper;
import android.content.Intent;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.os.Build;

public class AudioEncoder {

    public static class FakePackageNameContext extends ContextWrapper {
        public FakePackageNameContext() {
            super(null);
        }

        @Override
        public String getOpPackageName() {
            // Android 11
            return ServiceManager.PACKAGE_NAME;
        }

        @Override
        public AttributionSource getAttributionSource() {
            try {
                // Android 12+
                return (AttributionSource) AttributionSource.class.getConstructor(int.class, String.class, String.class)
                        .newInstance(2000, ServiceManager.PACKAGE_NAME, null);
            } catch (Throwable e) {
                Ln.e("Can't create fake AttributionSource", e);
                return null;
            }
        }
    }

    private static final int SAMPLE_RATE = 48000;
    private static final int CHANNELS = 2;

    private DesktopConnection connection;

    public AudioEncoder(DesktopConnection connection) {
        this.connection = connection;
    }

    private static AudioFormat createAudioFormat() {
        AudioFormat.Builder builder = new AudioFormat.Builder();
        builder.setEncoding(AudioFormat.ENCODING_PCM_16BIT);
        builder.setSampleRate(SAMPLE_RATE);
        builder.setChannelMask(AudioFormat.CHANNEL_IN_STEREO);
        return builder.build();
    }

    private static AudioRecord createAudioRecord()
            throws ClassNotFoundException, NoSuchMethodException, SecurityException, IllegalAccessException,
            IllegalArgumentException, InstantiationException, InvocationTargetException, NoSuchFieldException {
        AudioRecord.Builder builder = new AudioRecord.Builder();
        try {
            // Android 12+
            builder.setContext(new FakePackageNameContext());
        } catch (NoSuchMethodError e) {
            // Android 11
            Class<?> activityThreadClass = Class.forName("android.app.ActivityThread");
            Constructor<?> activityThreadConstructor = activityThreadClass.getDeclaredConstructor();
            activityThreadConstructor.setAccessible(true);
            Object activityThread = activityThreadConstructor.newInstance();

            Field sCurrentActivityThreadField = activityThreadClass.getDeclaredField("sCurrentActivityThread");
            sCurrentActivityThreadField.setAccessible(true);
            sCurrentActivityThreadField.set(null, activityThread);

            Application app = Application.class.newInstance();
            Field baseField = ContextWrapper.class.getDeclaredField("mBase");
            baseField.setAccessible(true);
            baseField.set(app, new FakePackageNameContext());

            Field mInitialApplicationField = activityThreadClass.getDeclaredField("mInitialApplication");
            mInitialApplicationField.setAccessible(true);
            mInitialApplicationField.set(activityThread, app);
        }
        builder.setAudioSource(MediaRecorder.AudioSource.REMOTE_SUBMIX);
        builder.setAudioFormat(createAudioFormat());
        builder.setBufferSizeInBytes(1024 * 1024);
        return builder.build();
    }

    public Thread startRecording() {
        try {
            // Android 11 requires Apps to be at foreground to record audio.
            // Normally, each App has its own user ID,
            // so Android checks whether the requesting App
            // has the user ID that's at the foreground.
            // But Scrcpy server is NOT an App.
            // It's only a jar package started from Android shell.
            // So it has the same user ID (2000) with Android shell (`com.android.shell`).
            // If there is an activitiy from Android shell running at foreground,
            // the App Ops permission system will believe Scrcpy is also at foreground.
            if (Build.VERSION.SDK_INT == Build.VERSION_CODES.R) {
                Intent intent = new Intent(Intent.ACTION_MAIN);
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                intent.addCategory(Intent.CATEGORY_LAUNCHER);
                intent.setComponent(
                        new ComponentName(ServiceManager.PACKAGE_NAME, "com.android.shell.HeapDumpActivity"));
                ServiceManager.getActivityManager().startActivityAsUserWithFeature(intent);
                // Wait for activity to start
                // TODO find better method
                Thread.sleep(1000);
            }

            final AudioRecord recorder = createAudioRecord();
            Ln.i("AudioRecord created");

            DesktopConnection connection = this.connection;
            Thread recorderThread = new Thread(new Runnable() {
                @Override
                public void run() {
                    try {
                        // Both `new AudioRecord()` and `AudioRecord#startRecoding()`
                        // have the forefround check.
                        recorder.startRecording();
                        Ln.i("AudioRecord started");

                        // Close the dialog opened above to bypass Android 11 foreground check
                        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.R) {
                            ServiceManager.getActivityManager().forceStopPackage(ServiceManager.PACKAGE_NAME);
                        }

                        int BUFFER_MS = 15; // do not buffer more than BUFFER_MS milliseconds
                        byte[] buf = new byte[SAMPLE_RATE * CHANNELS * BUFFER_MS / 1000];
                        while (true) {
                            int r = recorder.read(buf, 0, buf.length);
                            if (r > 0) {
                                connection.sendAudioData(buf, 0, r);
                            }
                            if (r < 0) {
                                Ln.e("Audio capture error: " + r);
                            }
                        }
                    } catch (IOException e) {
                        // ignore
                    } finally {
                        Ln.i("Audio capture stop");
                        recorder.stop();
                    }
                }
            });
            recorderThread.start();

            return recorderThread;
        } catch (Throwable e) {
            Ln.e("Can't create AudioRecord", e);
            return null;
        }
    }

}
