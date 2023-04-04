package com.genymobile.scrcpy.wrappers;

import java.lang.ref.WeakReference;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.Iterator;

import com.genymobile.scrcpy.Ln;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.AttributionSource;
import android.content.Context;
import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.os.Binder;
import android.os.Build;
import android.os.Looper;
import android.os.Parcel;

public class AudioRecordWrapper {
    public final static String SUBMIX_FIXED_VOLUME = "fixedVolume";

    private static Method getChannelMaskFromLegacyConfigMethod;
    private static Method getCurrentOpPackageNameMethod;

    private static Method getGetChannelMaskFromLegacyConfigMethod() throws NoSuchMethodException {
        if (getChannelMaskFromLegacyConfigMethod == null) {
            getChannelMaskFromLegacyConfigMethod = AudioRecord.class.getDeclaredMethod("getChannelMaskFromLegacyConfig",
                    int.class, boolean.class);
            getChannelMaskFromLegacyConfigMethod.setAccessible(true);
        }
        return getChannelMaskFromLegacyConfigMethod;
    }

    public static int getChannelMaskFromLegacyConfig(int inChannelConfig, boolean allowLegacyConfig)
            throws IllegalAccessException, IllegalArgumentException, InvocationTargetException,
            NoSuchMethodException {
        try {
            return (int) getGetChannelMaskFromLegacyConfigMethod().invoke(null, inChannelConfig, allowLegacyConfig);
        } catch (IllegalAccessException | IllegalArgumentException | InvocationTargetException
                | NoSuchMethodException e) {
            Ln.e("Failed to invoke AudioRecord.getChannelMaskFromLegacyConfig()", e);
            return 0;
        }
    }

    private static Method getGetCurrentOpPackageNameMethod() throws NoSuchMethodException {
        if (getCurrentOpPackageNameMethod == null) {
            getCurrentOpPackageNameMethod = AudioRecord.class.getDeclaredMethod("getCurrentOpPackageName");
            getCurrentOpPackageNameMethod.setAccessible(true);
        }
        return getCurrentOpPackageNameMethod;
    }

    public static String getCurrentOpPackageName(AudioRecord audioRecord)
            throws IllegalAccessException, IllegalArgumentException,
            InvocationTargetException, NoSuchMethodException {
        try {
            return (String) getGetCurrentOpPackageNameMethod().invoke(audioRecord);
        } catch (IllegalAccessException | IllegalArgumentException | InvocationTargetException
                | NoSuchMethodException e) {
            Ln.e("Failed to invoke AudioRecord.getCurrentOpPackageName()", e);
            return null;
        }
    }

    @TargetApi(Build.VERSION_CODES.R)
    @SuppressLint({ "WrongConstant", "MissingPermission", "BlockedPrivateApi" })
    public static AudioRecord newInstance(AudioAttributes attributes, AudioFormat format,
            int bufferSizeInBytes, int sessionId, Context context, int maxSharedAudioHistoryMs)
            throws Exception {
        // Vivo (and maybe some other third-party ROMs) modified `AudioRecord`'s
        // constructor, requiring `Context`s from real App environment.
        //
        // This method invokes the `AudioRecord(long nativeRecordInJavaObj)` constructor
        // to create an empty `AudioRecord` instance,
        // then uses reflections to initialize it like the normal constructor do (or the
        // `AudioRecord.Builder.build()` method do).
        // As a result, the modified code was not executed.
        //
        // The AOSP version of `AudioRecord` constructor code can be found at:
        // Android 11 (R):
        // https://cs.android.com/android/platform/superproject/+/android-11.0.0_r1:frameworks/base/media/java/android/media/AudioRecord.java;l=335;drc=64ed2ec38a511bbbd048985fe413268335e072f8
        // Android 12 (S):
        // https://cs.android.com/android/platform/superproject/+/android-12.0.0_r1:frameworks/base/media/java/android/media/AudioRecord.java;l=388;drc=2eebf929650e0d320a21f0d13677a27d7ab278e9
        // Android 13 (T, functionally identical to Android 12):
        // https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/base/media/java/android/media/AudioRecord.java;l=382;drc=ed242da52f975a1dd18671afb346b18853d729f2
        // Android 14 (U):
        // Not released, but expected to change

        try {
            // AudioRecord audioRecord = new AudioRecord(0L);
            Constructor<AudioRecord> audioRecordConstructor = AudioRecord.class.getDeclaredConstructor(long.class);
            audioRecordConstructor.setAccessible(true);
            AudioRecord audioRecord = audioRecordConstructor.newInstance(0L);

            // audioRecord.mRecordingState = RECORDSTATE_STOPPED;
            Field mRecordingStateField = AudioRecord.class.getDeclaredField("mRecordingState");
            mRecordingStateField.setAccessible(true);
            mRecordingStateField.set(audioRecord, AudioRecord.RECORDSTATE_STOPPED);

            if (attributes == null) {
                throw new IllegalArgumentException("Illegal null AudioAttributes");
            }
            if (format == null) {
                throw new IllegalArgumentException("Illegal null AudioFormat");
            }

            Looper looper = Looper.myLooper();
            if (looper == null) {
                looper = Looper.getMainLooper();
            }

            // audioRecord.mInitializationLooper = looper;
            Field mInitializationLooperField = AudioRecord.class.getDeclaredField("mInitializationLooper");
            mInitializationLooperField.setAccessible(true);
            mInitializationLooperField.set(audioRecord, looper);

            if (AudioAttributesWrapper.getCapturePreset(attributes) == MediaRecorder.AudioSource.REMOTE_SUBMIX) {
                AudioAttributes.Builder audioAttributeBuilder = new AudioAttributes.Builder(attributes);

                final Iterator<String> tagsIter = AudioAttributesWrapper.getTags(attributes).iterator();
                while (tagsIter.hasNext()) {
                    String tag = tagsIter.next();
                    if (tag.equalsIgnoreCase(SUBMIX_FIXED_VOLUME)) {
                        // audioRecord.mIsSubmixFullVolume = true;
                        Field mIsSubmixFullVolumeField = AudioRecord.class.getDeclaredField("mIsSubmixFullVolume");
                        mIsSubmixFullVolumeField.setAccessible(true);
                        mIsSubmixFullVolumeField.set(audioRecord, true);
                    } else {
                        AudioAttributesWrapper.Builder.addTag(audioAttributeBuilder, tag);
                    }
                }

                AudioAttributesWrapper.Builder.setInternalCapturePreset(audioAttributeBuilder,
                        AudioAttributesWrapper.getCapturePreset(attributes));
                attributes = audioAttributeBuilder.build();
            }

            // audioRecord.mAudioAttributes = attributes;
            Field mAudioAttributesField = AudioRecord.class.getDeclaredField("mAudioAttributes");
            mAudioAttributesField.setAccessible(true);
            mAudioAttributesField.set(audioRecord, attributes);

            int rate = format.getSampleRate();
            if (rate == AudioFormat.SAMPLE_RATE_UNSPECIFIED) {
                rate = 0;
            }

            int encoding = AudioFormat.ENCODING_DEFAULT;
            if ((AudioFormatWrapper.getPropertySetMask(format)
                    & AudioFormatWrapper.AUDIO_FORMAT_HAS_PROPERTY_ENCODING) != 0) {
                encoding = format.getEncoding();
            }

            // audioRecord.audioParamCheck(capturePreset, rate, encoding);
            Method audioParamCheckMethod = AudioRecord.class.getDeclaredMethod("audioParamCheck", int.class, int.class,
                    int.class);
            audioParamCheckMethod.setAccessible(true);
            audioParamCheckMethod.invoke(audioRecord, AudioAttributesWrapper.getCapturePreset(attributes), rate,
                    encoding);

            int mChannelIndexMask = 0;
            int mChannelMask = 0;
            int mChannelCount = 0;

            if ((AudioFormatWrapper.getPropertySetMask(format)
                    & AudioFormatWrapper.AUDIO_FORMAT_HAS_PROPERTY_CHANNEL_INDEX_MASK) != 0) {
                mChannelIndexMask = format.getChannelIndexMask();
                mChannelCount = format.getChannelCount();
            }
            if ((AudioFormatWrapper.getPropertySetMask(format)
                    & AudioFormatWrapper.AUDIO_FORMAT_HAS_PROPERTY_CHANNEL_MASK) != 0) {
                mChannelMask = getChannelMaskFromLegacyConfig(format.getChannelMask(), false);
                mChannelCount = format.getChannelCount();
            } else {
                mChannelMask = getChannelMaskFromLegacyConfig(AudioFormat.CHANNEL_IN_DEFAULT, false);
                mChannelCount = AudioFormatWrapper.channelCountFromInChannelMask(mChannelMask);
            }

            Field mChannelIndexMaskField = AudioRecord.class.getDeclaredField("mChannelIndexMask");
            mChannelIndexMaskField.setAccessible(true);
            mChannelIndexMaskField.set(audioRecord, mChannelIndexMask);

            Field mChannelMaskField = AudioRecord.class.getDeclaredField("mChannelMask");
            mChannelMaskField.setAccessible(true);
            mChannelMaskField.set(audioRecord, mChannelMask);

            Field mChannelCountField = AudioRecord.class.getDeclaredField("mChannelCount");
            mChannelCountField.setAccessible(true);
            mChannelCountField.set(audioRecord, mChannelCount);

            // audioRecord.audioBuffSizeCheck(bufferSizeInBytes)
            Method audioBuffSizeCheckMethod = AudioRecord.class.getDeclaredMethod("audioBuffSizeCheck", int.class);
            audioBuffSizeCheckMethod.setAccessible(true);
            audioBuffSizeCheckMethod.invoke(audioRecord, bufferSizeInBytes);

            int[] sampleRate = new int[] { 0 };
            int[] session = new int[] { sessionId };

            int initResult;
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
                // private native final int native_setup(Object audiorecord_this,
                // Object /*AudioAttributes*/ attributes,
                // int[] sampleRate, int channelMask, int channelIndexMask, int audioFormat,
                // int buffSizeInBytes, int[] sessionId, String opPackageName,
                // long nativeRecordInJavaObj);
                Method nativeSetupMethod = AudioRecord.class.getDeclaredMethod("native_setup", Object.class,
                        Object.class, int[].class, int.class, int.class, int.class, int.class, int[].class,
                        String.class, long.class);
                nativeSetupMethod.setAccessible(true);
                initResult = (int) nativeSetupMethod.invoke(audioRecord, new WeakReference<AudioRecord>(audioRecord),
                        attributes, sampleRate, mChannelMask, mChannelIndexMask, audioRecord.getAudioFormat(),
                        bufferSizeInBytes, session, getCurrentOpPackageName(audioRecord), 0L);
            } else {
                AttributionSource attributionSource = context != null ? context.getAttributionSource()
                        : AttributionSource.myAttributionSource();

                if (attributionSource.getPackageName() == null) {
                    // Command line utility
                    // attributionSource = attributionSource.withPackageName("uid:" +
                    // Binder.getCallingUid());
                    Method withPackageNameMethod = AttributionSource.class.getDeclaredMethod("withPackageName",
                            String.class);
                    withPackageNameMethod.setAccessible(true);
                    attributionSource = (AttributionSource) withPackageNameMethod.invoke(attributionSource,
                            "uid:" + Binder.getCallingUid());
                }

                // ScopedParcelState attributionSourceState =
                // attributionSource.asScopedParcelState()
                Method asScopedParcelStateMethod = AttributionSource.class.getDeclaredMethod("asScopedParcelState");
                asScopedParcelStateMethod.setAccessible(true);

                try (AutoCloseable attributionSourceState = (AutoCloseable) asScopedParcelStateMethod
                        .invoke(attributionSource)) {
                    Method getParcelMethod = attributionSourceState.getClass().getDeclaredMethod("getParcel");
                    Parcel attributionSourceParcel = (Parcel) getParcelMethod.invoke(attributionSourceState);

                    // private native int native_setup(Object audiorecordThis,
                    // Object /*AudioAttributes*/ attributes,
                    // int[] sampleRate, int channelMask, int channelIndexMask, int audioFormat,
                    // int buffSizeInBytes, int[] sessionId, @NonNull Parcel attributionSource,
                    // long nativeRecordInJavaObj, int maxSharedAudioHistoryMs);
                    Method nativeSetupMethod = AudioRecord.class.getDeclaredMethod("native_setup", Object.class,
                            Object.class, int[].class, int.class, int.class, int.class, int.class, int[].class,
                            Parcel.class, long.class, int.class);
                    nativeSetupMethod.setAccessible(true);
                    initResult = (int) nativeSetupMethod.invoke(audioRecord,
                            new WeakReference<AudioRecord>(audioRecord), attributes, sampleRate, mChannelMask,
                            mChannelIndexMask, audioRecord.getAudioFormat(), bufferSizeInBytes, session,
                            attributionSourceParcel, 0L, maxSharedAudioHistoryMs);
                }
            }

            if (initResult != AudioRecord.SUCCESS) {
                Ln.e("Error code " + initResult + " when initializing native AudioRecord object.");
                return audioRecord;
            }

            // mSampleRate = sampleRate[0]
            Field mSampleRateField = AudioRecord.class.getDeclaredField("mSampleRate");
            mSampleRateField.setAccessible(true);
            mSampleRateField.set(audioRecord, sampleRate[0]);

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
            throw e;
        }
    }

    public static final int PRIVACY_SENSITIVE_DEFAULT = -1;
    public static final int PRIVACY_SENSITIVE_DISABLED = 0;
    public static final int PRIVACY_SENSITIVE_ENABLED = 1;

    @SuppressLint({ "BlockedPrivateApi" })
    public static AudioRecord build(AudioRecord.Builder builder) {
        try {
            Field mFormatField = AudioRecord.Builder.class.getDeclaredField("mFormat");
            mFormatField.setAccessible(true);
            AudioFormat mFormat = (AudioFormat) mFormatField.get(builder);

            if (mFormat == null) {
                mFormat = new AudioFormat.Builder()
                        .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                        .setChannelMask(AudioFormat.CHANNEL_IN_MONO)
                        .build();
            } else {
                if (mFormat.getEncoding() == AudioFormat.ENCODING_INVALID) {
                    mFormat = new AudioFormat.Builder(mFormat)
                            .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                            .build();
                }
                if (mFormat.getChannelMask() == AudioFormat.CHANNEL_INVALID
                        && mFormat.getChannelIndexMask() == AudioFormat.CHANNEL_INVALID) {
                    mFormat = new AudioFormat.Builder(mFormat)
                            .setChannelMask(AudioFormat.CHANNEL_IN_MONO)
                            .build();
                }
            }

            Field mAttributesField = AudioRecord.Builder.class.getDeclaredField("mAttributes");
            mAttributesField.setAccessible(true);
            AudioAttributes mAttributes = (AudioAttributes) mAttributesField.get(builder);
            if (mAttributes == null) {
                AudioAttributes.Builder audioAttributesBuilder = new AudioAttributes.Builder();
                AudioAttributesWrapper.Builder.setInternalCapturePreset(audioAttributesBuilder,
                        MediaRecorder.AudioSource.DEFAULT);
                mAttributes = audioAttributesBuilder.build();
            }

            Field mPrivacySensitiveField = AudioRecord.Builder.class.getDeclaredField("mPrivacySensitive");
            mPrivacySensitiveField.setAccessible(true);
            int mPrivacySensitive = (int) mPrivacySensitiveField.get(builder);
            if (mPrivacySensitive != PRIVACY_SENSITIVE_DEFAULT) {
                int source = AudioAttributesWrapper.getCapturePreset(mAttributes);
                if (source == MediaRecorder.AudioSource.REMOTE_SUBMIX
                        || source == MediaRecorder.AudioSource.VOICE_DOWNLINK
                        || source == MediaRecorder.AudioSource.VOICE_UPLINK
                        || source == MediaRecorder.AudioSource.VOICE_CALL) {
                    throw new UnsupportedOperationException(
                            "Cannot request private capture with source: " + source);
                }

                AudioAttributes.Builder audioAttributesBuilder = new AudioAttributes.Builder();
                AudioAttributesWrapper.Builder.setInternalCapturePreset(audioAttributesBuilder, source);
                AudioAttributesWrapper.Builder.setPrivacySensitive(audioAttributesBuilder,
                        mPrivacySensitive == PRIVACY_SENSITIVE_ENABLED);
                mAttributes = audioAttributesBuilder.build();
            }

            Field mBufferSizeInBytesField = AudioRecord.Builder.class.getDeclaredField("mBufferSizeInBytes");
            mBufferSizeInBytesField.setAccessible(true);
            int mBufferSizeInBytes = (int) mBufferSizeInBytesField.get(builder);

            // If the buffer size is not specified,
            // use a single frame for the buffer size and let the
            // native code figure out the minimum buffer size.
            if (mBufferSizeInBytes == 0) {
                mBufferSizeInBytes = mFormat.getChannelCount()
                        * AudioFormatWrapper.getBytesPerSample(mFormat, mFormat.getEncoding());
            }

            Field mSessionIdField = AudioRecord.Builder.class.getDeclaredField("mSessionId");
            mSessionIdField.setAccessible(true);
            int mSessionId = (int) mSessionIdField.get(builder);

            Context mContext;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                Field mContextField = AudioRecord.Builder.class.getDeclaredField("mContext");
                mContextField.setAccessible(true);
                mContext = (Context) mContextField.get(builder);
            } else {
                mContext = null;
            }

            final AudioRecord record = newInstance(
                    mAttributes, mFormat, mBufferSizeInBytes, mSessionId, mContext, 0);
            if (record.getState() == AudioRecord.STATE_UNINITIALIZED) {
                // release is not necessary
                throw new UnsupportedOperationException("Cannot create AudioRecord");
            }
            return record;
        } catch (Exception e) {
            throw new UnsupportedOperationException("Cannot create AudioRecord", e);
        }
    }
}
