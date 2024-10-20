package com.genymobile.scrcpy.util;

import com.genymobile.scrcpy.device.Point;
import com.genymobile.scrcpy.device.Size;

/**
 * Represents a 2D affine transform (a 3x3 matrix):
 *
 * <pre>
 *     / a c e \
 *     | b d f |
 *     \ 0 0 1 /
 * </pre>
 * <p>
 * Or, a 4x4 matrix if we add a z axis:
 *
 * <pre>
 *     / a c 0 e \
 *     | b d 0 f |
 *     | 0 0 1 0 |
 *     \ 0 0 0 1 /
 * </pre>
 */
public class AffineMatrix {

    private final double a, b, c, d, e, f;

    /**
     * The identity matrix.
     */
    public static final AffineMatrix IDENTITY = new AffineMatrix(1, 0, 0, 1, 0, 0);

    /**
     * Create a new matrix:
     *
     * <pre>
     *     / a c e \
     *     | b d f |
     *     \ 0 0 1 /
     * </pre>
     */
    public AffineMatrix(double a, double b, double c, double d, double e, double f) {
        this.a = a;
        this.b = b;
        this.c = c;
        this.d = d;
        this.e = e;
        this.f = f;
    }

    @Override
    public String toString() {
        return "[" + a + ", " + c + ", " + e + "; " + b + ", " + d + ", " + f + "]";
    }

    /**
     * Return a matrix which converts from Normalized Device Coordinates to pixels.
     *
     * @param size the target size
     * @return the transform matrix
     */
    public static AffineMatrix ndcFromPixels(Size size) {
        double w = size.getWidth();
        double h = size.getHeight();
        return new AffineMatrix(1 / w, 0, 0, -1 / h, 0, 1);
    }

    /**
     * Return a matrix which converts from pixels to Normalized Device Coordinates.
     *
     * @param size the source size
     * @return the transform matrix
     */
    public static AffineMatrix ndcToPixels(Size size) {
        double w = size.getWidth();
        double h = size.getHeight();
        return new AffineMatrix(w, 0, 0, -h, 0, h);
    }

    /**
     * Apply the transform to a point ({@code this} should be a matrix converted to pixels coordinates via {@link #ndcToPixels(Size)}).
     *
     * @param point the source point
     * @return the converted point
     */
    public Point apply(Point point) {
        int x = point.getX();
        int y = point.getY();
        int xx = (int) (a * x + c * y + e);
        int yy = (int) (b * x + d * y + f);
        return new Point(xx, yy);
    }

    /**
     * Compute <code>this * rhs</code>.
     *
     * @param rhs the matrix to multiply
     * @return the product
     */
    public AffineMatrix multiply(AffineMatrix rhs) {
        if (rhs == null) {
            // For convenience
            return this;
        }

        double aa = this.a * rhs.a + this.c * rhs.b;
        double bb = this.b * rhs.a + this.d * rhs.b;
        double cc = this.a * rhs.c + this.c * rhs.d;
        double dd = this.b * rhs.c + this.d * rhs.d;
        double ee = this.a * rhs.e + this.c * rhs.f + this.e;
        double ff = this.b * rhs.e + this.d * rhs.f + this.f;
        return new AffineMatrix(aa, bb, cc, dd, ee, ff);
    }

    /**
     * Multiply all matrices from left to right, ignoring any {@code null} matrix (for convenience).
     *
     * @param matrices the matrices
     * @return the product
     */
    public static AffineMatrix multiplyAll(AffineMatrix... matrices) {
        AffineMatrix result = null;
        for (AffineMatrix matrix : matrices) {
            if (result == null) {
                result = matrix;
            } else {
                result = result.multiply(matrix);
            }
        }
        return result;
    }

    /**
     * Invert the matrix.
     *
     * @return the inverse matrix (or {@code null} if not invertible).
     */
    public AffineMatrix invert() {
        // The 3x3 matrix M can be decomposed into M = M1 * M2:
        //         M1          M2
        //      / 1 0 e \   / a c 0 \
        //      | 0 1 f | * | b d 0 |
        //      \ 0 0 1 /   \ 0 0 1 /
        //
        // The inverse of an invertible 2x2 matrix is given by this formula:
        //
        //      / A B \⁻¹     1   /  D -B \
        //      \ C D /   = ----- \ -C  A /
        //                  AD-BC
        //
        // Let B=c and C=b (to apply the general formula with the same letters).
        //
        //     M⁻¹ = (M1 * M2)⁻¹ = M2⁻¹ * M1⁻¹
        //
        //                  M2⁻¹              M1⁻¹
        //           /----------------\
        //             1   /  d -B  0 \   / 1  0 -e \
        //         = ----- | -C  a  0 | * | 0  1 -f |
        //           ad-BC \  0  0  1 /   \ 0  0  1 /
        //
        // With the original letters:
        //
        //             1   /  d -c  0 \   / 1  0 -e \
        //     M⁻¹ = ----- | -b  a  0 | * | 0  1 -f |
        //           ad-cb \  0  0  1 /   \ 0  0  1 /
        //
        //             1   /  d -c  cf-de \
        //         = ----- | -b  a  be-af |
        //           ad-cb \  0  0    1   /

        double det = a * d - c * b;
        if (det == 0) {
            // Not invertible
            return null;
        }

        double aa = d / det;
        double bb = -b / det;
        double cc = -c / det;
        double dd = a / det;
        double ee = (c * f - d * e) / det;
        double ff = (b * e - a * f) / det;

        return new AffineMatrix(aa, bb, cc, dd, ee, ff);
    }

    /**
     * Return this transform applied from the center (0.5, 0.5).
     *
     * @return the resulting matrix
     */
    public AffineMatrix fromCenter() {
        return translate(0.5, 0.5).multiply(this).multiply(translate(-0.5, -0.5));
    }

    /**
     * Return this transform with the specified aspect ratio.
     *
     * @param ar the aspect ratio
     * @return the resulting matrix
     */
    public AffineMatrix withAspectRatio(double ar) {
        return scale(1 / ar, 1).multiply(this).multiply(scale(ar, 1));
    }

    /**
     * Return this transform with the specified aspect ratio.
     *
     * @param size the size describing the aspect ratio
     * @return the transform
     */
    public AffineMatrix withAspectRatio(Size size) {
        double ar = (double) size.getWidth() / size.getHeight();
        return withAspectRatio(ar);
    }

    /**
     * Return a translation matrix.
     *
     * @param x the horizontal translation
     * @param y the vertical translation
     * @return the matrix
     */
    public static AffineMatrix translate(double x, double y) {
        return new AffineMatrix(1, 0, 0, 1, x, y);
    }

    /**
     * Return a scaling matrix.
     *
     * @param x the horizontal scaling
     * @param y the vertical scaling
     * @return the matrix
     */
    public static AffineMatrix scale(double x, double y) {
        return new AffineMatrix(x, 0, 0, y, 0, 0);
    }

    /**
     * Return a scaling matrix.
     *
     * @param from the source size
     * @param to   the destination size
     * @return the matrix
     */
    public static AffineMatrix scale(Size from, Size to) {
        double scaleX = (double) to.getWidth() / from.getWidth();
        double scaleY = (double) to.getHeight() / from.getHeight();
        return scale(scaleX, scaleY);
    }

    /**
     * Return a matrix applying a "reframing" (cropping a rectangle).
     * <p/>
     * <code>(x, y)</code> is the bottom-left corner, <code>(w, h)</code> is the size of the rectangle.
     *
     * @param x horizontal coordinate (increasing to the right)
     * @param y vertical coordinate (increasing upwards)
     * @param w width
     * @param h height
     * @return the matrix
     */
    public static AffineMatrix reframe(double x, double y, double w, double h) {
        if (w == 0 || h == 0) {
            throw new IllegalArgumentException("Cannot reframe to an empty area: " + w + "x" + h);
        }
        return scale(1 / w, 1 / h).multiply(translate(-x, -y));
    }

    /**
     * Return an orthogonal rotation matrix.
     *
     * @param ccwRotation the counter-clockwise rotation
     * @return the matrix
     */
    public static AffineMatrix rotateOrtho(int ccwRotation) {
        switch (ccwRotation) {
            case 0:
                return IDENTITY;
            case 1:
                // 90° counter-clockwise
                return new AffineMatrix(0, 1, -1, 0, 1, 0);
            case 2:
                // 180°
                return new AffineMatrix(-1, 0, 0, -1, 1, 1);
            case 3:
                // 90° clockwise
                return new AffineMatrix(0, -1, 1, 0, 0, 1);
            default:
                throw new IllegalArgumentException("Invalid rotation: " + ccwRotation);
        }
    }

    /**
     * Return an horizontal flip matrix.
     *
     * @return the matrix
     */
    public static AffineMatrix hflip() {
        return new AffineMatrix(-1, 0, 0, 1, 1, 0);
    }

    /**
     * Return a vertical flip matrix.
     *
     * @return the matrix
     */
    public static AffineMatrix vflip() {
        return new AffineMatrix(1, 0, 0, -1, 0, 1);
    }

    /**
     * Return a rotation matrix.
     *
     * @param ccwDegrees the angle, in degrees (counter-clockwise)
     * @return the matrix
     */
    public static AffineMatrix rotate(double ccwDegrees) {
        double radians = Math.toRadians(ccwDegrees);
        double cos = Math.cos(radians);
        double sin = Math.sin(radians);
        return new AffineMatrix(cos, sin, -sin, cos, 0, 0);
    }

    /**
     * Export this affine transform to a 4x4 column-major order matrix.
     *
     * @param matrix output 4x4 matrix
     */
    public void to4x4(float[] matrix) {
        // matrix is a 4x4 matrix in column-major order

        // Column 0
        matrix[0] = (float) a;
        matrix[1] = (float) b;
        matrix[2] = 0;
        matrix[3] = 0;

        // Column 1
        matrix[4] = (float) c;
        matrix[5] = (float) d;
        matrix[6] = 0;
        matrix[7] = 0;

        // Column 2
        matrix[8] = 0;
        matrix[9] = 0;
        matrix[10] = 1;
        matrix[11] = 0;

        // Column 3
        matrix[12] = (float) e;
        matrix[13] = (float) f;
        matrix[14] = 0;
        matrix[15] = 1;
    }

    /**
     * Export this affine transform to a 4x4 column-major order matrix.
     *
     * @return 4x4 matrix
     */
    public float[] to4x4() {
        float[] matrix = new float[16];
        to4x4(matrix);
        return matrix;
    }
}
