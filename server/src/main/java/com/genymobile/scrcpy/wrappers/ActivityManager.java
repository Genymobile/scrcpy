package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.AndroidVersions;
import com.genymobile.scrcpy.FakeContext;
import com.genymobile.scrcpy.util.Ln;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.IContentProvider;
import android.content.Intent;
import android.os.Binder;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.os.IInterface;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.List;

@SuppressLint("PrivateApi,DiscouragedPrivateApi")
public final class ActivityManager {

    private final IInterface manager;
    private Method getContentProviderExternalMethod;
    private boolean getContentProviderExternalMethodNewVersion = true;
    private Method removeContentProviderExternalMethod;
    private Method startActivityAsUserMethod;
    private Method forceStopPackageMethod;

    static ActivityManager create() {
        try {
            // On old Android versions, the ActivityManager is not exposed via AIDL,
            // so use ActivityManagerNative.getDefault()
            Class<?> cls = Class.forName("android.app.ActivityManagerNative");
            Method getDefaultMethod = cls.getDeclaredMethod("getDefault");
            IInterface am = (IInterface) getDefaultMethod.invoke(null);
            return new ActivityManager(am);
        } catch (ReflectiveOperationException e) {
            throw new AssertionError(e);
        }
    }

    private ActivityManager(IInterface manager) {
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

    @TargetApi(AndroidVersions.API_29_ANDROID_10)
    public IContentProvider getContentProviderExternal(String name, IBinder token) {
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
            return (IContentProvider) providerField.get(providerHolder);
        } catch (ReflectiveOperationException e) {
            Ln.e("Could not invoke method", e);
            return null;
        }
    }

    void removeContentProviderExternal(String name, IBinder token) {
        try {
            Method method = getRemoveContentProviderExternalMethod();
            method.invoke(manager, name, token);
        } catch (ReflectiveOperationException e) {
            Ln.e("Could not invoke method", e);
        }
    }

    public ContentProvider createSettingsProvider() {
        IBinder token = new Binder();
        IContentProvider provider = getContentProviderExternal("settings", token);
        if (provider == null) {
            return null;
        }
        return new ContentProvider(this, provider, "settings", token);
    }

    private Method getStartActivityAsUserMethod() throws NoSuchMethodException, ClassNotFoundException {
        if (startActivityAsUserMethod == null) {
            Class<?> iApplicationThreadClass = Class.forName("android.app.IApplicationThread");
            Class<?> profilerInfo = Class.forName("android.app.ProfilerInfo");
            startActivityAsUserMethod = manager.getClass()
                    .getMethod("startActivityAsUser", iApplicationThreadClass, String.class, Intent.class, String.class, IBinder.class, String.class,
                            int.class, int.class, profilerInfo, Bundle.class, int.class);
        }
        return startActivityAsUserMethod;
    }

    public int startActivity(Intent intent) {
        return startActivity(intent, null);
    }

    @SuppressWarnings("ConstantConditions")
    public int startActivity(Intent intent, Bundle options) {
        try {
            Method method = getStartActivityAsUserMethod();
            return (int) method.invoke(
                    /* this */ manager,
                    /* caller */ null,
                    /* callingPackage */ FakeContext.PACKAGE_NAME,
                    /* intent */ intent,
                    /* resolvedType */ null,
                    /* resultTo */ null,
                    /* resultWho */ null,
                    /* requestCode */ 0,
                    /* startFlags */ 0,
                    /* profilerInfo */ null,
                    /* bOptions */ options,
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

    @SuppressWarnings("unchecked")
    public String getTopPackage() {
        try {
            List<android.app.ActivityManager.RunningTaskInfo> tasks = null;
            IInterface target;
            if (Build.VERSION.SDK_INT >= 29) {
                target = ServiceManager.getService("activity_task", "android.app.IActivityTaskManager");
            } else {
                target = manager;
            }

            Method getTasksMethod = null;
            for (Method method : target.getClass().getMethods()) {
                if (method.getName().equals("getTasks")) {
                    getTasksMethod = method;
                    break;
                }
            }

            if (getTasksMethod != null) {
                Class<?>[] paramTypes = getTasksMethod.getParameterTypes();
                Object[] args = new Object[paramTypes.length];
                for (int i = 0; i < paramTypes.length; i++) {
                    Class<?> type = paramTypes[i];
                    if (type == int.class) {
                        args[i] = (i == 0) ? 1 : 0;
                    } else if (type == boolean.class) {
                        args[i] = false;
                    } else {
                        args[i] = null;
                    }
                }
                tasks = (List<android.app.ActivityManager.RunningTaskInfo>) getTasksMethod.invoke(target, args);
            }

            if (tasks != null && !tasks.isEmpty()) {
                android.app.ActivityManager.RunningTaskInfo task = tasks.get(0);
                if (task != null && task.topActivity != null) {
                    return task.topActivity.getPackageName();
                }
            }
        } catch (Throwable e) {
            Ln.e("Could not get top package name", e);
        }
        return null;
    }
}
