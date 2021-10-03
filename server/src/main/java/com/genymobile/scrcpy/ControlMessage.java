package com.genymobile.scrcpy;

import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;

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
    public static final int TYPE_EXPAND_SETTINGS_PANEL = 6;
    public static final int TYPE_COLLAPSE_PANELS = 7;
    public static final int TYPE_GET_CLIPBOARD = 8;
    public static final int TYPE_SET_CLIPBOARD = 9;
    public static final int TYPE_SET_SCREEN_POWER_MODE = 10;
    public static final int TYPE_ROTATE_DEVICE = 11;
    public static final int TYPE_CHANGE_STREAM_PARAMETERS = 101;
    public static final int TYPE_PUSH_FILE = 102;

    public static final int PUSH_STATE_NEW = 0;
    public static final int PUSH_STATE_START = 1;
    public static final int PUSH_STATE_APPEND = 2;
    public static final int PUSH_STATE_FINISH = 3;
    public static final int PUSH_STATE_CANCEL = 4;

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
    private boolean paste;
    private int repeat;
    private byte[] bytes;
    private short pushId;
    private int pushState;
    private byte[] pushChunk;
    private int pushChunkSize;
    private int fileSize;
    private String fileName;
    private VideoSettings videoSettings;

    private ControlMessage() {
    }

    public static ControlMessage createInjectKeycode(int action, int keycode, int repeat, int metaState) {
        ControlMessage msg = new ControlMessage();
        msg.type = TYPE_INJECT_KEYCODE;
        msg.action = action;
        msg.keycode = keycode;
        msg.repeat = repeat;
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

    public static ControlMessage createBackOrScreenOn(int action) {
        ControlMessage msg = new ControlMessage();
        msg.type = TYPE_BACK_OR_SCREEN_ON;
        msg.action = action;
        return msg;
    }

    public static ControlMessage createSetClipboard(String text, boolean paste) {
        ControlMessage msg = new ControlMessage();
        msg.type = TYPE_SET_CLIPBOARD;
        msg.text = text;
        msg.paste = paste;
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

    public static ControlMessage createChangeSteamParameters(byte[] bytes) {
        ControlMessage event = new ControlMessage();
        event.type = TYPE_CHANGE_STREAM_PARAMETERS;
        event.videoSettings = VideoSettings.fromByteArray(bytes);
        return event;
    }

    public static ControlMessage createFilePush(byte[] bytes) {
        ControlMessage event = new ControlMessage();
        event.type = TYPE_PUSH_FILE;
        ByteBuffer buffer = ByteBuffer.wrap(bytes);
        event.pushId = buffer.getShort();
        event.pushState = buffer.get();
        switch (event.pushState) {
            case PUSH_STATE_START:
                event.fileSize = buffer.getInt();
                short nameLength = buffer.getShort();
                byte[] textBuffer = new byte[nameLength];
                buffer.get(textBuffer, 0, nameLength);
                event.fileName = new String(textBuffer, 0, nameLength, StandardCharsets.UTF_8);
                break;
            case PUSH_STATE_APPEND:
                int chunkSize = buffer.getInt();
                byte[] chunk = new byte[chunkSize];
                if (buffer.remaining() >= chunkSize) {
                    buffer.get(chunk, 0, chunkSize);
                    event.pushChunkSize = chunkSize;
                    event.pushChunk = chunk;
                } else {
                    event.pushState = PUSH_STATE_CANCEL;
                }
                break;
            case PUSH_STATE_NEW:
            case PUSH_STATE_CANCEL:
            case PUSH_STATE_FINISH:
                break;
                // nothing special;
            default:
                Ln.w("Unknown push event state: " + event.pushState);
                return null;
        }
        return event;
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

    public boolean getPaste() {
        return paste;
    }

    public int getRepeat() {
        return repeat;
    }

    public byte[] getBytes() {
        return bytes;
    }

    public short getPushId() {
        return pushId;
    }

    public int getPushState() {
        return pushState;
    }

    public byte[] getPushChunk() {
        return pushChunk;
    }

    public int getPushChunkSize() {
        return pushChunkSize;
    }

    public String getFileName() {
        return fileName;
    }

    public int getFileSize() {
        return fileSize;
    }

    public VideoSettings getVideoSettings() {
        return videoSettings;
    }
}
