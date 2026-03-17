package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.util.Ln;

import android.content.res.Configuration;
import android.os.Parcel;
import android.os.RemoteException;
import android.view.IDisplayWindowListener;

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

    // Android 16 added these methods to IDisplayWindowListener.
    // They are not in the SDK stubs, so no @Override, but they must exist
    // to prevent AbstractMethodError when the binder dispatches to them.
    public void onDisplayAnimationsDisabledChanged(int displayId, boolean animationsDisabled) {
        // empty default implementation
    }

    public void onDisplayAddSystemDecorations(int displayId) {
        // empty default implementation
    }

    @Override
    public boolean onTransact(int code, Parcel data, Parcel reply, int flags) throws RemoteException {
        try {
            return super.onTransact(code, data, reply, flags);
        } catch (AbstractMethodError e) {
            Ln.v("Ignoring AbstractMethodError: " + e.getMessage());
            // Ignore unknown methods, write default response to reply parcel
            reply.writeNoException();
            return true;
        }
    }
}
