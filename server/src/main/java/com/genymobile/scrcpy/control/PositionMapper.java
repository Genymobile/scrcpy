package com.genymobile.scrcpy.control;

import com.genymobile.scrcpy.model.Point;
import com.genymobile.scrcpy.model.Position;
import com.genymobile.scrcpy.model.Size;
import com.genymobile.scrcpy.util.AffineMatrix;

public final class PositionMapper {

    private final Size videoSize;
    private final AffineMatrix videoToDeviceMatrix;
    private final AffineMatrix deviceToVideoMatrix;

    public PositionMapper(Size videoSize, AffineMatrix videoToDeviceMatrix) {
        this.videoSize = videoSize;
        this.videoToDeviceMatrix = videoToDeviceMatrix;
        deviceToVideoMatrix = videoToDeviceMatrix != null ? videoToDeviceMatrix.invert() : null;
    }

    public static PositionMapper create(Size videoSize, AffineMatrix filterTransform, Size targetSize) {
        boolean convertToPixels = !videoSize.equals(targetSize) || filterTransform != null;
        AffineMatrix transform = filterTransform;
        if (convertToPixels) {
            AffineMatrix inputTransform = AffineMatrix.ndcFromPixels(videoSize);
            AffineMatrix outputTransform = AffineMatrix.ndcToPixels(targetSize);
            transform = outputTransform.multiply(transform).multiply(inputTransform);
        }

        return new PositionMapper(videoSize, transform);
    }

    public Size getVideoSize() {
        return videoSize;
    }

    public Point map(Position position) {
        Size clientVideoSize = position.getScreenSize();
        if (!videoSize.equals(clientVideoSize)) {
            // The client sends a click relative to a video with wrong dimensions,
            // the device may have been rotated since the event was generated, so ignore the event
            return null;
        }

        Point point = position.getPoint();
        if (videoToDeviceMatrix != null) {
            point = videoToDeviceMatrix.apply(point);
        }
        return point;
    }

    public Point unmap(Point point) {
        if (videoToDeviceMatrix != null) {
            if (deviceToVideoMatrix == null) {
                return null;
            }
            point = deviceToVideoMatrix.apply(point);
        }
        if (point.getX() < 0 || point.getY() < 0 || point.getX() > videoSize.getWidth() || point.getY() > videoSize.getHeight()) {
            return null;
        }
        return point;
    }
}
