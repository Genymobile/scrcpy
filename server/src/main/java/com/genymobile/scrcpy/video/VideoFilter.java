package com.genymobile.scrcpy.video;

import com.genymobile.scrcpy.device.Orientation;
import com.genymobile.scrcpy.device.Size;
import com.genymobile.scrcpy.util.AffineMatrix;

import android.graphics.Rect;

public class VideoFilter {

    private Size size;
    private AffineMatrix transform;

    public VideoFilter(Size inputSize) {
        this.size = inputSize;
    }

    public Size getOutputSize() {
        return size;
    }

    public AffineMatrix getTransform() {
        return transform;
    }

    /**
     * Return the inverse transform.
     * <p/>
     * The direct affine transform describes how the input image is transformed.
     * <p/>
     * It is often useful to retrieve the inverse transform instead:
     * <ul>
     *     <li>The OpenGL filter expects the matrix to transform the image <em>coordinates</em>, which is the inverse transform;</li>
     *     <li>The click positions must be transformed back to the device positions, using the inverse transform too.</li>
     * </ul>
     *
     * @return the inverse transform
     */
    public AffineMatrix getInverseTransform() {
        if (transform == null) {
            return null;
        }
        return transform.invert();
    }

    private static Rect transposeRect(Rect rect) {
        return new Rect(rect.top, rect.left, rect.bottom, rect.right);
    }

    public void addCrop(Rect crop, boolean transposed) {
        if (transposed) {
            crop = transposeRect(crop);
        }

        double inputWidth = size.getWidth();
        double inputHeight = size.getHeight();

        if (crop.left < 0 || crop.top < 0 || crop.right > inputWidth || crop.bottom > inputHeight) {
            throw new IllegalArgumentException("Crop " + crop + " exceeds the input area (" + size + ")");
        }

        double x = crop.left / inputWidth;
        double y = 1 - (crop.bottom / inputHeight); // OpenGL origin is bottom-left
        double w = crop.width() / inputWidth;
        double h = crop.height() / inputHeight;

        transform = AffineMatrix.reframe(x, y, w, h).multiply(transform);
        size = new Size(crop.width(), crop.height());
    }

    public void addRotation(int ccwRotation) {
        if (ccwRotation == 0) {
            return;
        }

        transform = AffineMatrix.rotateOrtho(ccwRotation).multiply(transform);
        if (ccwRotation % 2 != 0) {
            size = size.rotate();
        }
    }

    public void addOrientation(Orientation captureOrientation) {
        if (captureOrientation.isFlipped()) {
            transform = AffineMatrix.hflip().multiply(transform);
        }
        int ccwRotation = (4 - captureOrientation.getRotation()) % 4;
        addRotation(ccwRotation);
    }

    public void addOrientation(int displayRotation, boolean locked, Orientation captureOrientation) {
        if (locked) {
            // flip/rotate the current display from the natural device orientation (i.e. where display rotation is 0)
            int reverseDisplayRotation = (4 - displayRotation) % 4;
            addRotation(reverseDisplayRotation);
        }
        addOrientation(captureOrientation);
    }

    public void addAngle(double cwAngle) {
        if (cwAngle == 0) {
            return;
        }
        double ccwAngle = -cwAngle;
        transform = AffineMatrix.rotate(ccwAngle).withAspectRatio(size).fromCenter().multiply(transform);
    }

    public void addResize(Size targetSize) {
        if (size.equals(targetSize)) {
            return;
        }

        if (transform == null) {
            // The requested scaling is performed by the viewport (by changing the output size), but the OpenGL filter must still run, even if
            // resizing is not performed by the shader. So transform MUST NOT be null.
            transform = AffineMatrix.IDENTITY;
        }
        size = targetSize;
    }
}
