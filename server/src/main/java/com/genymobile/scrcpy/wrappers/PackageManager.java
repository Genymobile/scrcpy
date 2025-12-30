package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.util.Ln;

import android.annotation.SuppressLint;
import android.content.pm.ApplicationInfo;
import android.os.IBinder;
import android.os.IInterface;

import java.lang.reflect.Method;

@SuppressLint("PrivateApi,DiscouragedPrivateApi")
public final class PackageManager {

    private final IInterface manager;
    private Method getApplicationInfoMethod;

    static PackageManager create() {
        try {
            Class<?> serviceManagerClass = Class.forName("android.os.ServiceManager");
            Method getServiceMethod = serviceManagerClass.getDeclaredMethod("getService", String.class);
            IBinder binder = (IBinder) getServiceMethod.invoke(null, "package");
            Class<?> iPackageManagerClass = Class.forName("android.content.pm.IPackageManager");
            Class<?> iPackageManagerStubClass = Class.forName("android.content.pm.IPackageManager$Stub");
            Method asInterfaceMethod = iPackageManagerStubClass.getMethod("asInterface", IBinder.class);
            IInterface manager = (IInterface) asInterfaceMethod.invoke(null, binder);
            return new PackageManager(manager);
        } catch (Exception e) {
            Ln.e("Could not create PackageManager", e);
            throw new AssertionError(e);
        }
    }

    private PackageManager(IInterface manager) {
        this.manager = manager;
    }

    private Method getGetApplicationInfoMethod() throws NoSuchMethodException {
        if (getApplicationInfoMethod == null) {
            try {
                getApplicationInfoMethod = manager.getClass().getMethod("getApplicationInfo", String.class, long.class, int.class);
            } catch (NoSuchMethodException e) {
                try {
                    getApplicationInfoMethod = manager.getClass().getMethod("getApplicationInfo", String.class, int.class, int.class);
                } catch (NoSuchMethodException e2) {
                    throw e2;
                }
            }
        }
        return getApplicationInfoMethod;
    }

    public ApplicationInfo getApplicationInfo(String packageName, int flags, int userId) {
        try {
            Method method = getGetApplicationInfoMethod();
            Class<?>[] parameterTypes = method.getParameterTypes();
            if (parameterTypes[1] == long.class) {
                return (ApplicationInfo) method.invoke(manager, packageName, (long) flags, userId);
            } else {
                return (ApplicationInfo) method.invoke(manager, packageName, flags, userId);
            }
        } catch (Throwable e) {
            Ln.e("Could not invoke getApplicationInfo", e);
            return null;
        }
    }
}
