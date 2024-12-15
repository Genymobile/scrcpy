package com.genymobile.scrcpy.wrappers;

import android.annotation.SuppressLint;
import android.hardware.ICameraServiceListener;
import android.os.Binder;
import android.os.IBinder;
import android.os.IInterface;
import android.os.Parcel;
import android.util.Log;

import com.genymobile.scrcpy.util.Ln;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

public class CameraService {
    private final IInterface service;

    private Method addListenerMethod;
    private Method removeListenerMethod;

    public CameraService(IInterface service) {
        this.service = service;
    }

    private Method getAddListenerMethod() throws NoSuchMethodException {
        if (addListenerMethod == null) {
            addListenerMethod = service.getClass().getDeclaredMethod("addListener", ICameraServiceListener.class);
        }
        return addListenerMethod;
    }

    public CameraStatus[] addListener(ICameraServiceListener listener) {
        try {
            Object[] rawStatuses = (Object[]) getAddListenerMethod().invoke(service, listener);
            if (rawStatuses == null) {
                return null;
            }
            CameraStatus[] cameraStatuses = new CameraStatus[rawStatuses.length];
            for (int i = 0; i < rawStatuses.length; i++) {
                cameraStatuses[i] = new CameraStatus(rawStatuses[i]);
            }
            return cameraStatuses;
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    private Method getRemoveListenerMethod() throws NoSuchMethodException {
        if (removeListenerMethod == null) {
            removeListenerMethod = service.getClass().getDeclaredMethod("removeListener", ICameraServiceListener.class);
        }
        return removeListenerMethod;
    }

    public void removeListener(ICameraServiceListener listener) {
        try {
            getRemoveListenerMethod().invoke(service, listener);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    // Simulate `CameraManager.getCameraIdList()`
    // <https://android.googlesource.com/platform/frameworks/base/+/af274a1b252752b385bceb14f1f88e361e2c91e5/core/java/android/hardware/camera2/CameraManager.java#2351>
    public String[] getCameraIdList() {
        Binder binder = new Binder() {
            @Override
            protected boolean onTransact(int code, Parcel data, Parcel reply, int flags) {
                return true;
            }
        };
        ICameraServiceListener listener = new ICameraServiceListener.Default() {
            // To immune from future changes to `ICameraServiceListener`,
            // we use an empty `Binder` that ignores all transactions.
            @Override
            public IBinder asBinder() {
                return binder;
            }
        };

        CameraStatus[] cameraStatuses = addListener(listener);
        removeListener(listener);

        if (cameraStatuses == null) {
            return null;
        }

        List<String> cameraIds = new ArrayList<>();
        for (CameraStatus cameraStatus : cameraStatuses) {
            if (cameraStatus.status != ICameraServiceListener.STATUS_NOT_PRESENT && cameraStatus.status != ICameraServiceListener.STATUS_ENUMERATING) {
                cameraIds.add(cameraStatus.cameraId);
            }
        }

        return cameraIds.toArray(new String[0]);
    }

    @SuppressLint({"SoonBlockedPrivateApi", "PrivateApi"})
    public static class CameraStatus {
        public int status;
        public String cameraId;

        public CameraStatus(Object raw) throws NoSuchFieldException, IllegalAccessException {
            status = raw.getClass().getDeclaredField("status").getInt(raw);
            cameraId = (String) raw.getClass().getDeclaredField("cameraId").get(raw);
        }
    }
}
