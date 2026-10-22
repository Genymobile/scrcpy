package com.genymobile.scrcpy.control;

public final class DeviceMessage {

    public static final int TYPE_CLIPBOARD = 0;
    public static final int TYPE_ACK_CLIPBOARD = 1;
    public static final int TYPE_UHID_OUTPUT = 2;
    public static final int TYPE_MEDIA_UPDATE = 3;

    // Values must be kept in sync with the client (device_msg.h)
    public static final int MEDIA_STATE_STOPPED = 0;
    public static final int MEDIA_STATE_PLAYING = 1;
    public static final int MEDIA_STATE_PAUSED = 2;

    public static final int MEDIA_CAP_PLAY = 1 << 0;
    public static final int MEDIA_CAP_PAUSE = 1 << 1;
    public static final int MEDIA_CAP_NEXT = 1 << 2;
    public static final int MEDIA_CAP_PREVIOUS = 1 << 3;
    public static final int MEDIA_CAP_SEEK = 1 << 4;

    public static final long MEDIA_POSITION_UNKNOWN = -1;
    public static final long MEDIA_DURATION_UNKNOWN = -1;

    private int type;
    private String text;
    private long sequence;
    private int id;
    private byte[] data;
    private String title;
    private int state;
    private long position;
    private int capabilities;
    private long duration;
    private String artist;
    private String album;

    private DeviceMessage() {
    }

    public static DeviceMessage createClipboard(String text) {
        DeviceMessage event = new DeviceMessage();
        event.type = TYPE_CLIPBOARD;
        event.text = text;
        return event;
    }

    public static DeviceMessage createAckClipboard(long sequence) {
        DeviceMessage event = new DeviceMessage();
        event.type = TYPE_ACK_CLIPBOARD;
        event.sequence = sequence;
        return event;
    }

    public static DeviceMessage createUhidOutput(int id, byte[] data) {
        DeviceMessage event = new DeviceMessage();
        event.type = TYPE_UHID_OUTPUT;
        event.id = id;
        event.data = data;
        return event;
    }

    public static DeviceMessage createMediaUpdate(int state, long positionMs, int capabilities, long durationMs, String title, String artist,
            String album) {
        DeviceMessage event = new DeviceMessage();
        event.type = TYPE_MEDIA_UPDATE;
        event.state = state;
        event.position = positionMs;
        event.capabilities = capabilities;
        event.duration = durationMs;
        event.title = title;
        event.artist = artist;
        event.album = album;
        return event;
    }

    public int getType() {
        return type;
    }

    public String getText() {
        return text;
    }

    public long getSequence() {
        return sequence;
    }

    public int getId() {
        return id;
    }

    public byte[] getData() {
        return data;
    }

    public String getTitle() {
        return title;
    }

    public int getState() {
        return state;
    }

    public long getPosition() {
        return position;
    }

    public int getCapabilities() {
        return capabilities;
    }

    public long getDuration() {
        return duration;
    }

    public String getArtist() {
        return artist;
    }

    public String getAlbum() {
        return album;
    }
}
