package com.genymobile.scrcpy.video;

import android.media.MediaCodec;
import android.os.Bundle;

public class CaptureIdr {

    // Current instance of MediaCodec to "interrupt" on reset
    private MediaCodec runningMediaCodec;

    public synchronized void requestIdr() {
        Bundle bundle = new Bundle();
        bundle.putInt(MediaCodec.PARAMETER_KEY_REQUEST_SYNC_FRAME, 0);
        this.runningMediaCodec.setParameters(bundle);
    }

    public synchronized void setRunningMediaCodec(MediaCodec runningMediaCodec) {
        this.runningMediaCodec = runningMediaCodec;
    }

}
