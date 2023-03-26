package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.Ln;

import android.media.AudioAttributes;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.Set;

public class AudioAttributesWrapper {
    private static Method getCapturePresetMethod;
    private static Method setCapturePresetMethod;
    private static Method getTagsMethod;

    private static Method getGetCapturePresetMethod() throws NoSuchMethodException {
        if (getCapturePresetMethod == null) {
            getCapturePresetMethod = AudioAttributes.class.getMethod("getCapturePreset");
        }
        return getCapturePresetMethod;
    }

    public static int getCapturePreset(AudioAttributes audioAttributes)
            throws IllegalAccessException, IllegalArgumentException, InvocationTargetException, NoSuchMethodException {
        try {
            return (int) getGetCapturePresetMethod().invoke(audioAttributes);
        } catch (IllegalAccessException | IllegalArgumentException | InvocationTargetException
                | NoSuchMethodException e) {
            Ln.e("Failed to invoke AudioAttributes.getCapturePreset()", e);
            throw e;
        }
    }

    private static Method getSetCapturePresetMethod() throws NoSuchMethodException {
        if (setCapturePresetMethod == null) {
            setCapturePresetMethod = AudioAttributes.class.getMethod("setCapturePreset", int.class);
        }
        return setCapturePresetMethod;
    }

    public static void setCapturePreset(AudioAttributes audioAttributes, int preset)
            throws IllegalAccessException, IllegalArgumentException, InvocationTargetException, NoSuchMethodException {
        try {
            getSetCapturePresetMethod().invoke(audioAttributes, preset);
        } catch (IllegalAccessException | IllegalArgumentException | InvocationTargetException
                | NoSuchMethodException e) {
            Ln.e("Failed to invoke AudioAttributes.setCapturePreset()", e);
            throw e;
        }
    }

    private static Method getGetTagsMethod() throws NoSuchMethodException {
        if (getTagsMethod == null) {
            getTagsMethod = AudioAttributes.class.getMethod("getTags");
        }
        return getTagsMethod;
    }

    @SuppressWarnings("unchecked")
    public static Set<String> getTags(AudioAttributes audioAttributes)
            throws IllegalAccessException, IllegalArgumentException, InvocationTargetException, NoSuchMethodException {
        try {
            return (Set<String>) getGetTagsMethod().invoke(audioAttributes);
        } catch (IllegalAccessException | IllegalArgumentException | InvocationTargetException
                | NoSuchMethodException e) {
            Ln.e("Failed to invoke AudioAttributes.getTags()", e);
            throw e;
        }
    }

    public static class Builder {
        private static Method setInternalCapturePresetMethod;
        private static Method setPrivacySensitiveMethod;
        private static Method addTagMethod;

        private static Method getSetInternalCapturePresetMethod() throws NoSuchMethodException {
            if (setInternalCapturePresetMethod == null) {
                setInternalCapturePresetMethod = AudioAttributes.Builder.class.getMethod("setInternalCapturePreset",
                        int.class);
            }
            return setInternalCapturePresetMethod;
        }

        public static void setInternalCapturePreset(AudioAttributes.Builder builder, int preset)
                throws IllegalAccessException, IllegalArgumentException, InvocationTargetException,
                NoSuchMethodException {
            try {
                getSetInternalCapturePresetMethod().invoke(builder, preset);
            } catch (IllegalAccessException | IllegalArgumentException | InvocationTargetException
                    | NoSuchMethodException e) {
                Ln.e("Failed to invoke AudioAttributes.Builder.setInternalCapturePreset()", e);
                throw e;
            }
        }

        private static Method getSetPrivacySensitiveMethod() throws NoSuchMethodException {
            if (setPrivacySensitiveMethod == null) {
                setPrivacySensitiveMethod = AudioAttributes.Builder.class.getMethod("setPrivacySensitive",
                        boolean.class);
            }
            return setPrivacySensitiveMethod;
        }

        public static void setPrivacySensitive(AudioAttributes.Builder builder, boolean privacySensitive)
                throws IllegalAccessException, IllegalArgumentException, InvocationTargetException,
                NoSuchMethodException {
            try {
                getSetPrivacySensitiveMethod().invoke(builder, privacySensitive);
            } catch (IllegalAccessException | IllegalArgumentException | InvocationTargetException
                    | NoSuchMethodException e) {
                Ln.e("Failed to invoke AudioAttributes.Builder.setPrivacySensitive()", e);
                throw e;
            }
        }

        private static Method getAddTagMethod() throws NoSuchMethodException {
            if (addTagMethod == null) {
                addTagMethod = AudioAttributes.Builder.class.getMethod("addTag", String.class);
            }
            return addTagMethod;
        }

        public static void addTag(AudioAttributes.Builder builder, String tag)
                throws IllegalAccessException, IllegalArgumentException, InvocationTargetException,
                NoSuchMethodException {
            try {
                getAddTagMethod().invoke(builder, tag);
            } catch (IllegalAccessException | IllegalArgumentException | InvocationTargetException
                    | NoSuchMethodException e) {
                Ln.e("Failed to invoke AudioAttributes.Builder.addTag()", e);
                throw e;
            }
        }
    }
}
