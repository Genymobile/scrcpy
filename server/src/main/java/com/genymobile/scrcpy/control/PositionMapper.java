package com.genymobile.scrcpy.control;

import com.genymobile.scrcpy.device.Point;
import com.genymobile.scrcpy.device.Position;
import com.genymobile.scrcpy.device.Size;

public final class PositionMapper {

    private final Size sourceSize;
    private final Size videoSize;

    public PositionMapper(Size sourceSize, Size videoSize) {
        this.sourceSize = sourceSize;
        this.videoSize = videoSize;
    }

    public Point map(Position position) {
        Size clientVideoSize = position.getScreenSize();
        if (!videoSize.equals(clientVideoSize)) {
            // The client sends a click relative to a video with wrong dimensions,
            // the device may have been rotated since the event was generated, so ignore the event
            return null;
        }

        Point point = position.getPoint();
        int convertedX = point.getX() * sourceSize.getWidth() / videoSize.getWidth();
        int convertedY = point.getY() * sourceSize.getHeight() / videoSize.getHeight();
        return new Point(convertedX, convertedY);
    }
}
