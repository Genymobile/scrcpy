package com.genymobile.scrcpy.wrappers;

import android.content.res.Configuration;
import android.graphics.Rect;
import android.view.IDisplayWindowListener;

import java.util.List;

public class DisplayWindowListener extends IDisplayWindowListener.Stub {
    @Override
    public void onDisplayAdded(int displayId) {
        // empty default implementation
    }

    @Override
    public void onDisplayConfigurationChanged(int displayId, Configuration newConfig) {
        // empty default implementation
    }

    @Override
    public void onDisplayRemoved(int displayId) {
        // empty default implementation
    }

    @Override
    public void onFixedRotationStarted(int displayId, int newRotation) {
        // empty default implementation
    }

    @Override
    public void onFixedRotationFinished(int displayId) {
        // empty default implementation
    }

    @Override
    public void onKeepClearAreasChanged(int displayId, List<Rect> restricted, List<Rect> unrestricted) {
        // empty default implementation
    }
}
