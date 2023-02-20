package com.genymobile.scrcpy;

import android.graphics.Rect;

import java.util.List;

public class Options {

    private Ln.Level logLevel = Ln.Level.DEBUG;
    private int scid = -1; // 31-bit non-negative value, or -1
    private boolean audio = true;
    private int maxSize;
    private VideoCodec videoCodec = VideoCodec.H264;
    private int bitRate = 8000000;
    private int maxFps;
    private int lockVideoOrientation = -1;
    private boolean tunnelForward;
    private Rect crop;
    private boolean control = true;
    private int displayId;
    private boolean showTouches;
    private boolean stayAwake;
    private List<CodecOption> codecOptions;
    private String encoderName;
    private boolean powerOffScreenOnClose;
    private boolean clipboardAutosync = true;
    private boolean downsizeOnError = true;
    private boolean cleanup = true;
    private boolean powerOn = true;

    // Options not used by the scrcpy client, but useful to use scrcpy-server directly
    private boolean sendDeviceMeta = true; // send device name and size
    private boolean sendFrameMeta = true; // send PTS so that the client may record properly
    private boolean sendDummyByte = true; // write a byte on start to detect connection issues
    private boolean sendCodecId = true; // write the codec ID (4 bytes) before the stream

    public Ln.Level getLogLevel() {
        return logLevel;
    }

    public void setLogLevel(Ln.Level logLevel) {
        this.logLevel = logLevel;
    }

    public int getScid() {
        return scid;
    }

    public void setScid(int scid) {
        this.scid = scid;
    }

    public boolean getAudio() {
        return audio;
    }

    public void setAudio(boolean audio) {
        this.audio = audio;
    }

    public int getMaxSize() {
        return maxSize;
    }

    public void setMaxSize(int maxSize) {
        this.maxSize = maxSize;
    }

    public VideoCodec getVideoCodec() {
        return videoCodec;
    }

    public void setVideoCodec(VideoCodec videoCodec) {
        this.videoCodec = videoCodec;
    }

    public int getBitRate() {
        return bitRate;
    }

    public void setBitRate(int bitRate) {
        this.bitRate = bitRate;
    }

    public int getMaxFps() {
        return maxFps;
    }

    public void setMaxFps(int maxFps) {
        this.maxFps = maxFps;
    }

    public int getLockVideoOrientation() {
        return lockVideoOrientation;
    }

    public void setLockVideoOrientation(int lockVideoOrientation) {
        this.lockVideoOrientation = lockVideoOrientation;
    }

    public boolean isTunnelForward() {
        return tunnelForward;
    }

    public void setTunnelForward(boolean tunnelForward) {
        this.tunnelForward = tunnelForward;
    }

    public Rect getCrop() {
        return crop;
    }

    public void setCrop(Rect crop) {
        this.crop = crop;
    }

    public boolean getControl() {
        return control;
    }

    public void setControl(boolean control) {
        this.control = control;
    }

    public int getDisplayId() {
        return displayId;
    }

    public void setDisplayId(int displayId) {
        this.displayId = displayId;
    }

    public boolean getShowTouches() {
        return showTouches;
    }

    public void setShowTouches(boolean showTouches) {
        this.showTouches = showTouches;
    }

    public boolean getStayAwake() {
        return stayAwake;
    }

    public void setStayAwake(boolean stayAwake) {
        this.stayAwake = stayAwake;
    }

    public List<CodecOption> getCodecOptions() {
        return codecOptions;
    }

    public void setCodecOptions(List<CodecOption> codecOptions) {
        this.codecOptions = codecOptions;
    }

    public String getEncoderName() {
        return encoderName;
    }

    public void setEncoderName(String encoderName) {
        this.encoderName = encoderName;
    }

    public void setPowerOffScreenOnClose(boolean powerOffScreenOnClose) {
        this.powerOffScreenOnClose = powerOffScreenOnClose;
    }

    public boolean getPowerOffScreenOnClose() {
        return this.powerOffScreenOnClose;
    }

    public boolean getClipboardAutosync() {
        return clipboardAutosync;
    }

    public void setClipboardAutosync(boolean clipboardAutosync) {
        this.clipboardAutosync = clipboardAutosync;
    }

    public boolean getDownsizeOnError() {
        return downsizeOnError;
    }

    public void setDownsizeOnError(boolean downsizeOnError) {
        this.downsizeOnError = downsizeOnError;
    }

    public boolean getCleanup() {
        return cleanup;
    }

    public void setCleanup(boolean cleanup) {
        this.cleanup = cleanup;
    }

    public boolean getPowerOn() {
        return powerOn;
    }

    public void setPowerOn(boolean powerOn) {
        this.powerOn = powerOn;
    }

    public boolean getSendDeviceMeta() {
        return sendDeviceMeta;
    }

    public void setSendDeviceMeta(boolean sendDeviceMeta) {
        this.sendDeviceMeta = sendDeviceMeta;
    }

    public boolean getSendFrameMeta() {
        return sendFrameMeta;
    }

    public void setSendFrameMeta(boolean sendFrameMeta) {
        this.sendFrameMeta = sendFrameMeta;
    }

    public boolean getSendDummyByte() {
        return sendDummyByte;
    }

    public void setSendDummyByte(boolean sendDummyByte) {
        this.sendDummyByte = sendDummyByte;
    }

    public boolean getSendCodecId() {
        return sendCodecId;
    }

    public void setSendCodecId(boolean sendCodecId) {
        this.sendCodecId = sendCodecId;
    }
}
