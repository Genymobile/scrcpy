package androidx.system;

final class OsConstantsLibcore implements OsConstants {
    @Override
    public String errnoName(int errno) {
        return libcore.io.OsConstants.errnoName(errno);
    }
}
