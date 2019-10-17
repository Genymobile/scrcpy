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

    private Method getExpandNotificationsPanelMethod() {
        if (expandNotificationsPanelMethod == null) {
            try {
                expandNotificationsPanelMethod = manager.getClass().getMethod("expandNotificationsPanel");
            } catch (NoSuchMethodException e) {
                Ln.e("Could not find method", e);
            }
        }
        return expandNotificationsPanelMethod;
    }

    private Method getCollapsePanelsMethod() {
        if (collapsePanelsMethod == null) {
            try {
                collapsePanelsMethod = manager.getClass().getMethod("collapsePanels");
            } catch (NoSuchMethodException e) {
                Ln.e("Could not find method", e);
            }
        }
        return collapsePanelsMethod;
    }

    public void expandNotificationsPanel() {
        Method method = getExpandNotificationsPanelMethod();
        if (method == null) {
            return;
        }
        try {
            method.invoke(manager);
        } catch (InvocationTargetException | IllegalAccessException e) {
            Ln.e("Could not invoke " + method.getName(), e);
        }
    }

    public void collapsePanels() {
        Method method = getCollapsePanelsMethod();
        if (method == null) {
            return;
        }
        try {
            method.invoke(manager);
        } catch (InvocationTargetException | IllegalAccessException e) {
            Ln.e("Could not invoke " + method.getName(), e);
        }
    }
}
