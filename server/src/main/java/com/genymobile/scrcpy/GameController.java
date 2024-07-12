package com.genymobile.scrcpy;

public final class GameController extends UinputDevice {
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

    public GameController() {
        setup();
    }

    protected void setupKeys() {
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
    }

    protected boolean hasKeys() {
        return true;
    }

    protected void setupAbs() {
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
    }

    protected boolean hasAbs() {
        return true;
    }

    protected short getVendor() {
        return 0x045e;
    }

    protected short getProduct() {
        return 0x028e;
    }

    protected String getName() {
        return "scrcpy Xbox Controller";
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
        emitAbs(translateAxis(axis), value);
        emitReport();
    }

    public void setButton(int button, int state) {
        // DPad buttons are usually reported as axes

        switch (button) {
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
                emitAbs(XBOX_ABS_DPADY, state != 0 ? -1 : 0);
                break;

            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                emitAbs(XBOX_ABS_DPADY, state != 0 ? 1 : 0);
                break;

            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                emitAbs(XBOX_ABS_DPADX, state != 0 ? -1 : 0);
                break;

            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                emitAbs(XBOX_ABS_DPADX, state != 0 ? 1 : 0);
                break;

            default:
                emitKey(translateButton(button), state);
        }
        emitReport();
    }
}
