package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.FakeContext;
import com.genymobile.scrcpy.Ln;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.Intent;
import android.os.Binder;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.os.IInterface;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

@SuppressLint("PrivateApi,DiscouragedPrivateApi")
public final class ActivityManager {

    private final IInterface manager;
    private Method getContentProviderExternalMethod;
    private boolean getContentProviderExternalMethodNewVersion = true;
    private Method removeContentProviderExternalMethod;
    private Method startActivityAsUserWithFeatureMethod;
    private Method forceStopPackageMethod;

    public ActivityManager(IInterface manager) {
        this.manager = manager;
    }

    private Method getGetContentProviderExternalMethod() throws NoSuchMethodException {
        if (getContentProviderExternalMethod == null) {
            try {
                getContentProviderExternalMethod = manager.getClass()
                        .getMethod("getContentProviderExternal", String.class, int.class, IBinder.class, String.class);
            } catch (NoSuchMethodException e) {
                // old version
                getContentProviderExternalMethod = manager.getClass().getMethod("getContentProviderExternal", String.class, int.class, IBinder.class);
                getContentProviderExternalMethodNewVersion = false;
            }
        }
        return getContentProviderExternalMethod;
    }

    private Method getRemoveContentProviderExternalMethod() throws NoSuchMethodException {
        if (removeContentProviderExternalMethod == null) {
            removeContentProviderExternalMethod = manager.getClass().getMethod("removeContentProviderExternal", String.class, IBinder.class);
        }
        return removeContentProviderExternalMethod;
    }

    @TargetApi(Build.VERSION_CODES.Q)
    private ContentProvider getContentProviderExternal(String name, IBinder token) {
        try {
            Method method = getGetContentProviderExternalMethod();
            Object[] args;
            if (getContentProviderExternalMethodNewVersion) {
                // new version
                args = new Object[]{name, FakeContext.ROOT_UID, token, null};
            } else {
                // old version
                args = new Object[]{name, FakeContext.ROOT_UID, token};
            }
            // ContentProviderHolder providerHolder = getContentProviderExternal(...);
            Object providerHolder = method.invoke(manager, args);
            if (providerHolder == null) {
                return null;
            }
            // IContentProvider provider = providerHolder.provider;
            Field providerField = providerHolder.getClass().getDeclaredField("provider");
            providerField.setAccessible(true);
            Object provider = providerField.get(providerHolder);
            if (provider == null) {
                return null;
            }
            return new ContentProvider(this, provider, name, token);
        } catch (InvocationTargetException | IllegalAccessException | NoSuchMethodException | NoSuchFieldException e) {
            Ln.e("Could not invoke method", e);
            return null;
        }
    }

    void removeContentProviderExternal(String name, IBinder token) {
        try {
            Method method = getRemoveContentProviderExternalMethod();
            method.invoke(manager, name, token);
        } catch (InvocationTargetException | IllegalAccessException | NoSuchMethodException e) {
            Ln.e("Could not invoke method", e);
        }
    }

    public ContentProvider createSettingsProvider() {
        return getContentProviderExternal("settings", new Binder());
    }

    private Method getStartActivityAsUserWithFeatureMethod() throws NoSuchMethodException, ClassNotFoundException {
        if (startActivityAsUserWithFeatureMethod == null) {
            Class<?> iApplicationThreadClass = Class.forName("android.app.IApplicationThread");
            Class<?> profilerInfo = Class.forName("android.app.ProfilerInfo");
            startActivityAsUserWithFeatureMethod = manager.getClass()
                    .getMethod("startActivityAsUserWithFeature", iApplicationThreadClass, String.class, String.class, Intent.class, String.class,
                            IBinder.class, String.class, int.class, int.class, profilerInfo, Bundle.class, int.class);
        }
        return startActivityAsUserWithFeatureMethod;
    }

    @SuppressWarnings("ConstantConditions")
    public int startActivityAsUserWithFeature(Intent intent) {
        try {
            Method method = getStartActivityAsUserWithFeatureMethod();
            return (int) method.invoke(
                    /* this */ manager,
                    /* caller */ null,
                    /* callingPackage */ FakeContext.PACKAGE_NAME,
                    /* callingFeatureId */ null,
                    /* intent */ intent,
                    /* resolvedType */ null,
                    /* resultTo */ null,
                    /* resultWho */ null,
                    /* requestCode */ 0,
                    /* startFlags */ 0,
                    /* profilerInfo */ null,
                    /* bOptions */ null,
                    /* userId */ /* UserHandle.USER_CURRENT */ -2);
        } catch (Throwable e) {
            Ln.e("Could not invoke method", e);
            return 0;
        }
    }

    private Method getForceStopPackageMethod() throws NoSuchMethodException {
        if (forceStopPackageMethod == null) {
            forceStopPackageMethod = manager.getClass().getMethod("forceStopPackage", String.class, int.class);
        }
        return forceStopPackageMethod;
    }

    public void forceStopPackage(String packageName) {
        try {
            Method method = getForceStopPackageMethod();
            method.invoke(manager, packageName, /* userId */ /* UserHandle.USER_CURRENT */ -2);
        } catch (Throwable e) {
            Ln.e("Could not invoke method", e);
        }
    }
}
