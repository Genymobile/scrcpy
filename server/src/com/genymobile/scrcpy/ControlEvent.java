package com.genymobile.scrcpy;

/**
 * Union of all supported event types, identified by their {@code type}.
 */
public class ControlEvent {

    public static final int TYPE_KEYCODE = 0;
    public static final int TYPE_TEXT = 1;
    public static final int TYPE_MOUSE = 2;
    public static final int TYPE_SCROLL = 3;

    private int type;
    private String text;
    private int metaState; // KeyEvent.META_*
    private int action; // KeyEvent.ACTION_* or MotionEvent.ACTION_*
    private int keycode; // KeyEvent.KEYCODE_*
    private int buttons; // MotionEvent.BUTTON_*
    private int x;
    private int y;
    private int hScroll;
    private int vScroll;

    private ControlEvent() {
    }

    public static ControlEvent createKeycodeControlEvent(int action, int keycode, int metaState) {
        ControlEvent event = new ControlEvent();
        event.type = TYPE_KEYCODE;
        event.action = action;
        event.keycode = keycode;
        event.metaState = metaState;
        return event;
    }

    public static ControlEvent createTextControlEvent(String text) {
        ControlEvent event = new ControlEvent();
        event.type = TYPE_TEXT;
        event.text = text;
        return event;
    }

    public static ControlEvent createMotionControlEvent(int action, int buttons, int x, int y) {
        ControlEvent event = new ControlEvent();
        event.type = TYPE_MOUSE;
        event.action = action;
        event.buttons = buttons;
        event.x = x;
        event.y = y;
        return event;
    }

    public static ControlEvent createScrollControlEvent(int x, int y, int hScroll, int vScroll) {
        ControlEvent event = new ControlEvent();
        event.type = TYPE_SCROLL;
        event.x = x;
        event.y = y;
        event.hScroll = hScroll;
        event.vScroll = vScroll;
        return event;
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

    public int getX() {
        return x;
    }

    public int getY() {
        return y;
    }

    public int getHScroll() {
        return hScroll;
    }

    public int getVScroll() {
        return vScroll;
    }
}
