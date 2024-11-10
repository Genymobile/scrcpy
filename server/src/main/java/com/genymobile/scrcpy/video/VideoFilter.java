package com.genymobile.scrcpy.video;

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

    public void addLockVideoOrientation(int lockVideoOrientation, int displayRotation) {
        int ccwRotation = (4 + lockVideoOrientation - displayRotation) % 4;
        addRotation(ccwRotation);
    }
}
