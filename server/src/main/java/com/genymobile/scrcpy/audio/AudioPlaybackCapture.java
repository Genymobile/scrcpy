package com.genymobile.scrcpy.audio;

import com.genymobile.scrcpy.AndroidVersions;
import com.genymobile.scrcpy.FakeContext;
import com.genymobile.scrcpy.util.Ln;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.Context;
import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.MediaCodec;
import android.os.Build;

import java.lang.reflect.Method;
import java.nio.ByteBuffer;

public final class AudioPlaybackCapture implements AudioCapture {

    private final boolean keepPlayingOnDevice;

    private AudioRecord recorder;
    private AudioRecordReader reader;

    public AudioPlaybackCapture(boolean keepPlayingOnDevice) {
        this.keepPlayingOnDevice = keepPlayingOnDevice;
    }

    private int targetUid = -1;
    private final Object restartLock = new Object();
    private boolean restartRequested = false;

    public void setTargetUid(int uid) {
        synchronized (restartLock) {
            if (this.targetUid != uid) {
                Ln.i("AudioPlaybackCapture: setTargetUid changed from " + this.targetUid + " to " + uid);
                this.targetUid = uid;
                restartRequested = true;
                if (recorder != null) {
                    recorder.release();
                }
            }
        }
    }

    private Object currentAudioPolicy;

    @SuppressLint("PrivateApi")
    private AudioRecord createAudioRecord() throws AudioCaptureException {
        // See <https://github.com/Genymobile/scrcpy/issues/4380>
        try {
            Class<?> audioMixingRuleClass = Class.forName("android.media.audiopolicy.AudioMixingRule");
            Class<?> audioMixingRuleBuilderClass = Class.forName("android.media.audiopolicy.AudioMixingRule$Builder");

            // AudioMixingRule.Builder audioMixingRuleBuilder = new AudioMixingRule.Builder();
            Object audioMixingRuleBuilder = audioMixingRuleBuilderClass.getConstructor().newInstance();

            // audioMixingRuleBuilder.setTargetMixRole(AudioMixingRule.MIX_ROLE_PLAYERS);
            int mixRolePlayersConstant = audioMixingRuleClass.getField("MIX_ROLE_PLAYERS").getInt(null);
            Method setTargetMixRoleMethod = audioMixingRuleBuilderClass.getMethod("setTargetMixRole", int.class);
            setTargetMixRoleMethod.invoke(audioMixingRuleBuilder, mixRolePlayersConstant);

            // audioMixingRuleBuilder.addMixRule(AudioMixingRule.RULE_MATCH_ATTRIBUTE_USAGE, attributes);
            int ruleMatchAttributeUsageConstant = audioMixingRuleClass.getField("RULE_MATCH_ATTRIBUTE_USAGE").getInt(null);
            Method addMixRuleMethod = audioMixingRuleBuilderClass.getMethod("addMixRule", int.class, Object.class);

            if (targetUid >= 0) {
                // audioMixingRuleBuilder.addMixRule(AudioMixingRule.RULE_MATCH_UID, targetUid);
                int ruleMatchUidConstant;
                try {
                     ruleMatchUidConstant = audioMixingRuleClass.getField("RULE_MATCH_UID").getInt(null);
                } catch (Exception e) {
                     ruleMatchUidConstant = 4; // RULE_MATCH_UID = 4
                }
                addMixRuleMethod.invoke(audioMixingRuleBuilder, ruleMatchUidConstant, Integer.valueOf(targetUid));
                Ln.i("Added UID rule for targetUid: " + targetUid);

                for (int usage = 0; usage <= 16; usage++) {
                    try {
                        AudioAttributes attributes = new AudioAttributes.Builder().setUsage(usage).build();
                        addMixRuleMethod.invoke(audioMixingRuleBuilder, ruleMatchAttributeUsageConstant, attributes);
                    } catch (Exception e) {
                        // Ignore invalid usages if any
                    }
                }
                Ln.i("Added explicit usage rules (0-16) for targetUid: " + targetUid);
            } else {
                for (int usage = 0; usage <= 16; usage++) {
                    try {
                        AudioAttributes attributes = new AudioAttributes.Builder().setUsage(usage).build();
                        addMixRuleMethod.invoke(audioMixingRuleBuilder, ruleMatchAttributeUsageConstant, attributes);
                    } catch (Exception e) {
                        // Ignore invalid usages if any
                    }
                }
                Ln.i("No target UID set, global audio capture enabled (All Usages 0-16)");
            }

            // AudioMixingRule audioMixingRule = builder.build();
            Object audioMixingRule = audioMixingRuleBuilderClass.getMethod("build").invoke(audioMixingRuleBuilder);

            // audioMixingRuleBuilder.voiceCommunicationCaptureAllowed(true);
            Method voiceCommunicationCaptureAllowedMethod = audioMixingRuleBuilderClass.getMethod("voiceCommunicationCaptureAllowed", boolean.class);
            voiceCommunicationCaptureAllowedMethod.invoke(audioMixingRuleBuilder, true);

            Class<?> audioMixClass = Class.forName("android.media.audiopolicy.AudioMix");
            Class<?> audioMixBuilderClass = Class.forName("android.media.audiopolicy.AudioMix$Builder");

            // AudioMix.Builder audioMixBuilder = new AudioMix.Builder(audioMixingRule);
            Object audioMixBuilder = audioMixBuilderClass.getConstructor(audioMixingRuleClass).newInstance(audioMixingRule);

            // audioMixBuilder.setFormat(createAudioFormat());
            Method setFormat = audioMixBuilder.getClass().getMethod("setFormat", AudioFormat.class);
            setFormat.invoke(audioMixBuilder, AudioConfig.createAudioFormat());

            String routeFlagName = keepPlayingOnDevice ? "ROUTE_FLAG_LOOP_BACK_RENDER" : "ROUTE_FLAG_LOOP_BACK";
            int routeFlags = audioMixClass.getField(routeFlagName).getInt(null);

            // audioMixBuilder.setRouteFlags(routeFlag);
            Method setRouteFlags = audioMixBuilder.getClass().getMethod("setRouteFlags", int.class);
            setRouteFlags.invoke(audioMixBuilder, routeFlags);

            // AudioMix audioMix = audioMixBuilder.build();
            Object audioMix = audioMixBuilderClass.getMethod("build").invoke(audioMixBuilder);

            Class<?> audioPolicyClass = Class.forName("android.media.audiopolicy.AudioPolicy");
            Class<?> audioPolicyBuilderClass = Class.forName("android.media.audiopolicy.AudioPolicy$Builder");

            // AudioPolicy.Builder audioPolicyBuilder = new AudioPolicy.Builder();
            Object audioPolicyBuilder = audioPolicyBuilderClass.getConstructor(Context.class).newInstance(FakeContext.get());

            // audioPolicyBuilder.addMix(audioMix);
            Method addMixMethod = audioPolicyBuilderClass.getMethod("addMix", audioMixClass);
            addMixMethod.invoke(audioPolicyBuilder, audioMix);

            // AudioPolicy audioPolicy = audioPolicyBuilder.build();
            Object audioPolicy = audioPolicyBuilderClass.getMethod("build").invoke(audioPolicyBuilder);

            // AudioManager.registerAudioPolicyStatic(audioPolicy);
            Method registerAudioPolicyStaticMethod = AudioManager.class.getDeclaredMethod("registerAudioPolicyStatic", audioPolicyClass);
            registerAudioPolicyStaticMethod.setAccessible(true);
            int result = (int) registerAudioPolicyStaticMethod.invoke(null, audioPolicy);
            if (result != 0) {
                throw new RuntimeException("registerAudioPolicy() returned " + result);
            }
            this.currentAudioPolicy = audioPolicy; // Store it!

            // audioPolicy.createAudioRecordSink(audioPolicy);
            Method createAudioRecordSinkClass = audioPolicyClass.getMethod("createAudioRecordSink", audioMixClass);
            return (AudioRecord) createAudioRecordSinkClass.invoke(audioPolicy, audioMix);
        } catch (Exception e) {
            Ln.e("Could not capture audio playback", e);
            throw new AudioCaptureException();
        }
    }

    private void unregisterAudioPolicy() {
         if (currentAudioPolicy != null) {
             try {
                 Ln.d("Unregistering AudioPolicy");
                 Class<?> audioPolicyClass = Class.forName("android.media.audiopolicy.AudioPolicy");
                 Method unregisterMethod = AudioManager.class.getDeclaredMethod("unregisterAudioPolicyAsyncStatic", audioPolicyClass);
                 unregisterMethod.setAccessible(true);
                 unregisterMethod.invoke(null, currentAudioPolicy);
             } catch (Exception e) {
                 Ln.w("Could not unregister audio policy", e);
             }
             currentAudioPolicy = null;
         }
    }

    @Override
    public void checkCompatibility() throws AudioCaptureException {
        if (Build.VERSION.SDK_INT < AndroidVersions.API_33_ANDROID_13) {
            Ln.w("Audio disabled: audio playback capture source not supported before Android 13");
            throw new AudioCaptureException();
        }
    }

    @Override
    public void start() throws AudioCaptureException {
        unregisterAudioPolicy();
        try {
            // Give some time for the async unregistration to settle
            Thread.sleep(100);
        } catch (InterruptedException e) {
            // ignore
        }
        recorder = createAudioRecord();
        recorder.startRecording();
        reader = new AudioRecordReader(recorder);
    }

    @Override
    public void stop() {
        if (recorder != null) {
            // Will call .stop() if necessary, without throwing an IllegalStateException
            recorder.release();
            recorder = null; // Important to set null so check in restart works
        }
        unregisterAudioPolicy(); // Cleanup!
    }

    @Override
    @TargetApi(AndroidVersions.API_24_ANDROID_7_0)
    public int read(ByteBuffer outDirectBuffer, MediaCodec.BufferInfo outBufferInfo) {
        synchronized (restartLock) {
            if (restartRequested) {
                try {
                    start();
                    restartRequested = false;
                } catch (AudioCaptureException e) {
                    Ln.e("And restart failed", e);
                    return -1;
                }
            }
        }
        if (reader == null) {
            return -1;
        }
        int r = reader.read(outDirectBuffer, outBufferInfo);
        if (r < 0) {
            synchronized (restartLock) {
                if (restartRequested) {
                    // try to restart immediately to avoid returning error
                   return read(outDirectBuffer, outBufferInfo);
                }
            }
        }
        if (r == 0) {
            try {
                Thread.sleep(10);
            } catch (InterruptedException e) {
                return -1;
            }
            return read(outDirectBuffer, outBufferInfo);
        }
        return r;
    }
}
