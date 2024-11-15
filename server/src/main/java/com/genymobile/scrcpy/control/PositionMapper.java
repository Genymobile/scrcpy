package com.genymobile.scrcpy.control;

import com.genymobile.scrcpy.device.Point;
import com.genymobile.scrcpy.device.Position;
import com.genymobile.scrcpy.device.Size;
import com.genymobile.scrcpy.util.AffineMatrix;

public final class PositionMapper {

    private final Size videoSize;
    private final AffineMatrix videoToDeviceMatrix;

    public PositionMapper(Size videoSize, AffineMatrix videoToDeviceMatrix) {
        this.videoSize = videoSize;
        this.videoToDeviceMatrix = videoToDeviceMatrix;
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
}
