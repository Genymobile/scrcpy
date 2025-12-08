package com.genymobile.scrcpy;

import com.genymobile.scrcpy.wrappers.ServiceManager;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.AttributionSource;
import android.content.ContentResolver;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.IContentProvider;
import android.os.Binder;
import android.os.Process;

import java.lang.reflect.Field;

public final class FakeContext extends ContextWrapper {

    public static final String PACKAGE_NAME = "com.android.shell";
    public static final int ROOT_UID = 0; // Like android.os.Process.ROOT_UID, but before API 29

    private static final FakeContext INSTANCE = new FakeContext();

    /**
     * Returns the appropriate FakeContext:
     * - UID 1000 (system) -> impersonates 'android'
     * - others -> normal shell context
     */
    public static FakeContext get() {
        int uid = Process.myUid();
        if (uid == 1000) { // UID_SYSTEM
            // Return a context that impersonates the 'android' package
            return new FakeContext(Workarounds.getSystemContext());
        }
        // Normal shell context
        return INSTANCE;
    }

    private final ContentResolver contentResolver = new ContentResolver(this) {
        @SuppressWarnings({"unused", "ProtectedMemberInFinalClass"})
        protected IContentProvider acquireProvider(Context c, String name) {
            return ServiceManager.getActivityManager().getContentProviderExternal(name, new Binder());
        }

        @SuppressWarnings("unused")
        public boolean releaseProvider(IContentProvider icp) {
            return false;
        }

        @SuppressWarnings({"unused", "ProtectedMemberInFinalClass"})
        protected IContentProvider acquireUnstableProvider(Context c, String name) {
            return null;
        }

        @SuppressWarnings("unused")
        public boolean releaseUnstableProvider(IContentProvider icp) {
            return false;
        }

        @SuppressWarnings("unused")
        public void unstableProviderDied(IContentProvider icp) {
            // ignore
        }
    };

    // Default constructor for INSTANCE
    private FakeContext() {
        super(Workarounds.getSystemContext());
    }

    // Constructor for custom base context (e.g., UID 1000)
    private FakeContext(Context base) {
        super(base);
    }

    @Override
    public String getPackageName() {
        return PACKAGE_NAME;
    }

    @Override
    public String getOpPackageName() {
        return PACKAGE_NAME;
    }

    @TargetApi(AndroidVersions.API_31_ANDROID_12)
    @Override
    public AttributionSource getAttributionSource() {
        AttributionSource.Builder builder = new AttributionSource.Builder(Process.SHELL_UID);
        builder.setPackageName(PACKAGE_NAME);
        return builder.build();
    }

    @SuppressWarnings("unused")
    public int getDeviceId() {
        return 0;
    }

    @Override
    public Context getApplicationContext() {
        return this;
    }

    @Override
    public Context createPackageContext(String packageName, int flags) {
        return this;
    }

    @Override
    public ContentResolver getContentResolver() {
        return contentResolver;
    }

    @SuppressLint("SoonBlockedPrivateApi")
    @Override
    public Object getSystemService(String name) {
        Object service = super.getSystemService(name);
        if (service == null) {
            return null;
        }

        // "semclipboard" is a Samsung-internal service
        if (Context.CLIPBOARD_SERVICE.equals(name) || "semclipboard".equals(name)) {
            try {
                Field field = service.getClass().getDeclaredField("mContext");
                field.setAccessible(true);
                field.set(service, this);
            } catch (ReflectiveOperationException e) {
                throw new RuntimeException(e);
            }
        }

        return service;
    }
}
