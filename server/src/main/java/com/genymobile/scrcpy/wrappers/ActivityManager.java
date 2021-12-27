package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.Ln;

import android.content.Intent;
import android.os.Binder;
import android.os.Bundle;
import android.os.IBinder;
import android.os.IInterface;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class ActivityManager {

    private final IInterface manager;
    private Method getContentProviderExternalMethod;
    private boolean getContentProviderExternalMethodNewVersion = true;
    private Method removeContentProviderExternalMethod;
    private Method broadcastIntentMethod;

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

    private Method getBroadcastIntentMethod() throws NoSuchMethodException {
        if (broadcastIntentMethod == null) {
            try {
                Class<?> iApplicationThreadClass = Class.forName("android.app.IApplicationThread");
                Class<?> iIntentReceiverClass = Class.forName("android.content.IIntentReceiver");
                broadcastIntentMethod = manager.getClass()
                        .getMethod("broadcastIntent", iApplicationThreadClass, Intent.class, String.class, iIntentReceiverClass, int.class,
                                String.class, Bundle.class, String[].class, int.class, Bundle.class, boolean.class, boolean.class, int.class);
                return broadcastIntentMethod;
            } catch (ClassNotFoundException e) {
                throw new AssertionError(e);
            }
        }
        return broadcastIntentMethod;
    }

    private ContentProvider getContentProviderExternal(String name, IBinder token) {
        try {
            Method method = getGetContentProviderExternalMethod();
            Object[] args;
            if (getContentProviderExternalMethodNewVersion) {
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

    public void sendBroadcast(Intent intent) {
        try {
            Method method = getBroadcastIntentMethod();
            method.invoke(manager, null, intent, null, null, 0, null, null, null, -1, null, true, false, ServiceManager.USER_ID);
        } catch (InvocationTargetException | IllegalAccessException | NoSuchMethodException e) {
            Ln.e("Could not invoke method", e);
        }
    }
}
