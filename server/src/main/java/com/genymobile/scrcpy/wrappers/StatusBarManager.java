package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.Ln;

import android.os.IInterface;

import java.lang.reflect.Method;

public final class StatusBarManager {

    private final IInterface manager;
    private Method expandNotificationsPanelMethod;
    private boolean expandNotificationPanelMethodCustomVersion;
    private Method expandSettingsPanelMethod;
    private boolean expandSettingsPanelMethodNewVersion = true;
    private Method collapsePanelsMethod;

    static StatusBarManager create() {
        IInterface manager = ServiceManager.getService("statusbar", "com.android.internal.statusbar.IStatusBarService");
        return new StatusBarManager(manager);
    }

    private StatusBarManager(IInterface manager) {
        this.manager = manager;
    }

    private Method getExpandNotificationsPanelMethod() throws NoSuchMethodException {
        if (expandNotificationsPanelMethod == null) {
            try {
                expandNotificationsPanelMethod = manager.getClass().getMethod("expandNotificationsPanel");
            } catch (NoSuchMethodException e) {
                // Custom version for custom vendor ROM: <https://github.com/Genymobile/scrcpy/issues/2551>
                expandNotificationsPanelMethod = manager.getClass().getMethod("expandNotificationsPanel", int.class);
                expandNotificationPanelMethodCustomVersion = true;
            }
        }
        return expandNotificationsPanelMethod;
    }

    private Method getExpandSettingsPanel() throws NoSuchMethodException {
        if (expandSettingsPanelMethod == null) {
            try {
                // Since Android 7: https://android.googlesource.com/platform/frameworks/base.git/+/a9927325eda025504d59bb6594fee8e240d95b01%5E%21/
                expandSettingsPanelMethod = manager.getClass().getMethod("expandSettingsPanel", String.class);
            } catch (NoSuchMethodException e) {
                // old version
                expandSettingsPanelMethod = manager.getClass().getMethod("expandSettingsPanel");
                expandSettingsPanelMethodNewVersion = false;
            }
        }
        return expandSettingsPanelMethod;
    }

    private Method getCollapsePanelsMethod() throws NoSuchMethodException {
        if (collapsePanelsMethod == null) {
            collapsePanelsMethod = manager.getClass().getMethod("collapsePanels");
        }
        return collapsePanelsMethod;
    }

    public void expandNotificationsPanel() {
        try {
            Method method = getExpandNotificationsPanelMethod();
            if (expandNotificationPanelMethodCustomVersion) {
                method.invoke(manager, 0);
            } else {
                method.invoke(manager);
            }
        } catch (ReflectiveOperationException e) {
            Ln.e("Could not invoke method", e);
        }
    }

    public void expandSettingsPanel() {
        try {
            Method method = getExpandSettingsPanel();
            if (expandSettingsPanelMethodNewVersion) {
                // new version
                method.invoke(manager, (Object) null);
            } else {
                // old version
                method.invoke(manager);
            }
        } catch (ReflectiveOperationException e) {
            Ln.e("Could not invoke method", e);
        }
    }

    public void collapsePanels() {
        try {
            Method method = getCollapsePanelsMethod();
            method.invoke(manager);
        } catch (ReflectiveOperationException e) {
            Ln.e("Could not invoke method", e);
        }
    }
}
