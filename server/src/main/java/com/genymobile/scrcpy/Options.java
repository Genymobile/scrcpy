package com.genymobile.scrcpy;

import android.graphics.Rect;

public class Options {
    private int maxSize;
    private int bitRate;
    private int maxFps;
    private boolean tunnelForward;
    private Rect crop;
    private boolean sendFrameMeta; // send PTS so that the client may record properly
    private boolean control;

    //wen add
    private int quality;
    private int scale;
    private boolean controlOnly;
    private boolean nalu;
    private boolean dumpHierarchy;

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

    public int getQuality() {
        return quality;
    }

    public void setQuality(int quality) {
        this.quality = quality;
    }

    public int getScale() {
        return scale;
    }

    public void setScale(int scale) {
        this.scale = scale;
    }

    public boolean getControlOnly() {
        return controlOnly;
    }

    public void setControlOnly(boolean controlOnly) {
        this.controlOnly = controlOnly;
    }

    public boolean getNALU() {
        return nalu;
    }

    public void setNALU(boolean nalu) {
        this.nalu = nalu;
    }

    public boolean getDumpHierarchy() {
        return dumpHierarchy;
    }

    public void setDumpHierarchy(boolean dumpHierarchy) {
        this.dumpHierarchy = dumpHierarchy;
    }
}
