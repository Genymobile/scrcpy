package com.genymobile.scrcpy;

import android.graphics.Rect;

import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.List;
import java.util.Objects;

public class VideoSettings {
    private static final int DEFAULT_BIT_RATE = 8000000;
    private static final byte DEFAULT_MAX_FPS = 60;
    private static final byte DEFAULT_I_FRAME_INTERVAL = 10; // seconds

    private Size bounds;
    private int bitRate = DEFAULT_BIT_RATE;
    private int maxFps;
    private int lockedVideoOrientation;
    private byte iFrameInterval = DEFAULT_I_FRAME_INTERVAL;
    private Rect crop;
    private boolean sendFrameMeta; // send PTS so that the client may record properly
    private int displayId;
    private String codecOptionsString;
    private List<CodecOption> codecOptions;
    private String encoderName;

    public int getBitRate() {
        return bitRate;
    }

    public void setBitRate(int bitRate) {
        this.bitRate = bitRate;
    }

    public int getIFrameInterval() {
        return iFrameInterval;
    }

    public void setIFrameInterval(byte iFrameInterval) {
        this.iFrameInterval = iFrameInterval;
    };

    public Rect getCrop() {
        return crop;
    }

    public void setCrop(Rect crop) {
        this.crop = crop;
    }

    public boolean getSendFrameMeta() {
        return sendFrameMeta;
    }

    public void setSendFrameMeta(boolean sendFrameMeta) {
        this.sendFrameMeta = sendFrameMeta;
    }

    public int getDisplayId() {
        return displayId;
    }

    public void setDisplayId(int displayId) {
        this.displayId = displayId;
    }

    public int getMaxFps() {
        return maxFps;
    }

    public void setMaxFps(int maxFps) {
        this.maxFps = maxFps;
    }

    public int getLockedVideoOrientation() {
        return lockedVideoOrientation;
    }

    public void setLockedVideoOrientation(int lockedVideoOrientation) {
        this.lockedVideoOrientation = lockedVideoOrientation;
    }

    public Size getBounds() {
        return bounds;
    }

    public void setBounds(Size bounds) {
        this.bounds = bounds;
    }

    public void setBounds(int width, int height) {
        this.bounds = new Size(width & ~15, height & ~15); // multiple of 16
    }

    public List<CodecOption> getCodecOptions() {
        return codecOptions;
    }

    public void setCodecOptions(String codecOptionsString) {
        this.codecOptions = CodecOption.parse(codecOptionsString);
        if (codecOptionsString.equals("-")) {
            this.codecOptionsString = null;
        } else {
            this.codecOptionsString = codecOptionsString;
        }
    }

    public String getEncoderName() {
        return this.encoderName;
    }

    public void setEncoderName(String encoderName) {
        if (encoderName != null && encoderName.equals("-")) {
            this.encoderName = null;
        } else {
            this.encoderName = encoderName;
        }
    }

    public byte[] toByteArray() {
        // 35 bytes without codec options and encoder name
        int baseLength = 35;
        int additionalLength = 0;
        byte[] codeOptionsBytes = new byte[]{};
        if (this.codecOptionsString != null) {
            codeOptionsBytes = this.codecOptionsString.getBytes(StandardCharsets.UTF_8);
            additionalLength += codeOptionsBytes.length;
        }
        byte[] encoderNameBytes = new byte[]{};
        if (this.encoderName != null) {
            encoderNameBytes = this.encoderName.getBytes(StandardCharsets.UTF_8);
            additionalLength += encoderNameBytes.length;
        }
        ByteBuffer temp = ByteBuffer.allocate(baseLength + additionalLength);
        temp.putInt(bitRate);
        temp.putInt(maxFps);
        temp.put(iFrameInterval);
        int width = 0;
        int height = 0;
        if (bounds != null) {
            width = bounds.getWidth();
            height = bounds.getHeight();
        }
        temp.putShort((short) width);
        temp.putShort((short) height);
        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;
        if (crop != null) {
            left = crop.left;
            top = crop.top;
            right = crop.right;
            bottom = crop.bottom;
        }
        temp.putShort((short) left);
        temp.putShort((short) top);
        temp.putShort((short) right);
        temp.putShort((short) bottom);
        temp.put((byte) (sendFrameMeta ? 1 : 0));
        temp.put((byte) lockedVideoOrientation);
        temp.putInt(displayId);
        temp.putInt(codeOptionsBytes.length);
        if (codeOptionsBytes.length != 0) {
            temp.put(codeOptionsBytes);
        }
        temp.putInt(encoderNameBytes.length);
        if (encoderNameBytes.length != 0) {
            temp.put(encoderNameBytes);
        }
        return temp.array();
    }

    public void merge(VideoSettings source) {
        codecOptions = source.codecOptions;
        codecOptionsString = source.codecOptionsString;
        encoderName = source.encoderName;
        bitRate = source.bitRate;
        maxFps = source.maxFps;
        iFrameInterval = source.iFrameInterval;
        bounds = source.bounds;
        crop = source.crop;
        sendFrameMeta = source.sendFrameMeta;
        lockedVideoOrientation = source.lockedVideoOrientation;
        displayId = source.displayId;
    }

    public static VideoSettings fromByteArray(byte[] bytes) {
        VideoSettings videoSettings = new VideoSettings();
        mergeFromByteArray(videoSettings, bytes);
        return videoSettings;
    }

    public static void mergeFromByteArray(VideoSettings videoSettings, byte[] bytes) {
        ByteBuffer data = ByteBuffer.wrap(bytes);
        int bitRate = data.getInt();
        int maxFps = data.getInt();
        byte iFrameInterval = data.get();
        int width = data.getShort();
        int height = data.getShort();
        int left = data.getShort();
        int top = data.getShort();
        int right = data.getShort();
        int bottom = data.getShort();
        boolean sendMetaFrame = data.get() != 0;
        int lockedVideoOrientation = data.get();
        int displayId = data.getInt();
        if (data.remaining() > 0) {
            int codecOptionsLength = data.getInt();
            if (codecOptionsLength > 0) {
                byte[] textBuffer = new byte[codecOptionsLength];
                data.get(textBuffer, 0, codecOptionsLength);
                String codecOptions = new String(textBuffer, 0, codecOptionsLength, StandardCharsets.UTF_8);
                if (!codecOptions.isEmpty()) {
                    videoSettings.setCodecOptions(codecOptions);
                }
            }
        }
        if (data.remaining() > 0) {
            int encoderNameLength = data.getInt();
            if (encoderNameLength > 0) {
                byte[] textBuffer = new byte[encoderNameLength];
                data.get(textBuffer, 0, encoderNameLength);
                String encoderName = new String(textBuffer, 0, encoderNameLength, StandardCharsets.UTF_8);
                if (!encoderName.isEmpty()) {
                    videoSettings.setEncoderName(encoderName);
                }
            }
        }
        videoSettings.setBitRate(bitRate);
        videoSettings.setMaxFps(maxFps);
        videoSettings.setIFrameInterval(iFrameInterval);
        videoSettings.setBounds(width, height);
        if (left == 0 && right == 0 && top == 0 && bottom == 0) {
            videoSettings.setCrop(null);
        } else {
            videoSettings.setCrop(new Rect(left, top, right, bottom));
        }
        videoSettings.setSendFrameMeta(sendMetaFrame);
        videoSettings.setLockedVideoOrientation(lockedVideoOrientation);
        if (displayId > 0) {
            videoSettings.setDisplayId(displayId);
        }
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (o == null || getClass() != o.getClass()) {
            return false;
        }

        VideoSettings s = (VideoSettings) o;
        if (bitRate != s.bitRate || maxFps != s.maxFps || lockedVideoOrientation != s.lockedVideoOrientation || iFrameInterval != s.iFrameInterval
                || sendFrameMeta != s.sendFrameMeta || displayId != s.displayId) {
            return false;
        }
        if (!Objects.equals(codecOptionsString, s.codecOptionsString) || !Objects.equals(encoderName, s.encoderName)
                || !Objects.equals(bounds, s.bounds) || !Objects.equals(crop, s.crop)) {
            return false;
        }
        return true;
    }

    @Override
    public int hashCode() {
        return Objects.hash(bitRate, maxFps, lockedVideoOrientation, iFrameInterval, sendFrameMeta,
                displayId, Objects.hashCode(codecOptionsString), Objects.hashCode(encoderName),
                Objects.hashCode(bounds), Objects.hashCode(crop));
    }

    @Override
    public String toString() {
        return "VideoSettings{"
                + "bitRate=" + bitRate
                + ", maxFps=" + maxFps
                + ", iFrameInterval=" + iFrameInterval
                + ", bounds=" + bounds
                + ", crop=" + crop
                + ", metaFrame=" + sendFrameMeta
                + ", lockedVideoOrientation=" + lockedVideoOrientation
                + ", displayId=" + displayId
                + ", codecOptions=" + (this.codecOptionsString == null ? "-" : this.codecOptionsString)
                + ", encoderName=" + (this.encoderName == null ? "-" : this.encoderName)
                + "}";
    }

}
