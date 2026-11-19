package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.AndroidVersions;
import com.genymobile.scrcpy.FakeContext;
import com.genymobile.scrcpy.util.Ln;
import com.genymobile.scrcpy.util.SettingsException;

import android.annotation.SuppressLint;
import android.content.IContentProvider;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;

import java.io.Closeable;

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

    private static final int CALL_METHOD_VERSION_UNKNOWN = -1;
    private static final int CALL_METHOD_VERSION_ATTRIBUTION_SOURCE = 0;
    private static final int CALL_METHOD_VERSION_ATTRIBUTION_TAG = 1;
    private static final int CALL_METHOD_VERSION_AUTHORITY = 2;
    private static final int CALL_METHOD_VERSION_LEGACY = 3;

    private final ActivityManager manager;
    private final IContentProvider provider;
    private final String name;
    private final IBinder token;

    private int callMethodVersion = CALL_METHOD_VERSION_UNKNOWN;

    ContentProvider(ActivityManager manager, IContentProvider provider, String name, IBinder token) {
        this.manager = manager;
        this.provider = provider;
        this.name = name;
        this.token = token;
    }

    @SuppressLint("PrivateApi")
    private Bundle call(String callMethod, String arg, Bundle extras) throws RemoteException {
        try {
            switch (callMethodVersion) {
                case CALL_METHOD_VERSION_ATTRIBUTION_SOURCE:
                    return provider.call(FakeContext.get().getAttributionSource(), "settings", callMethod, arg, extras);
                case CALL_METHOD_VERSION_ATTRIBUTION_TAG:
                    return provider.call(FakeContext.PACKAGE_NAME, null, "settings", callMethod, arg, extras);
                case CALL_METHOD_VERSION_AUTHORITY:
                    return provider.call(FakeContext.PACKAGE_NAME, "settings", callMethod, arg, extras);
                case CALL_METHOD_VERSION_LEGACY:
                    return provider.call(FakeContext.PACKAGE_NAME, callMethod, arg, extras);
                default:
                    break;
            }

            if (Build.VERSION.SDK_INT >= AndroidVersions.API_31_ANDROID_12) {
                Bundle result = provider.call(FakeContext.get().getAttributionSource(), "settings", callMethod, arg, extras);
                callMethodVersion = CALL_METHOD_VERSION_ATTRIBUTION_SOURCE;
                return result;
            }

            try {
                Bundle result = provider.call(FakeContext.PACKAGE_NAME, null, "settings", callMethod, arg, extras);
                callMethodVersion = CALL_METHOD_VERSION_ATTRIBUTION_TAG;
                return result;
            } catch (NoSuchMethodError e) {
                try {
                    Bundle result = provider.call(FakeContext.PACKAGE_NAME, "settings", callMethod, arg, extras);
                    callMethodVersion = CALL_METHOD_VERSION_AUTHORITY;
                    return result;
                } catch (NoSuchMethodError e2) {
                    Bundle result = provider.call(FakeContext.PACKAGE_NAME, callMethod, arg, extras);
                    callMethodVersion = CALL_METHOD_VERSION_LEGACY;
                    return result;
                }
            }
        } catch (RemoteException | RuntimeException | LinkageError e) {
            Ln.e("Could not call content provider", e);
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
        } catch (Exception | LinkageError e) {
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
        } catch (Exception | LinkageError e) {
            throw new SettingsException(table, "put", key, value, e);
        }
    }
}
