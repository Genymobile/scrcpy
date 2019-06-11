package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.Ln;

import android.os.IInterface;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class StatusBarManager {

    private final IInterface manager;
    private Method expandNotificationsPanelMethod;
    private Method collapsePanelsMethod;

    public StatusBarManager(IInterface manager) {
        this.manager = manager;
    }

    public void expandNotificationsPanel() {
        if (expandNotificationsPanelMethod == null) {
            try {
                expandNotificationsPanelMethod = manager.getClass().getMethod("expandNotificationsPanel");
            } catch (NoSuchMethodException e) {
                Ln.e("ServiceBarManager.expandNotificationsPanel() is not available on this device");
                return;
            }
        }
        try {
            expandNotificationsPanelMethod.invoke(manager);
        } catch (InvocationTargetException | IllegalAccessException e) {
            Ln.e("Cannot invoke ServiceBarManager.expandNotificationsPanel()", e);
        }
    }

    public void collapsePanels() {
        if (collapsePanelsMethod == null) {
            try {
                collapsePanelsMethod = manager.getClass().getMethod("collapsePanels");
            } catch (NoSuchMethodException e) {
                Ln.e("ServiceBarManager.collapsePanels() is not available on this device");
                return;
            }
        }
        try {
            collapsePanelsMethod.invoke(manager);
        } catch (InvocationTargetException | IllegalAccessException e) {
            Ln.e("Cannot invoke ServiceBarManager.collapsePanels()", e);
        }
    }
}
