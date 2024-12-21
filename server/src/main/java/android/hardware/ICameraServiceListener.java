package android.hardware;

import android.os.IBinder;

public interface ICameraServiceListener {
    int STATUS_NOT_PRESENT = 0;
    int STATUS_ENUMERATING = 2;

    class Default implements ICameraServiceListener {
        public IBinder asBinder() {
            return null;
        }
    }
}
