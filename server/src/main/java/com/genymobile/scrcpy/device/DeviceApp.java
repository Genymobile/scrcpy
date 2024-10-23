package com.genymobile.scrcpy.device;

public final class DeviceApp {

    private final String packageName;
    private final String name;
    private final boolean system;

    public DeviceApp(String packageName, String name, boolean system) {
        this.packageName = packageName;
        this.name = name;
        this.system = system;
    }

    public String getPackageName() {
        return packageName;
    }

    public String getName() {
        return name;
    }

    public boolean isSystem() {
        return system;
    }
}
