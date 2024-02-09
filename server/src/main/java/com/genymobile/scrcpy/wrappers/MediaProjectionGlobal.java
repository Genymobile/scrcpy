package com.genymobile.scrcpy.wrappers;

import android.hardware.display.VirtualDisplay;
import android.view.Surface;
import com.genymobile.scrcpy.Ln;
import java.lang.reflect.Method;
import android.os.Build;

/**
 * A wrapper for system API android.media.projection.MediaProjectionGlobal.
 */
public final class MediaProjectionGlobal {

  private static java.lang.Class<?> MediaProjectionGlobalClass;
  private static Method getInstanceMethod;
  private static Method createVirtualDisplayMethod;

  static {
    try {
      MediaProjectionGlobalClass = java.lang.Class.forName("android.media.projection.MediaProjectionGlobal");
    } catch (ClassNotFoundException e) {
      MediaProjectionGlobalClass = null;
    }
  }

  private static Method getGetInstanceMethod() throws NoSuchMethodException {
    if (MediaProjectionGlobalClass == null) {
      throw new NullPointerException("MediaProjectionGlobalClass is null");
    }
    if (getInstanceMethod == null) {
      getInstanceMethod = MediaProjectionGlobalClass.getMethod("getInstance");
    }
    return getInstanceMethod;
  }

  // String name, int width, int height, int displayIdToMirror
  private static Method getCreateVirtualDisplayMethod() throws NoSuchMethodException {
    if (MediaProjectionGlobalClass == null) {
      throw new NullPointerException("MediaProjectionGlobalClass is null");
    }
    if (createVirtualDisplayMethod == null) {
      createVirtualDisplayMethod =
          MediaProjectionGlobalClass.getMethod(
              "createVirtualDisplay", String.class, int.class, int.class, int.class, Surface.class);
    }
    return createVirtualDisplayMethod;
  }

  public static VirtualDisplay createVirtualDisplay(
      String name, int width, int height, int displayIdToMirror, Surface surface)
      throws NoSuchMethodException {
    try {
      Object instance = getGetInstanceMethod().invoke(null);
      if (instance == null) {
        Ln.e("instance == null!");
      }
      return (VirtualDisplay)
          getCreateVirtualDisplayMethod()
              .invoke(instance, name, width, height, displayIdToMirror, surface);
    } catch (ReflectiveOperationException e) {
      Ln.e("Could not invoke method", e);
      return null;
    }
  }

  private MediaProjectionGlobal() {}
}
