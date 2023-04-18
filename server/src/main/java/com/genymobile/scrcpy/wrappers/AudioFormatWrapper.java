package com.genymobile.scrcpy.wrappers;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

import com.genymobile.scrcpy.Ln;

import android.media.AudioFormat;

public class AudioFormatWrapper {
    public final static int AUDIO_FORMAT_HAS_PROPERTY_CHANNEL_MASK = 0x1 << 2;
    public final static int AUDIO_FORMAT_HAS_PROPERTY_CHANNEL_INDEX_MASK = 0x1 << 3;

    private static Method getPropertySetMaskMethod;

    private static Method getGetPropertySetMaskMethod() throws NoSuchMethodException {
        if (getPropertySetMaskMethod == null) {
            getPropertySetMaskMethod = AudioFormat.class.getMethod("getPropertySetMask");
        }
        return getPropertySetMaskMethod;
    }

    public static int getPropertySetMask(AudioFormat audioFormat)
            throws IllegalAccessException, IllegalArgumentException, InvocationTargetException,
            NoSuchMethodException {
        try {
            return (int) getGetPropertySetMaskMethod().invoke(audioFormat);
        } catch (IllegalAccessException | IllegalArgumentException | InvocationTargetException
                | NoSuchMethodException e) {
            Ln.e("Failed to invoke AudioFormat.getPropertySetMask()", e);
            throw e;
        }
    }

}
