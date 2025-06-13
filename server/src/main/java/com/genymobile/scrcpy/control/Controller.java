package com.genymobile.scrcpy.control;

import com.genymobile.scrcpy.AndroidVersions;
import com.genymobile.scrcpy.AsyncProcessor;
import com.genymobile.scrcpy.CleanUp;
import com.genymobile.scrcpy.Options;
import com.genymobile.scrcpy.device.Device;
import com.genymobile.scrcpy.device.DeviceApp;
import com.genymobile.scrcpy.device.DisplayInfo;
import com.genymobile.scrcpy.device.Point;
import com.genymobile.scrcpy.device.Position;
import com.genymobile.scrcpy.device.Size;
import com.genymobile.scrcpy.util.Ln;
import com.genymobile.scrcpy.util.LogUtils;
import com.genymobile.scrcpy.video.SurfaceCapture;
import com.genymobile.scrcpy.video.VirtualDisplayListener;
import com.genymobile.scrcpy.wrappers.ClipboardManager;
import com.genymobile.scrcpy.wrappers.InputManager;
import com.genymobile.scrcpy.wrappers.ServiceManager;

import android.content.Intent;
import android.os.Build;
import android.os.SystemClock;
import android.util.Pair;
import android.view.InputDevice;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import android.view.MotionEvent;

import java.io.IOException;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

public class Controller implements AsyncProcessor, VirtualDisplayListener {

    /*
     * For event injection, there are two display ids:
     *  - the displayId passed to the constructor (which comes from --display-id passed by the client, 0 for the main display);
     *  - the virtualDisplayId used for mirroring, notified by the capture instance via the VirtualDisplayListener interface.
     *
     * (In case the ScreenCapture uses the "SurfaceControl API", then both ids are equals, but this is an implementation detail.)
     *
     * In order to make events work correctly in all cases:
     *  - virtualDisplayId must be used for events relative to the display (mouse and touch events with coordinates);
     *  - displayId must be used for other events (like key events).
     *
     * If a new separate virtual display is created (using --new-display), then displayId == Device.DISPLAY_ID_NONE. In that case, all events are
     * sent to the virtual display id.
     */

    private static final class DisplayData {
        private final int virtualDisplayId;
        private final PositionMapper positionMapper;

        private DisplayData(int virtualDisplayId, PositionMapper positionMapper) {
            this.virtualDisplayId = virtualDisplayId;
            this.positionMapper = positionMapper;
        }
    }

    private static final int DEFAULT_DEVICE_ID = 0;

    // control_msg.h values of the pointerId field in inject_touch_event message
    private static final int POINTER_ID_MOUSE = -1;

    private static final ScheduledExecutorService EXECUTOR = Executors.newSingleThreadScheduledExecutor();
    private ExecutorService startAppExecutor;

    private Thread thread;

    private UhidManager uhidManager;

    private final int displayId;
    private final boolean supportsInputEvents;
    private final ControlChannel controlChannel;
    private final CleanUp cleanUp;
    private final DeviceMessageSender sender;
    private final boolean clipboardAutosync;
    private final boolean powerOn;

    private final KeyCharacterMap charMap = KeyCharacterMap.load(KeyCharacterMap.VIRTUAL_KEYBOARD);

    private final AtomicBoolean isSettingClipboard = new AtomicBoolean();

    private final AtomicReference<DisplayData> displayData = new AtomicReference<>();
    private final Object displayDataAvailable = new Object(); // condition variable

    private long lastTouchDown;
    private final PointersState pointersState = new PointersState();
    private final MotionEvent.PointerProperties[] pointerProperties = new MotionEvent.PointerProperties[PointersState.MAX_POINTERS];
    private final MotionEvent.PointerCoords[] pointerCoords = new MotionEvent.PointerCoords[PointersState.MAX_POINTERS];

    private boolean keepDisplayPowerOff;

    // Used for resetting video encoding on RESET_VIDEO message
    private SurfaceCapture surfaceCapture;

    public Controller(ControlChannel controlChannel, CleanUp cleanUp, Options options) {
        this.displayId = options.getDisplayId();
        this.controlChannel = controlChannel;
        this.cleanUp = cleanUp;
        this.clipboardAutosync = options.getClipboardAutosync();
        this.powerOn = options.getPowerOn();
        initPointers();
        sender = new DeviceMessageSender(controlChannel);

        supportsInputEvents = Device.supportsInputEvents(displayId);
        if (!supportsInputEvents) {
            Ln.w("Input events are not supported for secondary displays before Android 10");
        }

        // Make sure the clipboard manager is always created from the main thread (even if clipboardAutosync is disabled)
        ClipboardManager clipboardManager = ServiceManager.getClipboardManager();
        if (clipboardAutosync) {
            // If control and autosync are enabled, synchronize Android clipboard to the computer automatically
            if (clipboardManager != null) {
                clipboardManager.addPrimaryClipChangedListener(() -> {
                    if (isSettingClipboard.get()) {
                        // This is a notification for the change we are currently applying, ignore it
                        return;
                    }
                    String text = Device.getClipboardText();
                    if (text != null) {
                        DeviceMessage msg = DeviceMessage.createClipboard(text);
                        sender.send(msg);
                    }
                });
            } else {
                Ln.w("No clipboard manager, copy-paste between device and computer will not work");
            }
        }
    }

    @Override
    public void onNewVirtualDisplay(int virtualDisplayId, PositionMapper positionMapper) {
        DisplayData data = new DisplayData(virtualDisplayId, positionMapper);
        DisplayData old = this.displayData.getAndSet(data);
        if (old == null) {
            // The very first time the Controller is notified of a new virtual display
            synchronized (displayDataAvailable) {
                displayDataAvailable.notify();
            }
        }
    }

    public void setSurfaceCapture(SurfaceCapture surfaceCapture) {
        this.surfaceCapture = surfaceCapture;
    }

    private UhidManager getUhidManager() {
        if (uhidManager == null) {
            int uhidDisplayId = displayId;
            if (Build.VERSION.SDK_INT >= AndroidVersions.API_35_ANDROID_15) {
                if (displayId == Device.DISPLAY_ID_NONE) {
                    // Mirroring a new virtual display id (using --new-display-id feature) on Android >= 15, where the UHID mouse pointer can be
                    // associated to the virtual display
                    try {
                        // Wait for at most 1 second until a virtual display id is known
                        DisplayData data = waitDisplayData(1000);
                        if (data != null) {
                            uhidDisplayId = data.virtualDisplayId;
                        }
                    } catch (InterruptedException e) {
                        // do nothing
                    }
                }
            }

            String displayUniqueId = null;
            if (uhidDisplayId > 0) {
                // Ignore Device.DISPLAY_ID_NONE and 0 (main display)
                DisplayInfo displayInfo = ServiceManager.getDisplayManager().getDisplayInfo(uhidDisplayId);
                if (displayInfo != null) {
                    displayUniqueId = displayInfo.getUniqueId();
                }
            }
            uhidManager = new UhidManager(sender, displayUniqueId);
        }

        return uhidManager;
    }

    private void initPointers() {
        for (int i = 0; i < PointersState.MAX_POINTERS; ++i) {
            MotionEvent.PointerProperties props = new MotionEvent.PointerProperties();
            props.toolType = MotionEvent.TOOL_TYPE_FINGER;

            MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
            coords.orientation = 0;
            coords.size = 0;

            pointerProperties[i] = props;
            pointerCoords[i] = coords;
        }
    }

    private void control() throws IOException {
        // on start, power on the device
        if (powerOn && displayId == 0 && !Device.isScreenOn(displayId)) {
            Device.pressReleaseKeycode(KeyEvent.KEYCODE_POWER, displayId, Device.INJECT_MODE_ASYNC);

            // dirty hack
            // After POWER is injected, the device is powered on asynchronously.
            // To turn the device screen off while mirroring, the client will send a message that
            // would be handled before the device is actually powered on, so its effect would
            // be "canceled" once the device is turned back on.
            // Adding this delay prevents to handle the message before the device is actually
            // powered on.
            SystemClock.sleep(500);
        }

        boolean alive = true;
        while (!Thread.currentThread().isInterrupted() && alive) {
            alive = handleEvent();
        }
    }

    @Override
    public void start(TerminationListener listener) {
        thread = new Thread(() -> {
            try {
                control();
            } catch (IOException e) {
                Ln.e("Controller error", e);
            } finally {
                Ln.d("Controller stopped");
                if (uhidManager != null) {
                    uhidManager.closeAll();
                }
                listener.onTerminated(true);
            }
        }, "control-recv");
        thread.start();
        sender.start();
    }

    @Override
    public void stop() {
        if (thread != null) {
            thread.interrupt();
        }
        sender.stop();
    }

    @Override
    public void join() throws InterruptedException {
        if (thread != null) {
            thread.join();
        }
        sender.join();
    }

    private boolean handleEvent() throws IOException {
        ControlMessage msg;
        try {
            msg = controlChannel.recv();
        } catch (IOException e) {
            // this is expected on close
            return false;
        }

        switch (msg.getType()) {
            case ControlMessage.TYPE_INJECT_KEYCODE:
                if (supportsInputEvents) {
                    injectKeycode(msg.getAction(), msg.getKeycode(), msg.getRepeat(), msg.getMetaState());
                }
                break;
            case ControlMessage.TYPE_INJECT_TEXT:
                if (supportsInputEvents) {
                    injectText(msg.getText());
                }
                break;
            case ControlMessage.TYPE_INJECT_TOUCH_EVENT:
                if (supportsInputEvents) {
                    injectTouch(msg.getAction(), msg.getPointerId(), msg.getPosition(), msg.getPressure(), msg.getActionButton(), msg.getButtons());
                }
                break;
            case ControlMessage.TYPE_INJECT_SCROLL_EVENT:
                if (supportsInputEvents) {
                    injectScroll(msg.getPosition(), msg.getHScroll(), msg.getVScroll(), msg.getButtons());
                }
                break;
            case ControlMessage.TYPE_BACK_OR_SCREEN_ON:
                if (supportsInputEvents) {
                    pressBackOrTurnScreenOn(msg.getAction());
                }
                break;
            case ControlMessage.TYPE_EXPAND_NOTIFICATION_PANEL:
                Device.expandNotificationPanel();
                break;
            case ControlMessage.TYPE_EXPAND_SETTINGS_PANEL:
                Device.expandSettingsPanel();
                break;
            case ControlMessage.TYPE_COLLAPSE_PANELS:
                Device.collapsePanels();
                break;
            case ControlMessage.TYPE_GET_CLIPBOARD:
                getClipboard(msg.getCopyKey());
                break;
            case ControlMessage.TYPE_SET_CLIPBOARD:
                setClipboard(msg.getText(), msg.getPaste(), msg.getSequence());
                break;
            case ControlMessage.TYPE_SET_DISPLAY_POWER:
                if (supportsInputEvents) {
                    setDisplayPower(msg.getOn());
                }
                break;
            case ControlMessage.TYPE_ROTATE_DEVICE:
                Device.rotateDevice(getActionDisplayId());
                break;
            case ControlMessage.TYPE_UHID_CREATE:
                getUhidManager().open(msg.getId(), msg.getVendorId(), msg.getProductId(), msg.getText(), msg.getData());
                break;
            case ControlMessage.TYPE_UHID_INPUT:
                getUhidManager().writeInput(msg.getId(), msg.getData());
                break;
            case ControlMessage.TYPE_UHID_DESTROY:
                getUhidManager().close(msg.getId());
                break;
            case ControlMessage.TYPE_OPEN_HARD_KEYBOARD_SETTINGS:
                openHardKeyboardSettings();
                break;
            case ControlMessage.TYPE_START_APP:
                startAppAsync(msg.getText());
                break;
            case ControlMessage.TYPE_RESET_VIDEO:
                resetVideo();
                break;
            default:
                // do nothing
        }

        return true;
    }

    private boolean injectKeycode(int action, int keycode, int repeat, int metaState) {
        if (keepDisplayPowerOff && action == KeyEvent.ACTION_UP && (keycode == KeyEvent.KEYCODE_POWER || keycode == KeyEvent.KEYCODE_WAKEUP)) {
            assert displayId != Device.DISPLAY_ID_NONE;
            scheduleDisplayPowerOff(displayId);
        }
        return injectKeyEvent(action, keycode, repeat, metaState, Device.INJECT_MODE_ASYNC);
    }

    private boolean injectChar(char c) {
        String decomposed = KeyComposition.decompose(c);
        char[] chars = decomposed != null ? decomposed.toCharArray() : new char[]{c};
        KeyEvent[] events = charMap.getEvents(chars);
        if (events == null) {
            return false;
        }

        int actionDisplayId = getActionDisplayId();
        for (KeyEvent event : events) {
            if (!Device.injectEvent(event, actionDisplayId, Device.INJECT_MODE_ASYNC)) {
                return false;
            }
        }
        return true;
    }

    private int injectText(String text) {
        int successCount = 0;
        for (char c : text.toCharArray()) {
            if (!injectChar(c)) {
                Ln.w("Could not inject char u+" + String.format("%04x", (int) c));
                continue;
            }
            successCount++;
        }
        return successCount;
    }

    private Pair<Point, Integer> getEventPointAndDisplayId(Position position) {
        // it hides the field on purpose, to read it with atomic access
        @SuppressWarnings("checkstyle:HiddenField")
        DisplayData displayData = this.displayData.get();
        // In scrcpy, displayData should never be null (a touch event can only be generated from the client when a video frame is present).
        // However, it is possible to send events without video playback when using scrcpy-server alone (except for virtual displays).
        assert displayData != null || displayId != Device.DISPLAY_ID_NONE : "Cannot receive a positional event without a display";

        Point point;
        int targetDisplayId;
        if (displayData != null) {
            point = displayData.positionMapper.map(position);
            if (point == null) {
                if (Ln.isEnabled(Ln.Level.VERBOSE)) {
                    Size eventSize = position.getScreenSize();
                    Size currentSize = displayData.positionMapper.getVideoSize();
                    Ln.v("Ignore positional event generated for size " + eventSize + " (current size is " + currentSize + ")");
                }
                return null;
            }
            targetDisplayId = displayData.virtualDisplayId;
        } else {
            // No display, use the raw coordinates
            point = position.getPoint();
            targetDisplayId = displayId;
        }

        return Pair.create(point, targetDisplayId);
    }

    private boolean injectTouch(int action, long pointerId, Position position, float pressure, int actionButton, int buttons) {
        long now = SystemClock.uptimeMillis();

        Pair<Point, Integer> pair = getEventPointAndDisplayId(position);
        if (pair == null) {
            return false;
        }

        Point point = pair.first;
        int targetDisplayId = pair.second;

        int pointerIndex = pointersState.getPointerIndex(pointerId);
        if (pointerIndex == -1) {
            Ln.w("Too many pointers for touch event");
            return false;
        }
        Pointer pointer = pointersState.get(pointerIndex);
        pointer.setPoint(point);
        pointer.setPressure(pressure);

        int source;
        boolean activeSecondaryButtons = ((actionButton | buttons) & ~MotionEvent.BUTTON_PRIMARY) != 0;
        if (pointerId == POINTER_ID_MOUSE && (action == MotionEvent.ACTION_HOVER_MOVE || activeSecondaryButtons)) {
            // real mouse event, or event incompatible with a finger
            pointerProperties[pointerIndex].toolType = MotionEvent.TOOL_TYPE_MOUSE;
            source = InputDevice.SOURCE_MOUSE;
            pointer.setUp(buttons == 0);
        } else {
            // POINTER_ID_GENERIC_FINGER, POINTER_ID_VIRTUAL_FINGER or real touch from device
            pointerProperties[pointerIndex].toolType = MotionEvent.TOOL_TYPE_FINGER;
            source = InputDevice.SOURCE_TOUCHSCREEN;
            // Buttons must not be set for touch events
            buttons = 0;
            pointer.setUp(action == MotionEvent.ACTION_UP);
        }

        int pointerCount = pointersState.update(pointerProperties, pointerCoords);
        if (pointerCount == 1) {
            if (action == MotionEvent.ACTION_DOWN) {
                lastTouchDown = now;
            }
        } else {
            // secondary pointers must use ACTION_POINTER_* ORed with the pointerIndex
            if (action == MotionEvent.ACTION_UP) {
                action = MotionEvent.ACTION_POINTER_UP | (pointerIndex << MotionEvent.ACTION_POINTER_INDEX_SHIFT);
            } else if (action == MotionEvent.ACTION_DOWN) {
                action = MotionEvent.ACTION_POINTER_DOWN | (pointerIndex << MotionEvent.ACTION_POINTER_INDEX_SHIFT);
            }
        }

        /* If the input device is a mouse (on API >= 23):
         *   - the first button pressed must first generate ACTION_DOWN;
         *   - all button pressed (including the first one) must generate ACTION_BUTTON_PRESS;
         *   - all button released (including the last one) must generate ACTION_BUTTON_RELEASE;
         *   - the last button released must in addition generate ACTION_UP.
         *
         * Otherwise, Chrome does not work properly: <https://github.com/Genymobile/scrcpy/issues/3635>
         */
        if (Build.VERSION.SDK_INT >= AndroidVersions.API_23_ANDROID_6_0 && source == InputDevice.SOURCE_MOUSE) {
            if (action == MotionEvent.ACTION_DOWN) {
                if (actionButton == buttons) {
                    // First button pressed: ACTION_DOWN
                    MotionEvent downEvent = MotionEvent.obtain(lastTouchDown, now, MotionEvent.ACTION_DOWN, pointerCount, pointerProperties,
                            pointerCoords, 0, buttons, 1f, 1f, DEFAULT_DEVICE_ID, 0, source, 0);
                    if (!Device.injectEvent(downEvent, targetDisplayId, Device.INJECT_MODE_ASYNC)) {
                        return false;
                    }
                }

                // Any button pressed: ACTION_BUTTON_PRESS
                MotionEvent pressEvent = MotionEvent.obtain(lastTouchDown, now, MotionEvent.ACTION_BUTTON_PRESS, pointerCount, pointerProperties,
                        pointerCoords, 0, buttons, 1f, 1f, DEFAULT_DEVICE_ID, 0, source, 0);
                if (!InputManager.setActionButton(pressEvent, actionButton)) {
                    return false;
                }
                if (!Device.injectEvent(pressEvent, targetDisplayId, Device.INJECT_MODE_ASYNC)) {
                    return false;
                }

                return true;
            }

            if (action == MotionEvent.ACTION_UP) {
                // Any button released: ACTION_BUTTON_RELEASE
                MotionEvent releaseEvent = MotionEvent.obtain(lastTouchDown, now, MotionEvent.ACTION_BUTTON_RELEASE, pointerCount, pointerProperties,
                        pointerCoords, 0, buttons, 1f, 1f, DEFAULT_DEVICE_ID, 0, source, 0);
                if (!InputManager.setActionButton(releaseEvent, actionButton)) {
                    return false;
                }
                if (!Device.injectEvent(releaseEvent, targetDisplayId, Device.INJECT_MODE_ASYNC)) {
                    return false;
                }

                if (buttons == 0) {
                    // Last button released: ACTION_UP
                    MotionEvent upEvent = MotionEvent.obtain(lastTouchDown, now, MotionEvent.ACTION_UP, pointerCount, pointerProperties,
                            pointerCoords, 0, buttons, 1f, 1f, DEFAULT_DEVICE_ID, 0, source, 0);
                    if (!Device.injectEvent(upEvent, targetDisplayId, Device.INJECT_MODE_ASYNC)) {
                        return false;
                    }
                }

                return true;
            }
        }

        MotionEvent event = MotionEvent.obtain(lastTouchDown, now, action, pointerCount, pointerProperties, pointerCoords, 0, buttons, 1f, 1f,
                DEFAULT_DEVICE_ID, 0, source, 0);
        return Device.injectEvent(event, targetDisplayId, Device.INJECT_MODE_ASYNC);
    }

    private boolean injectScroll(Position position, float hScroll, float vScroll, int buttons) {
        long now = SystemClock.uptimeMillis();

        Pair<Point, Integer> pair = getEventPointAndDisplayId(position);
        if (pair == null) {
            return false;
        }

        Point point = pair.first;
        int targetDisplayId = pair.second;

        MotionEvent.PointerProperties props = pointerProperties[0];
        props.id = 0;

        MotionEvent.PointerCoords coords = pointerCoords[0];
        coords.x = point.getX();
        coords.y = point.getY();
        coords.setAxisValue(MotionEvent.AXIS_HSCROLL, hScroll);
        coords.setAxisValue(MotionEvent.AXIS_VSCROLL, vScroll);

        MotionEvent event = MotionEvent.obtain(lastTouchDown, now, MotionEvent.ACTION_SCROLL, 1, pointerProperties, pointerCoords, 0, buttons, 1f, 1f,
                DEFAULT_DEVICE_ID, 0, InputDevice.SOURCE_MOUSE, 0);
        return Device.injectEvent(event, targetDisplayId, Device.INJECT_MODE_ASYNC);
    }

    /**
     * Schedule a call to set display power to off after a small delay.
     */
    private static void scheduleDisplayPowerOff(int displayId) {
        EXECUTOR.schedule(() -> {
            Ln.i("Forcing display off");
            Device.setDisplayPower(displayId, false);
        }, 200, TimeUnit.MILLISECONDS);
    }

    private boolean pressBackOrTurnScreenOn(int action) {
        if (displayId == Device.DISPLAY_ID_NONE || Device.isScreenOn(displayId)) {
            return injectKeyEvent(action, KeyEvent.KEYCODE_BACK, 0, 0, Device.INJECT_MODE_ASYNC);
        }

        // Screen is off
        // Only press POWER on ACTION_DOWN
        if (action != KeyEvent.ACTION_DOWN) {
            // do nothing,
            return true;
        }

        if (keepDisplayPowerOff) {
            assert displayId != Device.DISPLAY_ID_NONE;
            scheduleDisplayPowerOff(displayId);
        }
        return pressReleaseKeycode(KeyEvent.KEYCODE_POWER, Device.INJECT_MODE_ASYNC);
    }

    private void getClipboard(int copyKey) {
        // On Android >= 7, press the COPY or CUT key if requested
        if (copyKey != ControlMessage.COPY_KEY_NONE && Build.VERSION.SDK_INT >= AndroidVersions.API_24_ANDROID_7_0 && supportsInputEvents) {
            int key = copyKey == ControlMessage.COPY_KEY_COPY ? KeyEvent.KEYCODE_COPY : KeyEvent.KEYCODE_CUT;
            // Wait until the event is finished, to ensure that the clipboard text we read just after is the correct one
            pressReleaseKeycode(key, Device.INJECT_MODE_WAIT_FOR_FINISH);
        }

        // If clipboard autosync is enabled, then the device clipboard is synchronized to the computer clipboard whenever it changes, in
        // particular when COPY or CUT are injected, so it should not be synchronized twice. On Android < 7, do not synchronize at all rather than
        // copying an old clipboard content.
        if (!clipboardAutosync) {
            String clipboardText = Device.getClipboardText();
            if (clipboardText != null) {
                DeviceMessage msg = DeviceMessage.createClipboard(clipboardText);
                sender.send(msg);
            }
        }
    }

    private boolean setClipboard(String text, boolean paste, long sequence) {
        isSettingClipboard.set(true);
        boolean ok = Device.setClipboardText(text);
        isSettingClipboard.set(false);
        if (ok) {
            Ln.i("Device clipboard set");
        }

        // On Android >= 7, also press the PASTE key if requested
        if (paste && Build.VERSION.SDK_INT >= AndroidVersions.API_24_ANDROID_7_0 && supportsInputEvents) {
            pressReleaseKeycode(KeyEvent.KEYCODE_PASTE, Device.INJECT_MODE_ASYNC);
        }

        if (sequence != ControlMessage.SEQUENCE_INVALID) {
            // Acknowledgement requested
            DeviceMessage msg = DeviceMessage.createAckClipboard(sequence);
            sender.send(msg);
        }

        return ok;
    }

    private void openHardKeyboardSettings() {
        Intent intent = new Intent("android.settings.HARD_KEYBOARD_SETTINGS");
        ServiceManager.getActivityManager().startActivity(intent);
    }

    private boolean injectKeyEvent(int action, int keyCode, int repeat, int metaState, int injectMode) {
        return Device.injectKeyEvent(action, keyCode, repeat, metaState, getActionDisplayId(), injectMode);
    }

    private boolean pressReleaseKeycode(int keyCode, int injectMode) {
        return Device.pressReleaseKeycode(keyCode, getActionDisplayId(), injectMode);
    }

    private int getActionDisplayId() {
        if (displayId != Device.DISPLAY_ID_NONE) {
            // Real screen mirrored, use the source display id
            return displayId;
        }

        // Virtual display created by --new-display, use the virtualDisplayId
        DisplayData data = displayData.get();
        if (data == null) {
            // If no virtual display id is initialized yet, use the main display id
            return 0;
        }

        return data.virtualDisplayId;
    }

    private void startAppAsync(String name) {
        if (startAppExecutor == null) {
            startAppExecutor = Executors.newSingleThreadExecutor();
        }

        // Listing and selecting the app may take a lot of time
        startAppExecutor.submit(() -> startApp(name));
    }

    private void startApp(String name) {
        boolean forceStopBeforeStart = name.startsWith("+");
        if (forceStopBeforeStart) {
            name = name.substring(1);
        }

        DeviceApp app;
        boolean searchByName = name.startsWith("?");
        if (searchByName) {
            name = name.substring(1);

            Ln.i("Processing Android apps... (this may take some time)");
            List<DeviceApp> apps = Device.findByName(name);
            if (apps.isEmpty()) {
                Ln.w("No app found for name \"" + name + "\"");
                return;
            }

            if (apps.size() > 1) {
                String title = "No unique app found for name \"" + name + "\":";
                Ln.w(LogUtils.buildAppListMessage(title, apps));
                return;
            }

            app = apps.get(0);
        } else {
            app = Device.findByPackageName(name);
            if (app == null) {
                Ln.w("No app found for package \"" + name + "\"");
                return;
            }
        }

        int startAppDisplayId = getStartAppDisplayId();
        if (startAppDisplayId == Device.DISPLAY_ID_NONE) {
            Ln.e("No known display id to start app \"" + name + "\"");
            return;
        }

        Ln.i("Starting app \"" + app.getName() + "\" [" + app.getPackageName() + "] on display " + startAppDisplayId + "...");
        Device.startApp(app.getPackageName(), startAppDisplayId, forceStopBeforeStart);
    }

    private int getStartAppDisplayId() {
        if (displayId != Device.DISPLAY_ID_NONE) {
            return displayId;
        }

        // Mirroring a new virtual display id (using --new-display-id feature)
        try {
            // Wait for at most 1 second until a virtual display id is known
            DisplayData data = waitDisplayData(1000);
            if (data != null) {
                return data.virtualDisplayId;
            }
        } catch (InterruptedException e) {
            // do nothing
        }

        // No display id available
        return Device.DISPLAY_ID_NONE;
    }

    private DisplayData waitDisplayData(long timeoutMillis) throws InterruptedException {
        long deadline = System.currentTimeMillis() + timeoutMillis;

        synchronized (displayDataAvailable) {
            DisplayData data = displayData.get();
            while (data == null) {
                long timeout = deadline - System.currentTimeMillis();
                if (timeout < 0) {
                    return null;
                }
                if (timeout > 0) {
                    displayDataAvailable.wait(timeout);
                }
                data = displayData.get();
            }

            return data;
        }
    }

    private void setDisplayPower(boolean on) {
        // Change the power of the main display when mirroring a virtual display
        int targetDisplayId = displayId != Device.DISPLAY_ID_NONE ? displayId : 0;
        boolean setDisplayPowerOk = Device.setDisplayPower(targetDisplayId, on);
        if (setDisplayPowerOk) {
            // Do not keep display power off for virtual displays: MOD+p must wake up the physical device
            keepDisplayPowerOff = displayId != Device.DISPLAY_ID_NONE && !on;
            Ln.i("Device display turned " + (on ? "on" : "off"));
            if (cleanUp != null) {
                boolean mustRestoreOnExit = !on;
                cleanUp.setRestoreDisplayPower(mustRestoreOnExit);
            }
        }
    }

    private void resetVideo() {
        if (surfaceCapture != null) {
            Ln.i("Video capture reset");
            surfaceCapture.requestInvalidate();
        }
    }
}
