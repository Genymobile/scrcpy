package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.Ln;

import android.os.Bundle;
import android.os.IBinder;

import java.io.Closeable;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class ContentProvider implements Closeable {

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
    private boolean callMethodLegacy;

    ContentProvider(ActivityManager manager, Object provider, String name, IBinder token) {
        this.manager = manager;
        this.provider = provider;
        this.name = name;
        this.token = token;
    }

    private Method getCallMethod() throws NoSuchMethodException {
        if (callMethod == null) {
            try {
                callMethod = provider.getClass().getMethod("call", String.class, String.class, String.class, String.class, Bundle.class);
            } catch (NoSuchMethodException e) {
                // old version
                callMethod = provider.getClass().getMethod("call", String.class, String.class, String.class, Bundle.class);
                callMethodLegacy = true;
            }
        }
        return callMethod;
    }

    private Bundle call(String callMethod, String arg, Bundle extras) {
        try {
            Method method = getCallMethod();
            Object[] args;
            if (!callMethodLegacy) {
                args = new Object[]{ServiceManager.PACKAGE_NAME, "settings", callMethod, arg, extras};
            } else {
                args = new Object[]{ServiceManager.PACKAGE_NAME, callMethod, arg, extras};
            }
            return (Bundle) method.invoke(provider, args);
        } catch (InvocationTargetException | IllegalAccessException | NoSuchMethodException e) {
            Ln.e("Could not invoke method", e);
            return null;
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

    public String getValue(String table, String key) {
        String method = getGetMethod(table);
        Bundle arg = new Bundle();
        arg.putInt(CALL_METHOD_USER_KEY, ServiceManager.USER_ID);
        Bundle bundle = call(method, key, arg);
        if (bundle == null) {
            return null;
        }
        return bundle.getString("value");
    }

    public void putValue(String table, String key, String value) {
        String method = getPutMethod(table);
        Bundle arg = new Bundle();
        arg.putInt(CALL_METHOD_USER_KEY, ServiceManager.USER_ID);
        arg.putString(NAME_VALUE_TABLE_VALUE, value);
        call(method, key, arg);
    }

    public String getAndPutValue(String table, String key, String value) {
        String oldValue = getValue(table, key);
        if (!value.equals(oldValue)) {
            putValue(table, key, value);
        }
        return oldValue;
    }
}
