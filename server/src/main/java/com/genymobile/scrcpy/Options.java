package com.genymobile.scrcpy;

import android.graphics.Rect;

import java.util.List;

public class Options {

    private Ln.Level logLevel = Ln.Level.DEBUG;
    private int scid = -1; // 31-bit non-negative value, or -1
    private boolean audio = true;
    private int maxSize;
    private VideoCodec videoCodec = VideoCodec.H264;
    private AudioCodec audioCodec = AudioCodec.OPUS;
    private int videoBitRate = 8000000;
    private int audioBitRate = 128000;
    private int maxFps;
    private int lockVideoOrientation = -1;
    private boolean tunnelForward;
    private Rect crop;
    private boolean control = true;
    private int displayId;
    private boolean showTouches;
    private boolean stayAwake;
    private List<CodecOption> videoCodecOptions;
    private List<CodecOption> audioCodecOptions;

    private String videoEncoder;
    private String audioEncoder;
    private boolean powerOffScreenOnClose;
    private boolean clipboardAutosync = true;
    private boolean downsizeOnError = true;
    private boolean cleanup = true;
    private boolean powerOn = true;

    private boolean listEncoders;
    private boolean listDisplays;

    // Options not used by the scrcpy client, but useful to use scrcpy-server directly
    private boolean sendDeviceMeta = true; // send device name and size
    private boolean sendFrameMeta = true; // send PTS so that the client may record properly
    private boolean sendDummyByte = true; // write a byte on start to detect connection issues
    private boolean sendCodecMeta = true; // write the codec metadata before the stream

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

    public AudioCodec getAudioCodec() {
        return audioCodec;
    }

    public void setAudioCodec(AudioCodec audioCodec) {
        this.audioCodec = audioCodec;
    }

    public int getVideoBitRate() {
        return videoBitRate;
    }

    public void setVideoBitRate(int videoBitRate) {
        this.videoBitRate = videoBitRate;
    }

    public int getAudioBitRate() {
        return audioBitRate;
    }

    public void setAudioBitRate(int audioBitRate) {
        this.audioBitRate = audioBitRate;
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

    public List<CodecOption> getVideoCodecOptions() {
        return videoCodecOptions;
    }

    public void setVideoCodecOptions(List<CodecOption> videoCodecOptions) {
        this.videoCodecOptions = videoCodecOptions;
    }

    public List<CodecOption> getAudioCodecOptions() {
        return audioCodecOptions;
    }

    public void setAudioCodecOptions(List<CodecOption> audioCodecOptions) {
        this.audioCodecOptions = audioCodecOptions;
    }

    public String getVideoEncoder() {
        return videoEncoder;
    }

    public void setVideoEncoder(String videoEncoder) {
        this.videoEncoder = videoEncoder;
    }

    public String getAudioEncoder() {
        return audioEncoder;
    }

    public void setAudioEncoder(String audioEncoder) {
        this.audioEncoder = audioEncoder;
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

    public boolean getListEncoders() {
        return listEncoders;
    }

    public void setListEncoders(boolean listEncoders) {
        this.listEncoders = listEncoders;
    }

    public boolean getListDisplays() {
        return listDisplays;
    }

    public void setListDisplays(boolean listDisplays) {
        this.listDisplays = listDisplays;
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

    public boolean getSendCodecMeta() {
        return sendCodecMeta;
    }

    public void setSendCodecMeta(boolean sendCodecMeta) {
        this.sendCodecMeta = sendCodecMeta;
    }
}
