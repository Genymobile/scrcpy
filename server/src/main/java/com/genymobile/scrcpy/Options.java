package com.genymobile.scrcpy;

import android.graphics.Rect;

public class Options {
    private int maxSize;
    private int bitRate;
    private boolean tunnelForward;
    private Rect crop;

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
}
