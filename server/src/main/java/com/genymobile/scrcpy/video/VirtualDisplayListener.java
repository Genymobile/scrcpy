package com.genymobile.scrcpy.video;

import com.genymobile.scrcpy.control.PositionMapper;

public interface VirtualDisplayListener {
    void onNewVirtualDisplay(int displayId, PositionMapper positionMapper);
}
