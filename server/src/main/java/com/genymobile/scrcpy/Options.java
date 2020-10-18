package com.genymobile.scrcpy;

import android.graphics.Rect;

public class Options {
    private Ln.Level logLevel;
    private int maxSize;
    private int bitRate;
    private int maxFps;
    private int lockedVideoOrientation;
    private boolean tunnelForward;
    private Rect crop;
    private boolean sendFrameMeta; // send PTS so that the client may record properly
    private boolean control;
    private int displayId;
    private boolean showTouches;
    private boolean stayAwake;
    private String codecOptions;

    public Ln.Level getLogLevel() {
        return logLevel;
    }

    public void setLogLevel(Ln.Level logLevel) {
        this.logLevel = logLevel;
    }

    public int getMaxSize() {
        return maxSize;
    }

    public void setMaxSize(int maxSize) {
        this.maxSize = maxSize;
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

    public int getLockedVideoOrientation() {
        return lockedVideoOrientation;
    }

    public void setLockedVideoOrientation(int lockedVideoOrientation) {
        this.lockedVideoOrientation = lockedVideoOrientation;
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

    public boolean getSendFrameMeta() {
        return sendFrameMeta;
    }

    public void setSendFrameMeta(boolean sendFrameMeta) {
        this.sendFrameMeta = sendFrameMeta;
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

    public String getCodecOptions() {
        return codecOptions;
    }

    public void setCodecOptions(String codecOptions) {
        this.codecOptions = codecOptions;
    }
}
