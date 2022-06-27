package androidx.system;

import androidx.annotation.RequiresApi;

@RequiresApi(21)
final class OsConstantsApi21 implements OsConstants {
    @Override
    public String errnoName(int errno) {
        return android.system.OsConstants.errnoName(errno);
    }
}
