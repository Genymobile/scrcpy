package com.genymobile.scrcpy.android;

import java.util.regex.Pattern;

final class AndroidClientValidation {
    private static final Pattern PACKAGE_NAME = Pattern.compile(
            "[A-Za-z][A-Za-z0-9_]*(\\.[A-Za-z][A-Za-z0-9_]*)*");
    private static final Pattern HOST_NAME = Pattern.compile("[A-Za-z0-9._:%\\[\\]-]+");

    private AndroidClientValidation() {
    }

    static boolean isValidPackageName(String packageName) {
        return packageName != null && packageName.length() <= 255
                && PACKAGE_NAME.matcher(packageName).matches();
    }

    static boolean isValidProfile(String name, String host, int port, String appPackage) {
        return isValidName(name) && isValidHost(host) && port >= 1 && port <= 65535
                && (appPackage == null || appPackage.isEmpty() || isValidPackageName(appPackage));
    }

    static boolean isValidHost(String host) {
        return host != null && !host.isEmpty() && host.length() <= 253
                && HOST_NAME.matcher(host).matches();
    }

    private static boolean isValidName(String name) {
        if (name == null || name.trim().isEmpty() || name.length() > 64) {
            return false;
        }
        for (int i = 0; i < name.length(); ++i) {
            if (Character.isISOControl(name.charAt(i))) {
                return false;
            }
        }
        return true;
    }
}
