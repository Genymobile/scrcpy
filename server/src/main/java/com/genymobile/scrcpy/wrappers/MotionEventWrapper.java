package com.genymobile.scrcpy.wrappers;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

import android.view.MotionEvent;

public final class MotionEventWrapper {
   static public MotionEvent obtain(long downTime, long eventTime,
            int action, int pointerCount, MotionEvent.PointerProperties[] pointerProperties,
            MotionEvent.PointerCoords[] pointerCoords, int metaState, int buttonState,
            float xPrecision, float yPrecision, int deviceId,
            int edgeFlags, int source, int displayId, int flags) {
        MotionEvent motionEvent;
        try {
            // Modern Android 10 (29+ API), Android R has new displayId parameter, to support multi-display touch
            motionEvent = (MotionEvent)MotionEvent.class.getMethod("obtain",
                    long.class, long.class,
                    int.class, int.class, MotionEvent.PointerProperties[].class,
                    MotionEvent.PointerCoords[].class, int.class, int.class,
                    float.class, float.class, int.class,
                    int.class, int.class, int.class, int.class)
                .invoke(null, downTime, eventTime, action, pointerCount, pointerProperties, pointerCoords, metaState, buttonState, xPrecision, yPrecision, deviceId, edgeFlags, source, displayId, flags);
        } catch (InvocationTargetException | IllegalAccessException | NoSuchMethodException e) {
            motionEvent = MotionEvent.obtain(downTime, eventTime, action, pointerCount, pointerProperties, pointerCoords, metaState, buttonState, xPrecision, yPrecision, deviceId, edgeFlags, source, flags);
        }
        return motionEvent;
    }
}
