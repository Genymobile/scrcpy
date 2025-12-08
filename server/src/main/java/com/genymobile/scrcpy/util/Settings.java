package com.genymobile.scrcpy.util;

import com.genymobile.scrcpy.wrappers.ContentProvider;
import com.genymobile.scrcpy.wrappers.ServiceManager;

import android.os.Process;

public final class Settings {

    public static final String TABLE_SYSTEM = ContentProvider.TABLE_SYSTEM;
    public static final String TABLE_SECURE = ContentProvider.TABLE_SECURE;
    public static final String TABLE_GLOBAL = ContentProvider.TABLE_GLOBAL;

    private Settings() {
        /* not instantiable */
    }

    public static String getValue(String table, String key) throws SettingsException {
        try (ContentProvider provider = ServiceManager.getActivityManager().createSettingsProvider()) {
            return provider.getValue(table, key);
        } catch (Exception e) {
            throw new SettingsException("getValue", table, key, null, e);
        }
    }

    public static void putValue(String table, String key, String value) throws SettingsException {
        if (Process.myUid() == 1000) {
            // UID_SYSTEM path: fallback to 'su -c settings put ...'
            try {
                String cmd = "settings put " + table + " " + key + " " + value;
                String[] command = { "su", "-c", cmd };

                java.lang.Process proc = Runtime.getRuntime().exec(command);
                int exit = proc.waitFor();

                if (exit != 0) {
                    throw new SettingsException("putValue", table, key, value, null);
                }

            } catch (Exception e) {
                throw new SettingsException("putValue", table, key, value, e);
            }
        } else {
            // AID_SHELL path: use standard provider
            try (ContentProvider provider = ServiceManager.getActivityManager().createSettingsProvider()) {
                provider.putValue(table, key, value);
            } catch (Exception e) {
                throw new SettingsException("putValue", table, key, value, e);
            }
        }
    }

    public static String getAndPutValue(String table, String key, String value) throws SettingsException {
        String oldValue = getValue(table, key);
        if (!value.equals(oldValue)) {
            putValue(table, key, value);
        }
        return oldValue;
    }
}
