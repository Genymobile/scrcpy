package com.genymobile.scrcpy;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.Application;
import android.content.AttributionSource;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.pm.ApplicationInfo;
import android.media.AudioAttributes;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.os.Build;
import android.os.Looper;
import android.os.Parcel;

import java.lang.ref.WeakReference;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;

public final class Workarounds {

    private static Class<?> activityThreadClass;
    private static Object activityThread;

    private Workarounds() {
        // not instantiable
    }

    public static void apply(boolean audio) {
        Workarounds.prepareMainLooper();

        boolean mustFillAppInfo = false;
        boolean mustFillBaseContext = false;
        boolean mustFillAppContext = false;


        if (Build.BRAND.equalsIgnoreCase("meizu")) {
            // Workarounds must be applied for Meizu phones:
            //  - <https://github.com/Genymobile/scrcpy/issues/240>
            //  - <https://github.com/Genymobile/scrcpy/issues/365>
            //  - <https://github.com/Genymobile/scrcpy/issues/2656>
            //
            // But only apply when strictly necessary, since workarounds can cause other issues:
            //  - <https://github.com/Genymobile/scrcpy/issues/940>
            //  - <https://github.com/Genymobile/scrcpy/issues/994>
            mustFillAppInfo = true;
        } else if (Build.BRAND.equalsIgnoreCase("honor")) {
            // More workarounds must be applied for Honor devices:
            //  - <https://github.com/Genymobile/scrcpy/issues/4015>
            //
            // The system context must not be set for all devices, because it would cause other problems:
            //  - <https://github.com/Genymobile/scrcpy/issues/4015#issuecomment-1595382142>
            //  - <https://github.com/Genymobile/scrcpy/issues/3805#issuecomment-1596148031>
            mustFillAppInfo = true;
            mustFillBaseContext = true;
            mustFillAppContext = true;
        }

        if (audio && Build.VERSION.SDK_INT == Build.VERSION_CODES.R) {
            // Before Android 11, audio is not supported.
            // Since Android 12, we can properly set a context on the AudioRecord.
            // Only on Android 11 we must fill the application context for the AudioRecord to work.
            mustFillAppContext = true;
        }

        if (mustFillAppInfo) {
            Workarounds.fillAppInfo();
        }
        if (mustFillBaseContext) {
            Workarounds.fillBaseContext();
        }
        if (mustFillAppContext) {
            Workarounds.fillAppContext();
        }
    }

    @SuppressWarnings("deprecation")
    private static void prepareMainLooper() {
        // Some devices internally create a Handler when creating an input Surface, causing an exception:
        //   "Can't create handler inside thread that has not called Looper.prepare()"
        // <https://github.com/Genymobile/scrcpy/issues/240>
        //
        // Use Looper.prepareMainLooper() instead of Looper.prepare() to avoid a NullPointerException:
        //   "Attempt to read from field 'android.os.MessageQueue android.os.Looper.mQueue'
        //    on a null object reference"
        // <https://github.com/Genymobile/scrcpy/issues/921>
        Looper.prepareMainLooper();
    }

    @SuppressLint("PrivateApi,DiscouragedPrivateApi")
    private static void fillActivityThread() throws Exception {
        if (activityThread == null) {
            // ActivityThread activityThread = new ActivityThread();
            activityThreadClass = Class.forName("android.app.ActivityThread");
            Constructor<?> activityThreadConstructor = activityThreadClass.getDeclaredConstructor();
            activityThreadConstructor.setAccessible(true);
            activityThread = activityThreadConstructor.newInstance();

            // ActivityThread.sCurrentActivityThread = activityThread;
            Field sCurrentActivityThreadField = activityThreadClass.getDeclaredField("sCurrentActivityThread");
            sCurrentActivityThreadField.setAccessible(true);
            sCurrentActivityThreadField.set(null, activityThread);
        }
    }

    @SuppressLint("PrivateApi,DiscouragedPrivateApi")
    private static void fillAppInfo() {
        try {
            fillActivityThread();

            // ActivityThread.AppBindData appBindData = new ActivityThread.AppBindData();
            Class<?> appBindDataClass = Class.forName("android.app.ActivityThread$AppBindData");
            Constructor<?> appBindDataConstructor = appBindDataClass.getDeclaredConstructor();
            appBindDataConstructor.setAccessible(true);
            Object appBindData = appBindDataConstructor.newInstance();

            ApplicationInfo applicationInfo = new ApplicationInfo();
            applicationInfo.packageName = FakeContext.PACKAGE_NAME;

            // appBindData.appInfo = applicationInfo;
            Field appInfoField = appBindDataClass.getDeclaredField("appInfo");
            appInfoField.setAccessible(true);
            appInfoField.set(appBindData, applicationInfo);

            // activityThread.mBoundApplication = appBindData;
            Field mBoundApplicationField = activityThreadClass.getDeclaredField("mBoundApplication");
            mBoundApplicationField.setAccessible(true);
            mBoundApplicationField.set(activityThread, appBindData);
        } catch (Throwable throwable) {
            // this is a workaround, so failing is not an error
            Ln.d("Could not fill app info: " + throwable.getMessage());
        }
    }

    @SuppressLint("PrivateApi,DiscouragedPrivateApi")
    private static void fillAppContext() {
        try {
            fillActivityThread();

            Application app = Application.class.newInstance();
            Field baseField = ContextWrapper.class.getDeclaredField("mBase");
            baseField.setAccessible(true);
            baseField.set(app, FakeContext.get());

            // activityThread.mInitialApplication = app;
            Field mInitialApplicationField = activityThreadClass.getDeclaredField("mInitialApplication");
            mInitialApplicationField.setAccessible(true);
            mInitialApplicationField.set(activityThread, app);
        } catch (Throwable throwable) {
            // this is a workaround, so failing is not an error
            Ln.d("Could not fill app context: " + throwable.getMessage());
        }
    }

    public static void fillBaseContext() {
        try {
            fillActivityThread();

            Method getSystemContextMethod = activityThreadClass.getDeclaredMethod("getSystemContext");
            Context context = (Context) getSystemContextMethod.invoke(activityThread);
            FakeContext.get().setBaseContext(context);
        } catch (Throwable throwable) {
            // this is a workaround, so failing is not an error
            Ln.d("Could not fill base context: " + throwable.getMessage());
        }
    }

    @TargetApi(Build.VERSION_CODES.R)
    @SuppressLint("WrongConstant,MissingPermission,BlockedPrivateApi,SoonBlockedPrivateApi,DiscouragedPrivateApi")
    public static AudioRecord createAudioRecord(int source, int sampleRate, int channelConfig, int channels, int channelMask, int encoding) {
        // Vivo (and maybe some other third-party ROMs) modified `AudioRecord`'s constructor, requiring `Context`s from real App environment.
        //
        // This method invokes the `AudioRecord(long nativeRecordInJavaObj)` constructor to create an empty `AudioRecord` instance, then uses
        // reflections to initialize it like the normal constructor do (or the `AudioRecord.Builder.build()` method do).
        // As a result, the modified code was not executed.
        try {
            // AudioRecord audioRecord = new AudioRecord(0L);
            Constructor<AudioRecord> audioRecordConstructor = AudioRecord.class.getDeclaredConstructor(long.class);
            audioRecordConstructor.setAccessible(true);
            AudioRecord audioRecord = audioRecordConstructor.newInstance(0L);

            // audioRecord.mRecordingState = RECORDSTATE_STOPPED;
            Field mRecordingStateField = AudioRecord.class.getDeclaredField("mRecordingState");
            mRecordingStateField.setAccessible(true);
            mRecordingStateField.set(audioRecord, AudioRecord.RECORDSTATE_STOPPED);

            Looper looper = Looper.myLooper();
            if (looper == null) {
                looper = Looper.getMainLooper();
            }

            // audioRecord.mInitializationLooper = looper;
            Field mInitializationLooperField = AudioRecord.class.getDeclaredField("mInitializationLooper");
            mInitializationLooperField.setAccessible(true);
            mInitializationLooperField.set(audioRecord, looper);

            // Create `AudioAttributes` with fixed capture preset
            int capturePreset = source;
            AudioAttributes.Builder audioAttributesBuilder = new AudioAttributes.Builder();
            Method setInternalCapturePresetMethod = AudioAttributes.Builder.class.getMethod("setInternalCapturePreset", int.class);
            setInternalCapturePresetMethod.invoke(audioAttributesBuilder, capturePreset);
            AudioAttributes attributes = audioAttributesBuilder.build();

            // audioRecord.mAudioAttributes = attributes;
            Field mAudioAttributesField = AudioRecord.class.getDeclaredField("mAudioAttributes");
            mAudioAttributesField.setAccessible(true);
            mAudioAttributesField.set(audioRecord, attributes);

            // audioRecord.audioParamCheck(capturePreset, sampleRate, encoding);
            Method audioParamCheckMethod = AudioRecord.class.getDeclaredMethod("audioParamCheck", int.class, int.class, int.class);
            audioParamCheckMethod.setAccessible(true);
            audioParamCheckMethod.invoke(audioRecord, capturePreset, sampleRate, encoding);

            // audioRecord.mChannelCount = channels
            Field mChannelCountField = AudioRecord.class.getDeclaredField("mChannelCount");
            mChannelCountField.setAccessible(true);
            mChannelCountField.set(audioRecord, channels);

            // audioRecord.mChannelMask = channelMask
            Field mChannelMaskField = AudioRecord.class.getDeclaredField("mChannelMask");
            mChannelMaskField.setAccessible(true);
            mChannelMaskField.set(audioRecord, channelMask);

            int minBufferSize = AudioRecord.getMinBufferSize(sampleRate, channelConfig, encoding);
            int bufferSizeInBytes = minBufferSize * 8;

            // audioRecord.audioBuffSizeCheck(bufferSizeInBytes)
            Method audioBuffSizeCheckMethod = AudioRecord.class.getDeclaredMethod("audioBuffSizeCheck", int.class);
            audioBuffSizeCheckMethod.setAccessible(true);
            audioBuffSizeCheckMethod.invoke(audioRecord, bufferSizeInBytes);

            final int channelIndexMask = 0;

            int[] sampleRateArray = new int[]{sampleRate};
            int[] session = new int[]{AudioManager.AUDIO_SESSION_ID_GENERATE};

            int initResult;
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
                // private native final int native_setup(Object audiorecord_this,
                // Object /*AudioAttributes*/ attributes,
                // int[] sampleRate, int channelMask, int channelIndexMask, int audioFormat,
                // int buffSizeInBytes, int[] sessionId, String opPackageName,
                // long nativeRecordInJavaObj);
                Method nativeSetupMethod = AudioRecord.class.getDeclaredMethod("native_setup", Object.class, Object.class, int[].class, int.class,
                        int.class, int.class, int.class, int[].class, String.class, long.class);
                nativeSetupMethod.setAccessible(true);
                initResult = (int) nativeSetupMethod.invoke(audioRecord, new WeakReference<AudioRecord>(audioRecord), attributes, sampleRateArray,
                        channelMask, channelIndexMask, audioRecord.getAudioFormat(), bufferSizeInBytes, session, FakeContext.get().getOpPackageName(),
                        0L);
            } else {
                // Assume `context` is never `null`
                AttributionSource attributionSource = FakeContext.get().getAttributionSource();

                // Assume `attributionSource.getPackageName()` is never null

                // ScopedParcelState attributionSourceState = attributionSource.asScopedParcelState()
                Method asScopedParcelStateMethod = AttributionSource.class.getDeclaredMethod("asScopedParcelState");
                asScopedParcelStateMethod.setAccessible(true);

                try (AutoCloseable attributionSourceState = (AutoCloseable) asScopedParcelStateMethod.invoke(attributionSource)) {
                    Method getParcelMethod = attributionSourceState.getClass().getDeclaredMethod("getParcel");
                    Parcel attributionSourceParcel = (Parcel) getParcelMethod.invoke(attributionSourceState);

                    // private native int native_setup(Object audiorecordThis,
                    // Object /*AudioAttributes*/ attributes,
                    // int[] sampleRate, int channelMask, int channelIndexMask, int audioFormat,
                    // int buffSizeInBytes, int[] sessionId, @NonNull Parcel attributionSource,
                    // long nativeRecordInJavaObj, int maxSharedAudioHistoryMs);
                    Method nativeSetupMethod = AudioRecord.class.getDeclaredMethod("native_setup", Object.class, Object.class, int[].class, int.class,
                            int.class, int.class, int.class, int[].class, Parcel.class, long.class, int.class);
                    nativeSetupMethod.setAccessible(true);
                    initResult = (int) nativeSetupMethod.invoke(audioRecord, new WeakReference<AudioRecord>(audioRecord), attributes, sampleRateArray,
                            channelMask, channelIndexMask, audioRecord.getAudioFormat(), bufferSizeInBytes, session, attributionSourceParcel, 0L, 0);
                }
            }

            if (initResult != AudioRecord.SUCCESS) {
                Ln.e("Error code " + initResult + " when initializing native AudioRecord object.");
                throw new RuntimeException("Cannot create AudioRecord");
            }

            // mSampleRate = sampleRate[0]
            Field mSampleRateField = AudioRecord.class.getDeclaredField("mSampleRate");
            mSampleRateField.setAccessible(true);
            mSampleRateField.set(audioRecord, sampleRateArray[0]);

            // audioRecord.mSessionId = session[0]
            Field mSessionIdField = AudioRecord.class.getDeclaredField("mSessionId");
            mSessionIdField.setAccessible(true);
            mSessionIdField.set(audioRecord, session[0]);

            // audioRecord.mState = AudioRecord.STATE_INITIALIZED
            Field mStateField = AudioRecord.class.getDeclaredField("mState");
            mStateField.setAccessible(true);
            mStateField.set(audioRecord, AudioRecord.STATE_INITIALIZED);

            return audioRecord;
        } catch (Exception e) {
            Ln.e("Failed to invoke AudioRecord.<init>.", e);
            throw new RuntimeException("Cannot create AudioRecord");
        }
    }
}
