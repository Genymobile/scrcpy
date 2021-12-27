package com.genymobile.scrcpy;

public class SettingsException extends Exception {
    private static String createMessage(String method, String table, String key, String value) {
        return "Could not access settings: " + method + " " + table + " " + key + (value != null ? " " + value : "");
    }

    public SettingsException(String method, String table, String key, String value, Throwable cause) {
        super(createMessage(method, table, key, value), cause);
    }
}
