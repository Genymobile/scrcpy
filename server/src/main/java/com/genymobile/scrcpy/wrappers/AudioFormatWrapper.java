package com.genymobile.scrcpy.wrappers;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

import com.genymobile.scrcpy.Ln;

import android.media.AudioFormat;

public class AudioFormatWrapper {
    public final static int AUDIO_FORMAT_HAS_PROPERTY_NONE = 0x0;
    public final static int AUDIO_FORMAT_HAS_PROPERTY_ENCODING = 0x1 << 0;
    public final static int AUDIO_FORMAT_HAS_PROPERTY_SAMPLE_RATE = 0x1 << 1;
    public final static int AUDIO_FORMAT_HAS_PROPERTY_CHANNEL_MASK = 0x1 << 2;
    public final static int AUDIO_FORMAT_HAS_PROPERTY_CHANNEL_INDEX_MASK = 0x1 << 3;

    private static Method getPropertySetMaskMethod;
    private static Method channelCountFromInChannelMaskMethod;
    private static Method getBytesPerSampleMethod;

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

    private static Method getChannelCountFromInChannelMaskMethod() throws NoSuchMethodException {
        if (channelCountFromInChannelMaskMethod == null) {
            channelCountFromInChannelMaskMethod = AudioFormat.class.getMethod("channelCountFromInChannelMask",
                    int.class);
        }
        return channelCountFromInChannelMaskMethod;
    }

    public static int channelCountFromInChannelMask(int channelMask)
            throws IllegalAccessException, IllegalArgumentException, InvocationTargetException,
            NoSuchMethodException {
        try {
            return (int) getChannelCountFromInChannelMaskMethod().invoke(null, channelMask);
        } catch (IllegalAccessException | IllegalArgumentException | InvocationTargetException
                | NoSuchMethodException e) {
            Ln.e("Failed to invoke AudioFormat.channelCountFromInChannelMask()", e);
            throw e;
        }
    }

    private static Method getGetBytesPerSampleMethod() throws NoSuchMethodException {
        if (getBytesPerSampleMethod == null) {
            getBytesPerSampleMethod = AudioFormat.class.getMethod("getBytesPerSample", int.class);
        }
        return getBytesPerSampleMethod;
    }

    public static int getBytesPerSample(AudioFormat format, int encoding)
            throws IllegalAccessException, IllegalArgumentException, InvocationTargetException,
            NoSuchMethodException {
        try {
            return (int) getGetBytesPerSampleMethod().invoke(format, encoding);
        } catch (IllegalAccessException | IllegalArgumentException | InvocationTargetException
                | NoSuchMethodException e) {
            Ln.e("Failed to invoke AudioFormat.getBytesPerSample()", e);
            throw e;
        }
    }
}
