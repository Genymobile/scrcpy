package com.genymobile.scrcpy;

import com.sun.jna.Library;
import com.sun.jna.Native;
import com.sun.jna.Platform;
import com.sun.jna.Pointer;
import com.sun.jna.Structure;

import java.util.Arrays;
import java.util.List;

public final class GameController {
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
    private static final int UI_DEV_CREATE = iow(UINPUT_IOCTL_BASE, 1, 0);
    private static final int UI_DEV_DESTROY = iow(UINPUT_IOCTL_BASE, 2, 0);

    private static final short EV_SYN = 0x00;
    private static final short EV_KEY = 0x01;
    private static final short EV_ABS = 0x03;

    private static final short SYN_REPORT = 0x00;

    private static final short BTN_A = 0x130;
    private static final short BTN_B = 0x131;
    private static final short BTN_X = 0x133;
    private static final short BTN_Y = 0x134;
    private static final short BTN_TL = 0x136;
    private static final short BTN_TR = 0x137;
    private static final short BTN_SELECT = 0x13a;
    private static final short BTN_START = 0x13b;
    private static final short BTN_MODE = 0x13c;
    private static final short BTN_THUMBL = 0x13d;
    private static final short BTN_THUMBR = 0x13e;

    private static final short ABS_X = 0x00;
    private static final short ABS_Y = 0x01;
    private static final short ABS_Z = 0x02;
    private static final short ABS_RX = 0x03;
    private static final short ABS_RY = 0x04;
    private static final short ABS_RZ = 0x05;
    private static final short ABS_HAT0X = 0x10;
    private static final short ABS_HAT0Y = 0x11;

    private static final short XBOX_BTN_A = BTN_A;
    private static final short XBOX_BTN_B = BTN_B;
    private static final short XBOX_BTN_X = BTN_X;
    private static final short XBOX_BTN_Y = BTN_Y;
    private static final short XBOX_BTN_BACK = BTN_SELECT;
    private static final short XBOX_BTN_START = BTN_START;
    private static final short XBOX_BTN_LB = BTN_TL;
    private static final short XBOX_BTN_RB = BTN_TR;
    private static final short XBOX_BTN_GUIDE = BTN_MODE;
    private static final short XBOX_BTN_LS = BTN_THUMBL;
    private static final short XBOX_BTN_RS = BTN_THUMBR;

    private static final short XBOX_ABS_LSX = ABS_X;
    private static final short XBOX_ABS_LSY = ABS_Y;
    private static final short XBOX_ABS_RSX = ABS_RX;
    private static final short XBOX_ABS_RSY = ABS_RY;
    private static final short XBOX_ABS_DPADX = ABS_HAT0X;
    private static final short XBOX_ABS_DPADY = ABS_HAT0Y;
    private static final short XBOX_ABS_LT = ABS_Z;
    private static final short XBOX_ABS_RT = ABS_RZ;

    public static final int SDL_CONTROLLER_AXIS_LEFTX = 0;
    public static final int SDL_CONTROLLER_AXIS_LEFTY = 1;
    public static final int SDL_CONTROLLER_AXIS_RIGHTX = 2;
    public static final int SDL_CONTROLLER_AXIS_RIGHTY = 3;
    public static final int SDL_CONTROLLER_AXIS_TRIGGERLEFT = 4;
    public static final int SDL_CONTROLLER_AXIS_TRIGGERRIGHT = 5;

    public static final int SDL_CONTROLLER_BUTTON_A = 0;
    public static final int SDL_CONTROLLER_BUTTON_B = 1;
    public static final int SDL_CONTROLLER_BUTTON_X = 2;
    public static final int SDL_CONTROLLER_BUTTON_Y = 3;
    public static final int SDL_CONTROLLER_BUTTON_BACK = 4;
    public static final int SDL_CONTROLLER_BUTTON_GUIDE = 5;
    public static final int SDL_CONTROLLER_BUTTON_START = 6;
    public static final int SDL_CONTROLLER_BUTTON_LEFTSTICK = 7;
    public static final int SDL_CONTROLLER_BUTTON_RIGHTSTICK = 8;
    public static final int SDL_CONTROLLER_BUTTON_LEFTSHOULDER = 9;
    public static final int SDL_CONTROLLER_BUTTON_RIGHTSHOULDER = 10;
    public static final int SDL_CONTROLLER_BUTTON_DPAD_UP = 11;
    public static final int SDL_CONTROLLER_BUTTON_DPAD_DOWN = 12;
    public static final int SDL_CONTROLLER_BUTTON_DPAD_LEFT = 13;
    public static final int SDL_CONTROLLER_BUTTON_DPAD_RIGHT = 14;

    private int fd;

    public interface LibC extends Library {
        LibC FN = (LibC) Native.load("c", LibC.class);

        int open(String pathname, int flags);
        int ioctl(int fd, long request, Object... args);
        long write(int fd, Pointer buf, long count);
        int close(int fd);
    }

    public static void loadNativeLibraries() {
        GameController.LibC.FN.write(1, null, 0);
    }

    public GameController() {
        fd = LibC.FN.open("/dev/uinput", O_WRONLY | O_NONBLOCK);
        if (fd == -1) {
            throw new RuntimeException("Couldn't open uinput device.");
        }

        LibC.FN.ioctl(fd, UI_SET_EVBIT, EV_KEY);
        addKey(XBOX_BTN_A);
        addKey(XBOX_BTN_B);
        addKey(XBOX_BTN_X);
        addKey(XBOX_BTN_Y);
        addKey(XBOX_BTN_BACK);
        addKey(XBOX_BTN_START);
        addKey(XBOX_BTN_LB);
        addKey(XBOX_BTN_RB);
        addKey(XBOX_BTN_GUIDE);
        addKey(XBOX_BTN_LS);
        addKey(XBOX_BTN_RS);

        LibC.FN.ioctl(fd, UI_SET_EVBIT, EV_ABS);
        addAbs(XBOX_ABS_LSX, -32768, 32767, 16, 128);
        addAbs(XBOX_ABS_LSY, -32768, 32767, 16, 128);
        addAbs(XBOX_ABS_RSX, -32768, 32767, 16, 128);
        addAbs(XBOX_ABS_RSY, -32768, 32767, 16, 128);
        addAbs(XBOX_ABS_DPADX, -1, 1, 0, 0);
        addAbs(XBOX_ABS_DPADY, -1, 1, 0, 0);
        // These values deviate from the real Xbox 360 controller,
        // but allow higher precision (eg. Xbox One controller)
        addAbs(XBOX_ABS_LT, 0, 32767, 0, 0);
        addAbs(XBOX_ABS_RT, 0, 32767, 0, 0);

        UinputSetup usetup = new UinputSetup();
        usetup.id.bustype = BUS_USB;
        usetup.id.vendor = 0x045e;
        usetup.id.product = 0x028e;
        byte[] name = "Microsoft X-Box 360 pad".getBytes();
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

    private void addKey(int key) {
        if (LibC.FN.ioctl(fd, UI_SET_KEYBIT, key) == -1) {
            Ln.e("Could not add key event.");
        }
    }

    private void addAbs(short code, int minimum, int maximum, int fuzz, int flat) {
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

    private static short translateAxis(int axis) {
        switch (axis) {
            case SDL_CONTROLLER_AXIS_LEFTX:
                return XBOX_ABS_LSX;

            case SDL_CONTROLLER_AXIS_LEFTY:
                return XBOX_ABS_LSY;

            case SDL_CONTROLLER_AXIS_RIGHTX:
                return XBOX_ABS_RSX;

            case SDL_CONTROLLER_AXIS_RIGHTY:
                return XBOX_ABS_RSY;

            case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
                return XBOX_ABS_LT;

            case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
                return XBOX_ABS_RT;

            default:
                return 0;
        }
    }

    private static short translateButton(int button) {
        switch (button) {
            case SDL_CONTROLLER_BUTTON_A:
                return XBOX_BTN_A;

            case SDL_CONTROLLER_BUTTON_B:
                return XBOX_BTN_B;

            case SDL_CONTROLLER_BUTTON_X:
                return XBOX_BTN_X;

            case SDL_CONTROLLER_BUTTON_Y:
                return XBOX_BTN_Y;

            case SDL_CONTROLLER_BUTTON_BACK:
                return XBOX_BTN_BACK;

            case SDL_CONTROLLER_BUTTON_GUIDE:
                return XBOX_BTN_GUIDE;

            case SDL_CONTROLLER_BUTTON_START:
                return XBOX_BTN_START;

            case SDL_CONTROLLER_BUTTON_LEFTSTICK:
                return XBOX_BTN_LS;

            case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
                return XBOX_BTN_RS;

            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                return XBOX_BTN_LB;

            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                return XBOX_BTN_RB;

            default:
                return 0;
        }
    }

    public void setAxis(int axis, int value) {
        emit(fd, EV_ABS, translateAxis(axis), value);
        emit(fd, EV_SYN, SYN_REPORT, 0);
    }

    public void setButton(int button, int state) {
        // DPad buttons are usually reported as axes

        switch (button) {
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
                emit(fd, EV_ABS, XBOX_ABS_DPADY, state != 0 ? -1 : 0);
                break;

            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                emit(fd, EV_ABS, XBOX_ABS_DPADY, state != 0 ? 1 : 0);
                break;

            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                emit(fd, EV_ABS, XBOX_ABS_DPADX, state != 0 ? -1 : 0);
                break;

            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                emit(fd, EV_ABS, XBOX_ABS_DPADX, state != 0 ? 1 : 0);
                break;

            default:
                emit(fd, EV_KEY, translateButton(button), state);
        }
        emit(fd, EV_SYN, SYN_REPORT, 0);
    }
}
