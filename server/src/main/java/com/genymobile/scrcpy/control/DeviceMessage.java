package com.genymobile.scrcpy.control;

public final class DeviceMessage {

    public static final int TYPE_CLIPBOARD = 0;
    public static final int TYPE_ACK_CLIPBOARD = 1;
    public static final int TYPE_UHID_OUTPUT = 2;
    public static final int TYPE_MEDIA_UPDATE = 3;

    private int type;
    private String text;
    private long sequence;
    private int id;
    private byte[] data;
    private int mediaState;
    private long mediaPosition;
    private int mediaCapabilities;
    private long mediaDuration;
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
        event.mediaState = state;
        event.mediaPosition = positionMs;
        event.mediaCapabilities = capabilities;
        event.mediaDuration = durationMs;
        event.text = title;
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

    public int getMediaState() {
        return mediaState;
    }

    public long getMediaPosition() {
        return mediaPosition;
    }

    public int getMediaCapabilities() {
        return mediaCapabilities;
    }

    public long getMediaDuration() {
        return mediaDuration;
    }

    public String getArtist() {
        return artist;
    }

    public String getAlbum() {
        return album;
    }
}
