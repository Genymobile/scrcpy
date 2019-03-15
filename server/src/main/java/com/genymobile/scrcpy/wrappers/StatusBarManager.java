package com.genymobile.scrcpy.wrappers;

import android.annotation.SuppressLint;
import android.os.IInterface;
import android.view.InputEvent;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class StatusBarManager {

    private final IInterface manager;
    private final Method expandNotificationsPanelMethod;
    private final Method collapsePanelsMethod;

    public StatusBarManager(IInterface manager) {
        this.manager = manager;
        try {
            expandNotificationsPanelMethod = manager.getClass().getMethod("expandNotificationsPanel");
            collapsePanelsMethod = manager.getClass().getMethod("collapsePanels");
        } catch (NoSuchMethodException e) {
            throw new AssertionError(e);
        }
    }

    public void expandNotificationsPanel() {
        try {
            expandNotificationsPanelMethod.invoke(manager);
        } catch (InvocationTargetException | IllegalAccessException e) {
            throw new AssertionError(e);
        }
    }

    public void collapsePanels() {
        try {
            collapsePanelsMethod.invoke(manager);
        } catch (InvocationTargetException | IllegalAccessException e) {
            throw new AssertionError(e);
        }
    }
}
