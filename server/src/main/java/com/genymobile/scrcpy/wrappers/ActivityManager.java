package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.Ln;

import android.os.Binder;
import android.os.IBinder;
import android.os.IInterface;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class ActivityManager {

    private final IInterface manager;
    private Method getContentProviderExternalMethod;
    private boolean getContentProviderExternalMethodLegacy;
    private Method removeContentProviderExternalMethod;

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
                getContentProviderExternalMethodLegacy = true;
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

    private ContentProvider getContentProviderExternal(String name, IBinder token) {
        try {
            Method method = getGetContentProviderExternalMethod();
            Object[] args;
            if (!getContentProviderExternalMethodLegacy) {
                // new version
                args = new Object[]{name, ServiceManager.USER_ID, token, null};
            } else {
                // old version
                args = new Object[]{name, ServiceManager.USER_ID, token};
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
}
