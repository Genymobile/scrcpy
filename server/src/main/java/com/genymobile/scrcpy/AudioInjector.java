package com.genymobile.scrcpy;

import com.genymobile.scrcpy.util.Ln;
import com.genymobile.scrcpy.audio.LatestAudioBuffer;

import android.annotation.SuppressLint;
import android.content.Context;
import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.media.MediaRecorder;
import android.os.Build;

import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.util.Objects;

/**
 * Injects audio from the client computer into the Android device's microphone
 * using AudioPolicy APIs via reflection.
 *
 * Based on: https://github.com/Genymobile/scrcpy/issues/3880#issuecomment-1595722119
 */
public final class AudioInjector {

    private AudioInjector() {
    }

    private static AudioAttributes createAudioAttributes(int capturePreset) throws Exception {
        AudioAttributes.Builder audioAttributesBuilder = new AudioAttributes.Builder();
        Method setCapturePresetMethod =
            audioAttributesBuilder.getClass().getDeclaredMethod("setCapturePreset", int.class);
        setCapturePresetMethod.invoke(audioAttributesBuilder, capturePreset);
        return audioAttributesBuilder.build();
    }

    private static void addCapturePreset(Object builder, Method addMixRuleMethod, int rule, int preset) throws Exception {
        addMixRuleMethod.invoke(builder, rule, createAudioAttributes(preset));
    }

    private static AudioTrack createAudioTrack(Object audioPolicy, Object audioMix) throws Exception {
        Method createAudioTrackSourceMethod = audioPolicy.getClass()
                        .getDeclaredMethod("createAudioTrackSource", audioMix.getClass());
        AudioTrack audioTrack = (AudioTrack) createAudioTrackSourceMethod.invoke(audioPolicy, audioMix);
        Objects.requireNonNull(audioTrack);
        try {
            audioTrack.play();
            return audioTrack;
        } catch (Exception e) {
            audioTrack.release();
            throw e;
        }
    }

    private static void unregisterAudioPolicy(AudioManager audioManager, Object audioPolicy) {
        try {
            Method unregisterAudioPolicyMethod = audioManager.getClass()
                            .getDeclaredMethod("unregisterAudioPolicy", audioPolicy.getClass());
            unregisterAudioPolicyMethod.invoke(audioManager, audioPolicy);
        } catch (Exception e) {
            Ln.w("Could not unregister client audio policy", e);
        }
    }

    /**
     * Injects audio from a bounded latest-sample buffer into the device's microphone.
     *
     * @param pcm The PCM audio data to inject
     * @throws Exception if audio injection setup fails
     */
    public static void injectAudio(LatestAudioBuffer pcm, Runnable onFailure) throws Exception {
        if (Build.VERSION.SDK_INT < AndroidVersions.API_33_ANDROID_13) {
            throw new UnsupportedOperationException("Client audio injection requires Android 13 or newer");
        }

        Context systemContext = Workarounds.getSystemContext();
        Objects.requireNonNull(systemContext);

        // var audioMixRuleBuilder = new AudioMixingRule.Builder();
        @SuppressLint("PrivateApi")
        Class<?> audioMixRuleBuilderClass =
                        Class.forName("android.media.audiopolicy.AudioMixingRule$Builder");
        Object audioMixRuleBuilder = audioMixRuleBuilderClass.getDeclaredConstructor().newInstance();

        try {
            // Added in Android 13, but previous versions don't work because lack of permission.
            // audioMixRuleBuilder.setTargetMixRole(MIX_ROLE_INJECTOR);
            Method setTargetMixRoleMethod =
                            audioMixRuleBuilder.getClass().getDeclaredMethod("setTargetMixRole", int.class);
            int mixRoleInjector = 1;
            setTargetMixRoleMethod.invoke(audioMixRuleBuilder, mixRoleInjector);
        } catch (Exception ignored) {
        }

        Method addMixRuleMethod = audioMixRuleBuilder.getClass()
                        .getDeclaredMethod("addMixRule", int.class, Object.class);
        int ruleMatchAttributeCapturePreset = 0x1 << 1;

        // Add mix rules for various capture presets to intercept all microphone capture
        addCapturePreset(audioMixRuleBuilder, addMixRuleMethod, ruleMatchAttributeCapturePreset, MediaRecorder.AudioSource.DEFAULT);
        addCapturePreset(audioMixRuleBuilder, addMixRuleMethod, ruleMatchAttributeCapturePreset, MediaRecorder.AudioSource.MIC);
        addCapturePreset(audioMixRuleBuilder, addMixRuleMethod, ruleMatchAttributeCapturePreset, MediaRecorder.AudioSource.VOICE_COMMUNICATION);
        addCapturePreset(audioMixRuleBuilder, addMixRuleMethod, ruleMatchAttributeCapturePreset, MediaRecorder.AudioSource.UNPROCESSED);
        // Recorder and speech apps commonly choose these presets instead of MIC.
        // They are still microphone capture paths and should receive the operator
        // audio while passthrough is explicitly enabled.
        addCapturePreset(audioMixRuleBuilder, addMixRuleMethod, ruleMatchAttributeCapturePreset, MediaRecorder.AudioSource.CAMCORDER);
        addCapturePreset(audioMixRuleBuilder, addMixRuleMethod, ruleMatchAttributeCapturePreset, MediaRecorder.AudioSource.VOICE_RECOGNITION);
        addCapturePreset(audioMixRuleBuilder, addMixRuleMethod, ruleMatchAttributeCapturePreset, MediaRecorder.AudioSource.VOICE_PERFORMANCE);

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
        int routeFlagLoopBack = 0x1 << 1;
        setRouteFlagsMethod.invoke(audioMixBuilder, routeFlagLoopBack);

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

        AudioManager audioManager = (AudioManager) systemContext.getSystemService(AudioManager.class);
        Objects.requireNonNull(audioManager);

        // audioManager.registerAudioPolicy(audioPolicy);
        Method registerAudioPolicyMethod = audioManager.getClass()
                        .getDeclaredMethod("registerAudioPolicy", audioPolicy.getClass());
        // noinspection DataFlowIssue
        int result = (int) registerAudioPolicyMethod.invoke(audioManager, audioPolicy);

        if (result != 0) {
            throw new IllegalStateException("registerAudioPolicy failed with status " + result);
        }

        AudioTrack audioTrack;
        try {
            audioTrack = createAudioTrack(audioPolicy, audioMix);
        } catch (Exception e) {
            unregisterAudioPolicy(audioManager, audioPolicy);
            throw e;
        }

        new Thread(() -> {
            byte[] audioBuffer = new byte[4096];
            try {
                while (true) {
                    int bytesRead = pcm.read(audioBuffer);
                    if (bytesRead <= 0) {
                        break;
                    }
                    int written = audioTrack.write(audioBuffer, 0, bytesRead);
                    if (written < 0) {
                        throw new IllegalStateException("AudioTrack write failed with status " + written);
                    }
                    if (written != bytesRead) {
                        Ln.w("Client audio short write: " + written + "/" + bytesRead + " bytes");
                    }
                }
            } catch (Exception e) {
                Ln.e("Audio injection error", e);
                onFailure.run();
            } finally {
                try {
                    audioTrack.stop();
                } catch (Exception e) {
                    // It may already be stopped after an AudioTrack failure.
                }
                audioTrack.release();
                unregisterAudioPolicy(audioManager, audioPolicy);
            }
        }, "client-audio-injector").start();
    }
}
