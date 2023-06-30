package com.genymobile.scrcpy;

import android.annotation.TargetApi;
import android.content.AttributionSource;
import android.content.Context;
import android.content.ContextWrapper;
import android.os.Build;
import android.os.Process;
import android.system.Os;

public final class FakeContext extends ContextWrapper {

    public static final String PACKAGE_SHELL = "com.android.shell";
    public static final int ROOT_UID = 0; // Like android.os.Process.ROOT_UID, but before API 29

    private static final FakeContext INSTANCE = new FakeContext();

    public static FakeContext get() {
        return INSTANCE;
    }

    private FakeContext() {
        super(Workarounds.getSystemContext());
    }

    @Override
    public String getPackageName() {
        return getPackageNameStatic();
    }

    @Override
    public String getOpPackageName() {
        return getPackageNameStatic();
    }

    public static String getPackageNameStatic() {
        if (Os.getuid() == 1000) {
            return "android";
        }
        return PACKAGE_SHELL;
    }

    @TargetApi(Build.VERSION_CODES.S)
    @Override
    public AttributionSource getAttributionSource() {
        AttributionSource.Builder builder = new AttributionSource.Builder(Process.SHELL_UID);
        builder.setPackageName(PACKAGE_SHELL);
        if (Os.getuid() == 1000) {
            builder.setPackageName("android");
        }
        return builder.build();
    }

    // @Override to be added on SDK upgrade for Android 14
    @SuppressWarnings("unused")
    public int getDeviceId() {
        return 0;
    }

    @Override
    public Context getApplicationContext() {
        return this;
    }
}
