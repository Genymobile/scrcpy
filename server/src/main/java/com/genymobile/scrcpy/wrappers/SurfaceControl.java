package com.genymobile.scrcpy.wrappers;

import android.graphics.Rect;
import android.os.IBinder;
import android.view.Surface;

public class SurfaceControl {

    private static final Class<?> cls;

    static {
        try {
            cls = Class.forName("android.view.SurfaceControl");
        } catch (ClassNotFoundException e) {
            throw new AssertionError(e);
        }
    }

    private SurfaceControl() {
        // only static methods
    }

    public static void openTransaction() {
        try {
            cls.getMethod("openTransaction").invoke(null);
        } catch (Exception e) {
            throw new AssertionError(e);
        }
    }

    public static void closeTransaction() {
        try {
            cls.getMethod("closeTransaction").invoke(null);
        } catch (Exception e) {
            throw new AssertionError(e);
        }
    }

    public static void setDisplayProjection(IBinder displayToken, int orientation, Rect layerStackRect, Rect displayRect) {
        try {
            cls.getMethod("setDisplayProjection", IBinder.class, int.class, Rect.class, Rect.class)
                    .invoke(null, displayToken, orientation, layerStackRect, displayRect);
        } catch (Exception e) {
            throw new AssertionError(e);
        }
    }

    public static void setDisplayLayerStack(IBinder displayToken, int layerStack) {
        try {
            cls.getMethod("setDisplayLayerStack", IBinder.class, int.class).invoke(null, displayToken, layerStack);
        } catch (Exception e) {
            throw new AssertionError(e);
        }
    }

    public static void setDisplaySurface(IBinder displayToken, Surface surface) {
        try {
            cls.getMethod("setDisplaySurface", IBinder.class, Surface.class).invoke(null, displayToken, surface);
        } catch (Exception e) {
            throw new AssertionError(e);
        }
    }

    public static IBinder createDisplay(String name, boolean secure) {
        try {
            return (IBinder) cls.getMethod("createDisplay", String.class, boolean.class).invoke(null, name, secure);
        } catch (Exception e) {
            throw new AssertionError(e);
        }
    }

    public static void destroyDisplay(IBinder displayToken) {
        try {
            cls.getMethod("destroyDisplay", IBinder.class).invoke(null, displayToken);
        } catch (Exception e) {
            throw new AssertionError(e);
        }
    }
}
