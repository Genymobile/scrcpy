package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.Ln;

import android.os.IInterface;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class StatusBarManager {

    private final IInterface manager;
    private Method expandNotificationsPanelMethod;
    private Method expandSettingsPanelMethod;
    private boolean expandSettingsPanelMethodNewVersion = true;
    private Method collapsePanelsMethod;

    public StatusBarManager(IInterface manager) {
        this.manager = manager;
    }

    private Method getExpandNotificationsPanelMethod() throws NoSuchMethodException {
        if (expandNotificationsPanelMethod == null) {
            expandNotificationsPanelMethod = manager.getClass().getMethod("expandNotificationsPanel");
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
            method.invoke(manager);
        } catch (InvocationTargetException | IllegalAccessException | NoSuchMethodException e) {
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
        } catch (InvocationTargetException | IllegalAccessException | NoSuchMethodException e) {
            Ln.e("Could not invoke method", e);
        }
    }

    public void collapsePanels() {
        try {
            Method method = getCollapsePanelsMethod();
            method.invoke(manager);
        } catch (InvocationTargetException | IllegalAccessException | NoSuchMethodException e) {
            Ln.e("Could not invoke method", e);
        }
    }
}
