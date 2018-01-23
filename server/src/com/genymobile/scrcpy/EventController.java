package com.genymobile.scrcpy;

import android.os.SystemClock;
import android.view.InputDevice;
import android.view.InputEvent;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import android.view.MotionEvent;

import com.genymobile.scrcpy.wrappers.InputManager;

import java.io.IOException;

public class EventController {

    private final InputManager inputManager;
    private final DesktopConnection connection;

    private final KeyCharacterMap charMap = KeyCharacterMap.load(KeyCharacterMap.VIRTUAL_KEYBOARD);

    private long lastMouseDown;
    private final MotionEvent.PointerProperties[] pointerProperties = { new MotionEvent.PointerProperties() };
    private final MotionEvent.PointerCoords[] pointerCoords = { new MotionEvent.PointerCoords() };

    public EventController(DesktopConnection connection) {
        this.connection = connection;
        inputManager = Device.getInstance().getInputManager();
        initPointer();
    }

    private void initPointer() {
        MotionEvent.PointerProperties props = pointerProperties[0];
        props.id = 0;
        props.toolType = MotionEvent.TOOL_TYPE_MOUSE;

        MotionEvent.PointerCoords coords = pointerCoords[0];
        coords.orientation = 0;
        coords.pressure = 1;
        coords.size = 1;
        coords.toolMajor = 1;
        coords.toolMinor = 1;
        coords.touchMajor = 1;
        coords.touchMinor = 1;
    }

    private void setPointerCoords(int x, int y) {
        MotionEvent.PointerCoords coords = pointerCoords[0];
        coords.x = x;
        coords.y = y;
    }

    private void setScroll(int hScroll, int vScroll) {
        MotionEvent.PointerCoords coords = pointerCoords[0];
        coords.setAxisValue(MotionEvent.AXIS_SCROLL, hScroll);
        coords.setAxisValue(MotionEvent.AXIS_VSCROLL, vScroll);
    }

    public void control() throws IOException {
        while (handleEvent());
    }

    private boolean handleEvent() throws IOException {
        ControlEvent controlEvent = connection.receiveControlEvent();
        if (controlEvent == null) {
            return false;
        }
        switch (controlEvent.getType()) {
            case ControlEvent.TYPE_KEYCODE:
                injectKeycode(controlEvent.getAction(), controlEvent.getKeycode(), controlEvent.getMetaState());
                break;
            case ControlEvent.TYPE_TEXT:
                injectText(controlEvent.getText());
                break;
            case ControlEvent.TYPE_MOUSE:
                injectMouse(controlEvent.getAction(), controlEvent.getButtons(), controlEvent.getX(), controlEvent.getY());
                break;
            case ControlEvent.TYPE_SCROLL:
                injectScroll(controlEvent.getButtons(), controlEvent.getX(), controlEvent.getY(), controlEvent.getHScroll(), controlEvent.getVScroll());
        }
        return true;
    }

    private boolean injectKeycode(int action, int keycode, int metaState) {
        return injectKeyEvent(action, keycode, 0, metaState);
    }

    private boolean injectText(String text) {
        KeyEvent[] events = charMap.getEvents(text.toCharArray());
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

    private boolean injectMouse(int action, int buttons, int x, int y) {
        long now = SystemClock.uptimeMillis();
        if (action == MotionEvent.ACTION_DOWN) {
            lastMouseDown = now;
        }
        setPointerCoords(x, y);
        MotionEvent event = MotionEvent.obtain(lastMouseDown, now, action, 1, pointerProperties, pointerCoords, 0, buttons, 1f, 1f, 0, 0, InputDevice.SOURCE_MOUSE, 0);
        return injectEvent(event);
    }

    private boolean injectScroll(int buttons, int x, int y, int hScroll, int vScroll) {
        long now = SystemClock.uptimeMillis();
        setPointerCoords(x, y);
        setScroll(hScroll, vScroll);
        MotionEvent event = MotionEvent.obtain(lastMouseDown, now, MotionEvent.ACTION_SCROLL, 1, pointerProperties, pointerCoords, 0, 0, 1f, 1f, 0, 0, InputDevice.SOURCE_MOUSE, 0);
        return injectEvent(event);
    }

    private boolean injectKeyEvent(int action, int keyCode, int repeat, int metaState) {
        long now = SystemClock.uptimeMillis();
        KeyEvent event = new KeyEvent(now, now, action, keyCode, repeat, metaState, KeyCharacterMap.VIRTUAL_KEYBOARD, 0, 0, InputDevice.SOURCE_KEYBOARD);
        return injectEvent(event);
    }

    private boolean injectEvent(InputEvent event) {
        return inputManager.injectInputEvent(event, InputManager.INJECT_INPUT_EVENT_MODE_ASYNC);
    }
}
