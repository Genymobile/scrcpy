package com.genymobile.scrcpy;

import com.sun.jna.Library;
import com.sun.jna.Native;
import com.sun.jna.Platform;
import com.sun.jna.Pointer;
import com.sun.jna.Structure;

import java.util.Arrays;
import java.util.List;

public abstract class UinputDevice {
    public static final int DEVICE_ADDED = 0;
    public static final int DEVICE_REMOVED = 1;

    private static final int UINPUT_MAX_NAME_SIZE = 80;

    @SuppressWarnings("checkstyle:VisibilityModifier")
    public static class InputId extends Structure {
        public short bustype = 0;
        public short vendor = 0;
        public short product = 0;
        public short version = 0;

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("bustype", "vendor", "product", "version");
        }
    }

    @SuppressWarnings("checkstyle:VisibilityModifier")
    public static class UinputSetup extends Structure {
        public InputId id;
        public byte[] name = new byte[UINPUT_MAX_NAME_SIZE];
        public int ffEffectsMax = 0;

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("id", "name", "ffEffectsMax");
        }
    }

    @SuppressWarnings("checkstyle:VisibilityModifier")
    public static class InputAbsinfo extends Structure {
        public int value = 0;
        public int minimum = 0;
        public int maximum = 0;
        public int fuzz = 0;
        public int flat = 0;
        public int resolution = 0;

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("value", "minimum", "maximum", "fuzz", "flat", "resolution");
        }
    };

    @SuppressWarnings("checkstyle:VisibilityModifier")
    public static class UinputAbsSetup extends Structure {
        public short code;
        public InputAbsinfo absinfo;

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("code", "absinfo");
        }
    };

    @SuppressWarnings("checkstyle:VisibilityModifier")
    public static class InputEvent32 extends Structure {
        public long time = 0;
        public short type = 0;
        public short code = 0;
        public int value = 0;

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("time", "type", "code", "value");
        }
    }

    @SuppressWarnings("checkstyle:VisibilityModifier")
    public static class InputEvent64 extends Structure {
        public long sec = 0;
        public long usec = 0;
        public short type = 0;
        public short code = 0;
        public int value = 0;

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("sec", "usec", "type", "code", "value");
        }
    }

    private static final int IOC_NONE = 0;
    private static final int IOC_WRITE = 1;

    private static final int IOC_DIRSHIFT = 30;
    private static final int IOC_TYPESHIFT = 8;
    private static final int IOC_NRSHIFT = 0;
    private static final int IOC_SIZESHIFT = 16;

    private static int ioc(int dir, int type, int nr, int size) {
        return (dir << IOC_DIRSHIFT)
             | (type << IOC_TYPESHIFT)
             | (nr << IOC_NRSHIFT)
             | (size << IOC_SIZESHIFT);
    }

    private static int io(int type, int nr, int size) {
        return ioc(IOC_NONE, type, nr, size);
    }

    private static int iow(int type, int nr, int size) {
        return ioc(IOC_WRITE, type, nr, size);
    }

    private static final int O_WRONLY = 01;
    private static final int O_NONBLOCK = 04000;

    private static final int BUS_USB = 0x03;

    private static final int UINPUT_IOCTL_BASE = 'U';

    private static final int UI_SET_EVBIT = iow(UINPUT_IOCTL_BASE, 100, 4);
    private static final int UI_SET_KEYBIT = iow(UINPUT_IOCTL_BASE, 101, 4);
    private static final int UI_SET_ABSBIT = iow(UINPUT_IOCTL_BASE, 103, 4);
    private static final int UI_ABS_SETUP = iow(UINPUT_IOCTL_BASE, 4, new UinputAbsSetup().size());

    private static final int UI_DEV_SETUP = iow(UINPUT_IOCTL_BASE, 3, new UinputSetup().size());
    private static final int UI_DEV_CREATE = io(UINPUT_IOCTL_BASE, 1, 0);
    private static final int UI_DEV_DESTROY = io(UINPUT_IOCTL_BASE, 2, 0);

    private static final short EV_SYN = 0x00;
    private static final short EV_KEY = 0x01;
    private static final short EV_ABS = 0x03;

    private static final short SYN_REPORT = 0x00;

    protected static final short BTN_A = 0x130;
    protected static final short BTN_B = 0x131;
    protected static final short BTN_X = 0x133;
    protected static final short BTN_Y = 0x134;
    protected static final short BTN_TL = 0x136;
    protected static final short BTN_TR = 0x137;
    protected static final short BTN_SELECT = 0x13a;
    protected static final short BTN_START = 0x13b;
    protected static final short BTN_MODE = 0x13c;
    protected static final short BTN_THUMBL = 0x13d;
    protected static final short BTN_THUMBR = 0x13e;

    protected static final short ABS_X = 0x00;
    protected static final short ABS_Y = 0x01;
    protected static final short ABS_Z = 0x02;
    protected static final short ABS_RX = 0x03;
    protected static final short ABS_RY = 0x04;
    protected static final short ABS_RZ = 0x05;
    protected static final short ABS_HAT0X = 0x10;
    protected static final short ABS_HAT0Y = 0x11;

    private int fd = -1;

    public interface LibC extends Library {
        LibC FN = (LibC) Native.load("c", LibC.class);

        int open(String pathname, int flags);
        int ioctl(int fd, long request, Object... args);
        long write(int fd, Pointer buf, long count);
        int close(int fd);
    }

    public static void loadNativeLibraries() {
        UinputDevice.LibC.FN.write(1, null, 0);
    }

    protected void setup() {
        fd = LibC.FN.open("/dev/uinput", O_WRONLY | O_NONBLOCK);
        if (fd == -1) {
            throw new RuntimeException("Couldn't open uinput device.");
        }

        if (hasKeys())
        {
            LibC.FN.ioctl(fd, UI_SET_EVBIT, EV_KEY);
            setupKeys();
        }

        if (hasAbs())
        {
            LibC.FN.ioctl(fd, UI_SET_EVBIT, EV_ABS);
            setupAbs();
        }

        UinputSetup usetup = new UinputSetup();
        usetup.id.bustype = BUS_USB;
        usetup.id.vendor = getVendor();
        usetup.id.product = getProduct();
        byte[] name = getName().getBytes();
        System.arraycopy(name, 0, usetup.name, 0, name.length);

        if (LibC.FN.ioctl(fd, UI_DEV_SETUP, usetup) == -1) {
            close();
            throw new RuntimeException("Couldn't setup uinput device.");
        }

        if (LibC.FN.ioctl(fd, UI_DEV_CREATE) == -1) {
            close();
            throw new RuntimeException("Couldn't create uinput device.");
        }
    }

    public void close() {
        if (fd != -1) {
            LibC.FN.ioctl(fd, UI_DEV_DESTROY);
            LibC.FN.close(fd);
            fd = -1;
        }
    }

    protected abstract void setupKeys();
    protected abstract boolean hasKeys();
    protected abstract void setupAbs();
    protected abstract boolean hasAbs();
    protected abstract short getVendor();
    protected abstract short getProduct();
    protected abstract String getName();

    protected void addKey(int key) {
        if (LibC.FN.ioctl(fd, UI_SET_KEYBIT, key) == -1) {
            Ln.e("Could not add key event.");
        }
    }

    protected void addAbs(short code, int minimum, int maximum, int fuzz, int flat) {
        if (LibC.FN.ioctl(fd, UI_SET_ABSBIT, code) == -1) {
            Ln.e("Could not add absolute event.");
        }

        UinputAbsSetup absSetup = new UinputAbsSetup();

        absSetup.code = code;
        absSetup.absinfo.minimum = minimum;
        absSetup.absinfo.maximum = maximum;
        absSetup.absinfo.fuzz = fuzz;
        absSetup.absinfo.flat = flat;

        if (LibC.FN.ioctl(fd, UI_ABS_SETUP, absSetup) == -1) {
            Ln.e("Could not set absolute event info.");
        }
    }

    private static void emit32(int fd, short type, short code, int val) {
        InputEvent32 ie = new InputEvent32();

        ie.type = type;
        ie.code = code;
        ie.value = val;

        ie.write();

        LibC.FN.write(fd, ie.getPointer(), ie.size());
    }

    private static void emit64(int fd, short type, short code, int val) {
        InputEvent64 ie = new InputEvent64();

        ie.type = type;
        ie.code = code;
        ie.value = val;

        ie.write();

        LibC.FN.write(fd, ie.getPointer(), ie.size());
    }

    private static void emit(int fd, short type, short code, int val) {
        if (Platform.is64Bit()) {
            emit64(fd, type, code, val);
        } else {
            emit32(fd, type, code, val);
        }
    }

    protected void emitAbs(short abs, int value) {
        emit(fd, EV_ABS, abs, value);
    }

    protected void emitKey(short key, int state) {
        emit(fd, EV_KEY, key, state);
    }

    protected void emitReport() {
        emit(fd, EV_SYN, SYN_REPORT, 0);
    }
}
