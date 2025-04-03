package com.genymobile.scrcpy;

import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;

import com.genymobile.scrcpy.util.Ln;

import java.io.File;
import java.io.FileOutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.HashMap;
import java.util.Map;

public class IcoFileCreator {
    public static void createIcoFile(String packageName) {
        try {
            PackageManager pm = FakeContext.get().getPackageManager();
            Drawable appIcon = pm.getApplicationIcon(packageName);

            // <https://stackoverflow.com/posts/74241508/revisions>
            int[] sizes = {256, 128, 64, 48, 40, 32, 24, 20, 16};
            byte[] icoData = createIcoData(appIcon, sizes);

            File outputDir = new File("/data/local/tmp/ico/");
            if (!outputDir.exists()) {
                outputDir.mkdirs();
            }
            File outputFile = new File(outputDir, packageName + ".ico");
            FileOutputStream fos = new FileOutputStream(outputFile);
            fos.write(icoData);
            fos.close();
            Ln.d("ICO file saved to: " + outputFile.getAbsolutePath());
        } catch (Exception e) {
            Ln.e("Error creating ICO file for package: "+packageName, e);
        }
    }

    private static byte[] createIcoData(Drawable drawable, int[] sizes) {
        int numImages = sizes.length;
        int totalImageDataSize = calculateImageDataSize(sizes);

        ByteBuffer buffer = ByteBuffer.allocate(6 + numImages * 16 + totalImageDataSize);
        buffer.order(ByteOrder.LITTLE_ENDIAN);
        buffer.putShort((short) 0); // Reserved
        buffer.putShort((short) 1); // ICO type
        buffer.putShort((short) numImages); // Number of images

        int imageDataOffset = 6 + numImages * 16;

        Map<Integer, byte[]> cachedImages = new HashMap<>();
        for (int size : sizes) {
            Bitmap bitmap = drawableToBitmap(drawable, size, size);

            byte[] imageData = bitmapToIcoImageData(bitmap);
            cachedImages.put(size, imageData);

            buffer.put((byte) size); // Width
            buffer.put((byte) size); // Height
            buffer.put((byte) 0); // Number of colors in palette
            buffer.put((byte) 0); // Reserved
            buffer.putShort((short) 1); // Color planes
            buffer.putShort((short) 32); // Bits per pixel
            buffer.putInt(imageData.length); // Image size
            buffer.putInt(imageDataOffset); // Offset to image data

            imageDataOffset += imageData.length;
            bitmap.recycle();
        }

        for (int size : sizes) {
            buffer.put(cachedImages.get(size));
        }

        return buffer.array();
    }

    private static int calculateImageDataSize(int[] sizes) {
        int totalSize = 0;
        for (int size : sizes) {
            int andMaskSize = ((size + 7) / 8) * size; // AND mask size
            totalSize += size * size * 4 + 40 + andMaskSize; // Pixel data + header + AND mask
        }
        return totalSize;
    }

    private static Bitmap drawableToBitmap(Drawable drawable, int width, int height) {
        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        drawable.setBounds(0, 0, width, height);
        drawable.draw(canvas);
        return bitmap;
    }

    private static byte[] bitmapToIcoImageData(Bitmap bitmap) {
        int width = bitmap.getWidth();
        int height = bitmap.getHeight();

        int andMaskRowSize = (width + 7) / 8;
        int andMaskSize = andMaskRowSize * height;

        int bufferSize = width * height * 4 + 40 + andMaskSize;

        ByteBuffer buffer = ByteBuffer.allocate(bufferSize);
        buffer.order(ByteOrder.LITTLE_ENDIAN);

        // BITMAPINFOHEADER
        buffer.putInt(40); // Header size
        buffer.putInt(width); // Width
        buffer.putInt(height * 2); // Height (times 2 for AND mask)
        buffer.putShort((short) 1); // Color planes
        buffer.putShort((short) 32); // Bits per pixel
        buffer.putInt(0); // Compression
        buffer.putInt(width * height * 4); // Image size
        buffer.putInt(0); // Horizontal resolution
        buffer.putInt(0); // Vertical resolution
        buffer.putInt(0); // Colors in color table
        buffer.putInt(0); // Important color count

        // Extract pixel data
        int[] pixels = new int[width * height];
        bitmap.getPixels(pixels, 0, width, 0, 0, width, height);

        // Write pixel data (ARGB)
        for (int y = height - 1; y >= 0; y--) {
            for (int x = 0; x < width; x++) {
                int pixel = pixels[y * width + x];
                buffer.put((byte) (pixel & 0xFF)); // Blue
                buffer.put((byte) ((pixel >> 8) & 0xFF)); // Green
                buffer.put((byte) ((pixel >> 16) & 0xFF)); // Red
                buffer.put((byte) ((pixel >> 24) & 0xFF)); // Alpha
            }
        }

        // Write the AND mask (all zeros for no transparency mask)
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < andMaskRowSize; x++) {
                buffer.put((byte) 0); // Write zeroes for the transparency mask
            }
        }
        return buffer.array();
    }
}
