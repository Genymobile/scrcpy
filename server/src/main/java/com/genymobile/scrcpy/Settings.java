package com.genymobile.scrcpy;

import com.genymobile.scrcpy.wrappers.ContentProvider;
import com.genymobile.scrcpy.wrappers.ServiceManager;

public class Settings {

    public static final String TABLE_SYSTEM = ContentProvider.TABLE_SYSTEM;
    public static final String TABLE_SECURE = ContentProvider.TABLE_SECURE;
    public static final String TABLE_GLOBAL = ContentProvider.TABLE_GLOBAL;

    private final ServiceManager serviceManager;

    public Settings(ServiceManager serviceManager) {
        this.serviceManager = serviceManager;
    }

    public String getValue(String table, String key) throws SettingsException {
        try (ContentProvider provider = serviceManager.getActivityManager().createSettingsProvider()) {
            return provider.getValue(table, key);
        }
    }

    public void putValue(String table, String key, String value) throws SettingsException {
        try (ContentProvider provider = serviceManager.getActivityManager().createSettingsProvider()) {
            provider.putValue(table, key, value);
        }
    }

    public String getAndPutValue(String table, String key, String value) throws SettingsException {
        try (ContentProvider provider = serviceManager.getActivityManager().createSettingsProvider()) {
            String oldValue = provider.getValue(table, key);
            if (!value.equals(oldValue)) {
                provider.putValue(table, key, value);
            }
            return oldValue;
        }
    }
}
