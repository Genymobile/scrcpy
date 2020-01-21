package com.genymobile.scrcpy;

/**
 * Union of all supported event types, identified by their {@code type}.
 */
public final class ControlMessage {

    public static final int TYPE_INJECT_KEYCODE = 0;
    public static final int TYPE_INJECT_TEXT = 1;
    public static final int TYPE_INJECT_TOUCH_EVENT = 2;
    public static final int TYPE_INJECT_SCROLL_EVENT = 3;
    public static final int TYPE_BACK_OR_SCREEN_ON = 4;
    public static final int TYPE_EXPAND_NOTIFICATION_PANEL = 5;
    public static final int TYPE_COLLAPSE_NOTIFICATION_PANEL = 6;
    public static final int TYPE_GET_CLIPBOARD = 7;
    public static final int TYPE_SET_CLIPBOARD = 8;
    public static final int TYPE_SET_SCREEN_POWER_MODE = 9;
    public static final int TYPE_ROTATE_DEVICE = 10;

    private int type;
    private String text;
    private int metaState; // KeyEvent.META_*
    private int action; // KeyEvent.ACTION_* or MotionEvent.ACTION_* or POWER_MODE_*
    private int keycode; // KeyEvent.KEYCODE_*
    private int buttons; // MotionEvent.BUTTON_*
    private long pointerId;
    private float pressure;
    private Position position;
    private int hScroll;
    private int vScroll;

    private ControlMessage() {
    }

    public static ControlMessage createInjectKeycode(int action, int keycode, int metaState) {
        ControlMessage msg = new ControlMessage();
        msg.type = TYPE_INJECT_KEYCODE;
        msg.action = action;
        msg.keycode = keycode;
        msg.metaState = metaState;
        return msg;
    }

    public static ControlMessage createInjectText(String text) {
        ControlMessage msg = new ControlMessage();
        msg.type = TYPE_INJECT_TEXT;
        msg.text = text;
        return msg;
    }

    public static ControlMessage createInjectTouchEvent(int action, long pointerId, Position position, float pressure, int buttons) {
        ControlMessage msg = new ControlMessage();
        msg.type = TYPE_INJECT_TOUCH_EVENT;
        msg.action = action;
        msg.pointerId = pointerId;
        msg.pressure = pressure;
        msg.position = position;
        msg.buttons = buttons;
        return msg;
    }

    public static ControlMessage createInjectScrollEvent(Position position, int hScroll, int vScroll) {
        ControlMessage msg = new ControlMessage();
        msg.type = TYPE_INJECT_SCROLL_EVENT;
        msg.position = position;
        msg.hScroll = hScroll;
        msg.vScroll = vScroll;
        return msg;
    }

    public static ControlMessage createSetClipboard(String text) {
        ControlMessage msg = new ControlMessage();
        msg.type = TYPE_SET_CLIPBOARD;
        msg.text = text;
        return msg;
    }

    /**
     * @param mode one of the {@code Device.SCREEN_POWER_MODE_*} constants
     */
    public static ControlMessage createSetScreenPowerMode(int mode) {
        ControlMessage msg = new ControlMessage();
        msg.type = TYPE_SET_SCREEN_POWER_MODE;
        msg.action = mode;
        return msg;
    }

    public static ControlMessage createEmpty(int type) {
        ControlMessage msg = new ControlMessage();
        msg.type = type;
        return msg;
    }

    public int getType() {
        return type;
    }

    public String getText() {
        return text;
    }

    public int getMetaState() {
        return metaState;
    }

    public int getAction() {
        return action;
    }

    public int getKeycode() {
        return keycode;
    }

    public int getButtons() {
        return buttons;
    }

    public long getPointerId() {
        return pointerId;
    }

    public float getPressure() {
        return pressure;
    }

    public Position getPosition() {
        return position;
    }

    public int getHScroll() {
        return hScroll;
    }

    public int getVScroll() {
        return vScroll;
    }
}
