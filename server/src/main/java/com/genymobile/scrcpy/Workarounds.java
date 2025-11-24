package com.genymobile.scrcpy;

import com.genymobile.scrcpy.audio.AudioCaptureException;
import com.genymobile.scrcpy.util.Ln;
import com.genymobile.scrcpy.util.Reflection;

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
import java.lang.reflect.Method;

@SuppressLint("PrivateApi,BlockedPrivateApi,SoonBlockedPrivateApi,DiscouragedPrivateApi")
public final class Workarounds {

    private static final Object ACTIVITY_THREAD;

    static {
        try {
            Class<?> ACTIVITY_THREAD_CLASS = Class.forName("android.app.ActivityThread");
            ACTIVITY_THREAD = Reflection.createInstance(ACTIVITY_THREAD_CLASS);
            // ActivityThread.sCurrentActivityThread = ACTIVITY_THREAD;
            Reflection.setField(ACTIVITY_THREAD_CLASS, null, "sCurrentActivityThread", ACTIVITY_THREAD);
            // ACTIVITY_THREAD.mSystemThread = true;
            Reflection.setField(ACTIVITY_THREAD, "mSystemThread", true);

            // debug log
            Ln.i(">>>ACTIVITY_THREAD.sCurrentActivityThread -> " + Reflection.getField(ACTIVITY_THREAD_CLASS, null, "sCurrentActivityThread"));
            Ln.i(">>>ACTIVITY_THREAD.mSystemThread -> " + Reflection.getField(ACTIVITY_THREAD, "mSystemThread"));
        } catch (Exception e) {
            throw new AssertionError(e);
        }
    }

    private Workarounds() {
        // not instantiable
    }

    public static void apply() {
        if (Build.VERSION.SDK_INT >= AndroidVersions.API_31_ANDROID_12) {
            // On some Samsung devices, DisplayManagerGlobal.getDisplayInfoLocked() calls ActivityThread.currentActivityThread().getConfiguration(),
            // which requires a non-null ConfigurationController.
            // ConfigurationController was introduced in Android 12, so do not attempt to set it on lower versions.
            // <https://github.com/Genymobile/scrcpy/issues/4467>
            // Must be called before fillAppContext() because it is necessary to get a valid system context.
            fillConfigurationController();
        }

        // On ONYX devices, fillAppInfo() breaks video mirroring:
        // <https://github.com/Genymobile/scrcpy/issues/5182>
        boolean mustFillAppInfo = !Build.BRAND.equalsIgnoreCase("ONYX");

        // For test
        // if (mustFillAppInfo) {
        fillAppInfo();
        // }

        fillAppContext();
    }

    private static void fillAppInfo() {
        try {
            Class<?> appBindDataClass = Class.forName("android.app.ActivityThread$AppBindData");
            Object appBindData = Reflection.createInstance(appBindDataClass);

            ApplicationInfo applicationInfo = new ApplicationInfo();
            applicationInfo.packageName = FakeContext.PACKAGE_NAME;

            // appBindData.appInfo = applicationInfo;
            Reflection.setField(appBindData, "appInfo", applicationInfo);

            // ACTIVITY_THREAD.mBoundApplication = appBindData;
            Reflection.setField(ACTIVITY_THREAD, "mBoundApplication", appBindData);

            // debug log
            Ln.i(">>>appBindData.appInfo -> " + Reflection.getField(appBindData, "appInfo"));
            Ln.i(">>>ACTIVITY_THREAD.mBoundApplication -> " + Reflection.getField(ACTIVITY_THREAD, "mBoundApplication"));
        } catch (Throwable throwable) {
            // this is a workaround, so failing is not an error
            Ln.d("Could not fill app info: " + throwable.getMessage());
        }
    }

    private static void fillAppContext() {
        try {
            Application app = new Application();
            // app.mBase = FakeContext.get();
            Reflection.setField(ContextWrapper.class, app, "mBase", FakeContext.get());

            // ACTIVITY_THREAD.mInitialApplication = app;
            Reflection.setField(ACTIVITY_THREAD, "mInitialApplication", app);

            // debug log
            Ln.i(">>>Application.mBase -> " + Reflection.getField(ContextWrapper.class, app, "mBase"));
            Ln.i(">>>ACTIVITY_THREAD.mInitialApplication -> " + Reflection.getField(ACTIVITY_THREAD, "mInitialApplication"));
        } catch (Throwable throwable) {
            // this is a workaround, so failing is not an error
            Ln.d("Could not fill app context: " + throwable.getMessage());
        }
    }

    private static void fillConfigurationController() {
        try {
            Class<?> configurationControllerClass = Class.forName("android.app.ConfigurationController");
            Class<?> activityThreadInternalClass = Class.forName("android.app.ActivityThreadInternal");
            // new ConfigurationController(ACTIVITY_THREAD);
            Object configurationController = Reflection.createInstance(configurationControllerClass, new Class[]{activityThreadInternalClass}, ACTIVITY_THREAD);
            // ACTIVITY_THREAD.mConfigurationController = new ConfigurationController(ACTIVITY_THREAD);
            Reflection.setField(ACTIVITY_THREAD, "mConfigurationController", configurationController);

            // debug log
            Ln.i(">>>Created ConfigurationController -> " + configurationController);
            Ln.i(">>>ACTIVITY_THREAD.mConfigurationController -> " + Reflection.getField(ACTIVITY_THREAD, "mConfigurationController"));
        } catch (Throwable throwable) {
            Ln.d("Could not fill configuration: " + throwable.getMessage());
        }
    }

    static Context getSystemContext() {
        try {
            return Reflection.invokeMethod(ACTIVITY_THREAD,"getSystemContext");
        } catch (Throwable throwable) {
            // this is a workaround, so failing is not an error
            Ln.d("Could not get system context: " + throwable.getMessage());
            return null;
        }
    }

    @TargetApi(AndroidVersions.API_30_ANDROID_11)
    @SuppressLint("WrongConstant,MissingPermission")
    public static AudioRecord createAudioRecord(int source, int sampleRate, int channelConfig, int channels, int channelMask, int encoding) throws
            AudioCaptureException {
        // Vivo (and maybe some other third-party ROMs) modified `AudioRecord`'s constructor, requiring `Context`s from real App environment.
        //
        // This method invokes the `AudioRecord(long nativeRecordInJavaObj)` constructor to create an empty `AudioRecord` instance, then uses
        // reflections to initialize it like the normal constructor do (or the `AudioRecord.Builder.build()` method do).
        // As a result, the modified code was not executed.
        try {
            // AudioRecord audioRecord = new AudioRecord(0L);
            AudioRecord audioRecord = Reflection.createInstance(AudioRecord.class, 0L);

            // audioRecord.mRecordingState = RECORDSTATE_STOPPED;
            Reflection.setField(audioRecord, "mRecordingState", AudioRecord.RECORDSTATE_STOPPED);

            Looper looper = Looper.myLooper();
            if (looper == null) {
                looper = Looper.getMainLooper();
            }

            // audioRecord.mInitializationLooper = looper;
            Reflection.setField(audioRecord, "mInitializationLooper", looper);

            // Create `AudioAttributes` with fixed capture preset
            int capturePreset = source;
            AudioAttributes.Builder audioAttributesBuilder = new AudioAttributes.Builder();
            Reflection.invokeMethod(audioAttributesBuilder, "setInternalCapturePreset", capturePreset);
            AudioAttributes attributes = audioAttributesBuilder.build();

            // audioRecord.mAudioAttributes = attributes;
            Reflection.setField(audioRecord, "mAudioAttributes", attributes);

            // audioRecord.audioParamCheck(capturePreset, sampleRate, encoding);
            Reflection.invokeMethod(audioRecord, "audioParamCheck", capturePreset, sampleRate, encoding);

            // audioRecord.mChannelCount = channels
            Reflection.setField(audioRecord, "mChannelCount", channels);

            // audioRecord.mChannelMask = channelMask
            Reflection.setField(audioRecord, "mChannelMask", channelMask);

            int minBufferSize = AudioRecord.getMinBufferSize(sampleRate, channelConfig, encoding);
            int bufferSizeInBytes = minBufferSize * 8;

            // audioRecord.audioBuffSizeCheck(bufferSizeInBytes)
            Reflection.invokeMethod(audioRecord, "audioBuffSizeCheck", bufferSizeInBytes);
            Ln.i(">>>Created empty AudioRecord -> " + audioRecord);
            Ln.i(">>>AudioRecord.mRecordingState -> " + Reflection.getField(audioRecord, "mRecordingState"));
            Ln.i(">>>AudioRecord.mInitializationLooper -> " + Reflection.getField(audioRecord, "mInitializationLooper"));
            Ln.i(">>>AudioAttributes.Builder after setInternalCapturePreset -> " + Reflection.getField(audioAttributesBuilder, "mSource"));
            Ln.i(">>>AudioRecord.mAudioAttributes -> " + Reflection.getField(audioRecord, "mAudioAttributes"));
            Ln.i(">>>AudioRecord.mRecordSource -> " + Reflection.getField(audioRecord, "mRecordSource"));
            Ln.i(">>>AudioRecord.mSampleRate -> " + Reflection.getField(audioRecord, "mSampleRate"));
            Ln.i(">>>AudioRecord.mAudioFormat -> " + Reflection.getField(audioRecord, "mAudioFormat"));
            Ln.i(">>>AudioRecord.mChannelCount -> " + Reflection.getField(audioRecord, "mChannelCount"));
            Ln.i(">>>AudioRecord.mChannelMask -> " + Reflection.getField(audioRecord, "mChannelMask"));
            Ln.i(">>>AudioRecord bufferSizeInBytes -> " + bufferSizeInBytes);

            final int channelIndexMask = 0;

            int[] sampleRateArray = new int[]{sampleRate};
            int[] session = new int[]{AudioManager.AUDIO_SESSION_ID_GENERATE};

            int initResult;
            if (Build.VERSION.SDK_INT < AndroidVersions.API_31_ANDROID_12) {
                // private native final int native_setup(Object audiorecord_this,
                //     Object /*AudioAttributes*/ attributes,
                //     int[] sampleRate, int channelMask, int channelIndexMask, int audioFormat,
                //     int buffSizeInBytes, int[] sessionId, String opPackageName,
                //     long nativeRecordInJavaObj);
                Class<?>[] nativeSetupParamTypes = new Class[]{
                        Object.class, Object.class, int[].class, int.class,
                        int.class, int.class, int.class, int[].class, String.class, long.class
                };
                initResult = Reflection.invokeMethodWithParam(
                        AudioRecord.class,
                        audioRecord,
                        "native_setup",
                        nativeSetupParamTypes,
                        new WeakReference<>(audioRecord),
                        attributes,
                        sampleRateArray,
                        channelMask, channelIndexMask, audioRecord.getAudioFormat(), bufferSizeInBytes,
                        session,
                        FakeContext.get().getOpPackageName(),
                        0L
                );
                Ln.i(">>>Invoked native_setup without AttributionSource, result -> " + initResult);
            } else {
                // Assume `context` is never `null`
                AttributionSource attributionSource = FakeContext.get().getAttributionSource();

                // Assume `attributionSource.getPackageName()` is never null

                // ScopedParcelState attributionSourceState = attributionSource.asScopedParcelState()
                Method asScopedParcelStateMethod = AttributionSource.class.getDeclaredMethod("asScopedParcelState");
                asScopedParcelStateMethod.setAccessible(true);


                try (AutoCloseable attributionSourceState = Reflection.invokeMethod(attributionSource, "asScopedParcelState")) {
                    Parcel attributionSourceParcel = Reflection.invokeMethod(attributionSourceState, "getParcel");
                    Ln.i(">>>Obtained AttributionSource Parcel -> " + attributionSourceParcel);

                    if (Build.VERSION.SDK_INT < AndroidVersions.API_34_ANDROID_14) {
                        // private native int native_setup(Object audiorecordThis,
                        //     Object /*AudioAttributes*/ attributes,
                        //     int[] sampleRate, int channelMask, int channelIndexMask, int audioFormat,
                        //     int buffSizeInBytes, int[] sessionId, @NonNull Parcel attributionSource,
                        //     long nativeRecordInJavaObj, int maxSharedAudioHistoryMs);
                        Class<?>[] nativeSetupParamTypes = new Class[]{
                                Object.class, Object.class, int[].class,
                                int.class, int.class, int.class, int.class, int[].class, Parcel.class, long.class, int.class
                        };
                        initResult = Reflection.invokeMethodWithParam(
                                AudioRecord.class,
                                audioRecord,
                                "native_setup",
                                nativeSetupParamTypes,
                                new WeakReference<>(audioRecord),
                                attributes,
                                sampleRateArray,
                                channelMask, channelIndexMask, audioRecord.getAudioFormat(), bufferSizeInBytes,
                                session,
                                attributionSourceParcel,
                                0L,
                                0
                        );
                        Ln.i(">>>Invoked native_setup with AttributionSource, result -> " + initResult);
                    } else {
                        // Android 14 added a new int parameter "halInputFlags"
                        // <https://github.com/aosp-mirror/platform_frameworks_base/commit/f6135d75db79b1d48fad3a3b3080d37be20a2313>
                        Class<?>[] nativeSetupParamTypes = new Class[]{
                                Object.class, Object.class, int[].class,
                                int.class, int.class, int.class, int.class, int[].class, Parcel.class, long.class, int.class, int.class
                        };
                        initResult = Reflection.invokeMethodWithParam(
                                AudioRecord.class,
                                audioRecord,
                                "native_setup",
                                nativeSetupParamTypes,
                                new WeakReference<>(audioRecord),
                                attributes,
                                sampleRateArray,
                                channelMask, channelIndexMask, audioRecord.getAudioFormat(), bufferSizeInBytes,
                                session,
                                attributionSourceParcel,
                                0L,
                                0,
                                0
                        );
                        Ln.i(">>>Invoked native_setup with AttributionSource (Android 14), result -> " + initResult);
                    }
                }
            }

            if (initResult != AudioRecord.SUCCESS) {
                Ln.e("Error code " + initResult + " when initializing native AudioRecord object.");
                throw new RuntimeException("Cannot create AudioRecord");
            }

            // mSampleRate = sampleRate[0]
            Reflection.setField(audioRecord, "mSampleRate", sampleRateArray[0]);
            Ln.i(">>>AudioRecord.mSampleRate -> " + Reflection.getField(audioRecord, "mSampleRate"));

            // audioRecord.mSessionId = session[0]
            Reflection.setField(audioRecord, "mSessionId", session[0]);
            Ln.i(">>>AudioRecord.mSessionId -> " + Reflection.getField(audioRecord, "mSessionId"));

            // audioRecord.mState = AudioRecord.STATE_INITIALIZED
            Reflection.setField(audioRecord, "mState", AudioRecord.STATE_INITIALIZED);
            Ln.i(">>>AudioRecord.mState -> " + Reflection.getField(audioRecord, "mState"));

            return audioRecord;
        } catch (Exception e) {
            Ln.e("Cannot create AudioRecord", e);
            throw new AudioCaptureException();
        }
    }
}
