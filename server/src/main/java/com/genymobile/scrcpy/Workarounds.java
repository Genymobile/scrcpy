package com.genymobile.scrcpy;

import android.annotation.SuppressLint;
import android.app.Application;
import android.content.ContextWrapper;
import android.content.pm.ApplicationInfo;
import android.os.Looper;

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;

public final class Workarounds {

    private static Class<?> activityThreadClass;
    private static Object activityThread;

    private Workarounds() {
        // not instantiable
    }

    @SuppressWarnings("deprecation")
    public static void prepareMainLooper() {
        // Some devices internally create a Handler when creating an input Surface, causing an exception:
        //   "Can't create handler inside thread that has not called Looper.prepare()"
        // <https://github.com/Genymobile/scrcpy/issues/240>
        //
        // Use Looper.prepareMainLooper() instead of Looper.prepare() to avoid a NullPointerException:
        //   "Attempt to read from field 'android.os.MessageQueue android.os.Looper.mQueue'
        //    on a null object reference"
        // <https://github.com/Genymobile/scrcpy/issues/921>
        Looper.prepareMainLooper();
    }

    @SuppressLint("PrivateApi,DiscouragedPrivateApi")
    private static void fillActivityThread() throws Exception {
        if (activityThread == null) {
            // ActivityThread activityThread = new ActivityThread();
            activityThreadClass = Class.forName("android.app.ActivityThread");
            Constructor<?> activityThreadConstructor = activityThreadClass.getDeclaredConstructor();
            activityThreadConstructor.setAccessible(true);
            activityThread = activityThreadConstructor.newInstance();

            // ActivityThread.sCurrentActivityThread = activityThread;
            Field sCurrentActivityThreadField = activityThreadClass.getDeclaredField("sCurrentActivityThread");
            sCurrentActivityThreadField.setAccessible(true);
            sCurrentActivityThreadField.set(null, activityThread);
        }
    }

    @SuppressLint("PrivateApi,DiscouragedPrivateApi")
    public static void fillAppInfo() {
        try {
            fillActivityThread();

            // ActivityThread.AppBindData appBindData = new ActivityThread.AppBindData();
            Class<?> appBindDataClass = Class.forName("android.app.ActivityThread$AppBindData");
            Constructor<?> appBindDataConstructor = appBindDataClass.getDeclaredConstructor();
            appBindDataConstructor.setAccessible(true);
            Object appBindData = appBindDataConstructor.newInstance();

            ApplicationInfo applicationInfo = new ApplicationInfo();
            applicationInfo.packageName = FakeContext.PACKAGE_NAME;

            // appBindData.appInfo = applicationInfo;
            Field appInfoField = appBindDataClass.getDeclaredField("appInfo");
            appInfoField.setAccessible(true);
            appInfoField.set(appBindData, applicationInfo);

            // activityThread.mBoundApplication = appBindData;
            Field mBoundApplicationField = activityThreadClass.getDeclaredField("mBoundApplication");
            mBoundApplicationField.setAccessible(true);
            mBoundApplicationField.set(activityThread, appBindData);
        } catch (Throwable throwable) {
            // this is a workaround, so failing is not an error
            Ln.d("Could not fill app info: " + throwable.getMessage());
        }
    }

    @SuppressLint("PrivateApi,DiscouragedPrivateApi")
    public static void fillAppContext() {
        try {
            fillActivityThread();

            Application app = Application.class.newInstance();
            Field baseField = ContextWrapper.class.getDeclaredField("mBase");
            baseField.setAccessible(true);
            baseField.set(app, FakeContext.get());

            // activityThread.mInitialApplication = app;
            Field mInitialApplicationField = activityThreadClass.getDeclaredField("mInitialApplication");
            mInitialApplicationField.setAccessible(true);
            mInitialApplicationField.set(activityThread, app);
        } catch (Throwable throwable) {
            // this is a workaround, so failing is not an error
            Ln.d("Could not fill app context: " + throwable.getMessage());
        }
    }
}
