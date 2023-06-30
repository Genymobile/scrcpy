package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.FakeContext;
import com.genymobile.scrcpy.Ln;
import com.genymobile.scrcpy.SettingsException;

import android.annotation.SuppressLint;
import android.content.AttributionSource;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;

import java.io.Closeable;
import java.lang.reflect.Method;

public final class ContentProvider implements Closeable {

    public static final String TABLE_SYSTEM = "system";
    public static final String TABLE_SECURE = "secure";
    public static final String TABLE_GLOBAL = "global";

    // See android/providerHolder/Settings.java
    private static final String CALL_METHOD_GET_SYSTEM = "GET_system";
    private static final String CALL_METHOD_GET_SECURE = "GET_secure";
    private static final String CALL_METHOD_GET_GLOBAL = "GET_global";

    private static final String CALL_METHOD_PUT_SYSTEM = "PUT_system";
    private static final String CALL_METHOD_PUT_SECURE = "PUT_secure";
    private static final String CALL_METHOD_PUT_GLOBAL = "PUT_global";

    private static final String CALL_METHOD_USER_KEY = "_user";

    private static final String NAME_VALUE_TABLE_VALUE = "value";

    private final ActivityManager manager;
    // android.content.IContentProvider
    private final Object provider;
    private final String name;
    private final IBinder token;

    private Method callMethod;
    private int callMethodVersion;

    ContentProvider(ActivityManager manager, Object provider, String name, IBinder token) {
        this.manager = manager;
        this.provider = provider;
        this.name = name;
        this.token = token;
    }

    @SuppressLint("PrivateApi")
    private Method getCallMethod() throws NoSuchMethodException {
        if (callMethod == null) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                callMethod = provider.getClass().getMethod("call", AttributionSource.class, String.class, String.class, String.class, Bundle.class);
                callMethodVersion = 0;
            } else {
                // old versions
                try {
                    callMethod = provider.getClass()
                            .getMethod("call", String.class, String.class, String.class, String.class, String.class, Bundle.class);
                    callMethodVersion = 1;
                } catch (NoSuchMethodException e1) {
                    try {
                        callMethod = provider.getClass().getMethod("call", String.class, String.class, String.class, String.class, Bundle.class);
                        callMethodVersion = 2;
                    } catch (NoSuchMethodException e2) {
                        callMethod = provider.getClass().getMethod("call", String.class, String.class, String.class, Bundle.class);
                        callMethodVersion = 3;
                    }
                }
            }
        }
        return callMethod;
    }

    private Bundle call(String callMethod, String arg, Bundle extras) throws ReflectiveOperationException {
        try {
            Method method = getCallMethod();
            Object[] args;

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S && callMethodVersion == 0) {
                args = new Object[]{FakeContext.get().getAttributionSource(), "settings", callMethod, arg, extras};
            } else {
                switch (callMethodVersion) {
                    case 1:
                        args = new Object[]{FakeContext.PACKAGE_NAME, null, "settings", callMethod, arg, extras};
                        break;
                    case 2:
                        args = new Object[]{FakeContext.PACKAGE_NAME, "settings", callMethod, arg, extras};
                        break;
                    default:
                        args = new Object[]{FakeContext.PACKAGE_NAME, callMethod, arg, extras};
                        break;
                }
            }
            return (Bundle) method.invoke(provider, args);
        } catch (ReflectiveOperationException e) {
            Ln.e("Could not invoke method", e);
            throw e;
        }
    }

    public void close() {
        manager.removeContentProviderExternal(name, token);
    }

    private static String getGetMethod(String table) {
        switch (table) {
            case TABLE_SECURE:
                return CALL_METHOD_GET_SECURE;
            case TABLE_SYSTEM:
                return CALL_METHOD_GET_SYSTEM;
            case TABLE_GLOBAL:
                return CALL_METHOD_GET_GLOBAL;
            default:
                throw new IllegalArgumentException("Invalid table: " + table);
        }
    }

    private static String getPutMethod(String table) {
        switch (table) {
            case TABLE_SECURE:
                return CALL_METHOD_PUT_SECURE;
            case TABLE_SYSTEM:
                return CALL_METHOD_PUT_SYSTEM;
            case TABLE_GLOBAL:
                return CALL_METHOD_PUT_GLOBAL;
            default:
                throw new IllegalArgumentException("Invalid table: " + table);
        }
    }

    public String getValue(String table, String key) throws SettingsException {
        String method = getGetMethod(table);
        Bundle arg = new Bundle();
        arg.putInt(CALL_METHOD_USER_KEY, FakeContext.ROOT_UID);
        try {
            Bundle bundle = call(method, key, arg);
            if (bundle == null) {
                return null;
            }
            return bundle.getString("value");
        } catch (Exception e) {
            throw new SettingsException(table, "get", key, null, e);
        }

    }

    public void putValue(String table, String key, String value) throws SettingsException {
        String method = getPutMethod(table);
        Bundle arg = new Bundle();
        arg.putInt(CALL_METHOD_USER_KEY, FakeContext.ROOT_UID);
        arg.putString(NAME_VALUE_TABLE_VALUE, value);
        try {
            call(method, key, arg);
        } catch (Exception e) {
            throw new SettingsException(table, "put", key, value, e);
        }
    }
}
