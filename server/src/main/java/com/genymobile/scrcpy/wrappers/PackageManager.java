package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.Ln;

import android.content.pm.ApplicationInfo;
import android.os.IInterface;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class PackageManager {

    private IInterface manager;
    private Method getApplicationInfoMethod;

    public PackageManager(IInterface manager) {
        this.manager = manager;
    }

    private Method getGetApplicationInfoMethod() throws NoSuchMethodException {
        if (getApplicationInfoMethod == null) {
            getApplicationInfoMethod = manager.getClass().getDeclaredMethod("getApplicationInfo", String.class, int.class, int.class);
        }
        return getApplicationInfoMethod;
    }

    public ApplicationInfo getApplicationInfo(String packageName) {
        try {
            Method method = getGetApplicationInfoMethod();
            return (ApplicationInfo) method.invoke(manager, packageName, 0, ServiceManager.USER_ID);
        } catch (NoSuchMethodException | InvocationTargetException | IllegalAccessException e) {
            Ln.e("Cannot get application info", e);
            return null;
        }
    }
}
