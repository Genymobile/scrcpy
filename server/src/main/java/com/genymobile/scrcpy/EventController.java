package com.genymobile.scrcpy;

import com.genymobile.scrcpy.wrappers.InputManager;

import android.graphics.Point;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.InputEvent;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import android.view.MotionEvent;

import java.io.IOException;


public class EventController {

    private final Device device;
    private final DesktopConnection connection;

    private final KeyCharacterMap charMap = KeyCharacterMap.load(KeyCharacterMap.VIRTUAL_KEYBOARD);

    private long lastMouseDown;
    private final MotionEvent.PointerProperties[] pointerProperties = {new MotionEvent.PointerProperties()};
    private final MotionEvent.PointerCoords[] pointerCoords = {new MotionEvent.PointerCoords()};

    public EventController(Device device, DesktopConnection connection) {
        this.device = device;
        this.connection = connection;
        initPointer();
    }

    private void initPointer() {
        MotionEvent.PointerProperties props = pointerProperties[0];
        props.id = 0;
        props.toolType = MotionEvent.TOOL_TYPE_FINGER;

        MotionEvent.PointerCoords coords = pointerCoords[0];
        coords.orientation = 0;
        coords.pressure = 1;
        coords.size = 1;
    }

    private void setPointerCoords(Point point) {
        MotionEvent.PointerCoords coords = pointerCoords[0];
        coords.x = point.x;
        coords.y = point.y;
    }

    private void setScroll(int hScroll, int vScroll) {
        MotionEvent.PointerCoords coords = pointerCoords[0];
        coords.setAxisValue(MotionEvent.AXIS_HSCROLL, hScroll);
        coords.setAxisValue(MotionEvent.AXIS_VSCROLL, vScroll);
    }

    public void control() throws IOException {
        // on start, turn screen on
        turnScreenOn();

        while (true) {
            handleEvent();
        }
    }

    private void handleEvent() throws IOException {
        ControlEvent controlEvent = connection.receiveControlEvent();
        switch (controlEvent.getType()) {
            case ControlEvent.TYPE_KEYCODE:
                injectKeycode(controlEvent.getAction(), controlEvent.getKeycode(), controlEvent.getMetaState());
                break;
            case ControlEvent.TYPE_TEXT:
                injectText(controlEvent.getText());
                break;
            case ControlEvent.TYPE_MOUSE:
                injectMouse(controlEvent.getAction(), controlEvent.getButtons(), controlEvent.getPosition());
                break;
            case ControlEvent.TYPE_SCROLL:
                injectScroll(controlEvent.getPosition(), controlEvent.getHScroll(), controlEvent.getVScroll());
                break;
            case ControlEvent.TYPE_COMMAND:
                executeCommand(controlEvent.getAction());
                break;
            default:
                // do nothing
        }
    }

    private boolean injectKeycode(int action, int keycode, int metaState) {
        return injectKeyEvent(action, keycode, 0, metaState);
    }

    private boolean injectChar(char c) {
        String decomposed = KeyComposition.decompose(c);
        char[] chars = decomposed != null ? decomposed.toCharArray() : new char[] {c};
        KeyEvent[] events = charMap.getEvents(chars);
        if (events == null) {
            return false;
        }
        for (KeyEvent event : events) {
            if (!injectEvent(event)) {
                return false;
            }
        }
        return true;
    }

    private boolean injectText(String text) {
        for (char c : text.toCharArray()) {
            if (!injectChar(c)) {
                return false;
            }
        }
        return true;
    }

    private boolean injectMouse(int action, int buttons, Position position) {
        long now = SystemClock.uptimeMillis();
        if (action == MotionEvent.ACTION_DOWN) {
            lastMouseDown = now;
        }
        Point point = device.getPhysicalPoint(position);
        if (point == null) {
            // ignore event
            return false;
        }
        setPointerCoords(point);
        MotionEvent event = MotionEvent.obtain(lastMouseDown, now, action, 1, pointerProperties, pointerCoords, 0, buttons, 1f, 1f, 0, 0,
                InputDevice.SOURCE_TOUCHSCREEN, 0);
        return injectEvent(event);
    }

    private boolean injectScroll(Position position, int hScroll, int vScroll) {
        long now = SystemClock.uptimeMillis();
        Point point = device.getPhysicalPoint(position);
        if (point == null) {
            // ignore event
            return false;
        }
        setPointerCoords(point);
        setScroll(hScroll, vScroll);
        MotionEvent event = MotionEvent.obtain(lastMouseDown, now, MotionEvent.ACTION_SCROLL, 1, pointerProperties, pointerCoords, 0, 0, 1f, 1f, 0,
                0, InputDevice.SOURCE_MOUSE, 0);
        return injectEvent(event);
    }

    private boolean injectKeyEvent(int action, int keyCode, int repeat, int metaState) {
        long now = SystemClock.uptimeMillis();
        KeyEvent event = new KeyEvent(now, now, action, keyCode, repeat, metaState, KeyCharacterMap.VIRTUAL_KEYBOARD, 0, 0,
                InputDevice.SOURCE_KEYBOARD);
        return injectEvent(event);
    }

    private boolean injectKeycode(int keyCode) {
        return injectKeyEvent(KeyEvent.ACTION_DOWN, keyCode, 0, 0)
                && injectKeyEvent(KeyEvent.ACTION_UP, keyCode, 0, 0);
    }

    private boolean injectEvent(InputEvent event) {
        return device.injectInputEvent(event, InputManager.INJECT_INPUT_EVENT_MODE_ASYNC);
    }

    private boolean turnScreenOn() {
        return device.isScreenOn() || injectKeycode(KeyEvent.KEYCODE_POWER);
    }

    private boolean pressBackOrTurnScreenOn() {
        int keycode = device.isScreenOn() ? KeyEvent.KEYCODE_BACK : KeyEvent.KEYCODE_POWER;
        return injectKeycode(keycode);
    }

    private boolean executeCommand(int action) {
        switch (action) {
            case ControlEvent.COMMAND_BACK_OR_SCREEN_ON:
                return pressBackOrTurnScreenOn();
            default:
                Ln.w("Unsupported command: " + action);
        }
        return false;
    }
}
