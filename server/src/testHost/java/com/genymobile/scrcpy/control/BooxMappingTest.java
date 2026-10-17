package com.genymobile.scrcpy.control;

import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;

/** Read-only, captured InputReader metadata; never opens evdev. */
public final class BooxMappingTest {
    private static int checks;
    private static void check(boolean value, String message) {
        ++checks;
        if (!value) {
            throw new AssertionError(message);
        }
    }
    private static void reject(String text, String label) {
        try {
            BooxMapping.parse(text, "/dev/input/event2", 0, 20966, 0, 15725);
            throw new AssertionError("accepted " + label);
        } catch (IllegalArgumentException expected) {
            ++checks;
        }
    }
    public static void main(String[] args) throws Exception {
        String text = new String(Files.readAllBytes(Paths.get(args[0])), StandardCharsets.UTF_8);
        BooxMapping mapping = BooxMapping.parse(text, "/dev/input/event2", 0, 20966, 0, 15725);
        check(mapping.rotation == 3, "live orientation1 gives inverse270");
        check(mapping.width == 1404 && mapping.height == 1872, "logical screen dimensions");
        // Real NoteAir: InputReader orientation=1 while logical DisplayInfo.rotation=0.
        // These are distinct coordinate systems; only logical dimensions must match.
        mapping.requireDisplaySize(1404, 1872);
        ++checks;
        for (int[] size : new int[][] {{1872, 1404}, {1403, 1872}, {1404, 1871}, {0, 0}}) {
            try {
                mapping.requireDisplaySize(size[0], size[1]);
                throw new AssertionError("accepted wrong logical display size");
            } catch (IllegalArgumentException expected) {
                ++checks;
            }
        }
        for (int orientation = 0; orientation < 4; ++orientation) {
            String candidate = text.replace("orientation=1", "orientation=" + orientation)
                    .replace("SurfaceOrientation: 1", "SurfaceOrientation: " + orientation);
            if (orientation % 2 == 0) {
                candidate = candidate.replace("SurfaceWidth: 1872px", "SurfaceWidth: 1404px")
                        .replace("SurfaceHeight: 1404px", "SurfaceHeight: 1872px")
                        .replace("PhysicalWidth: 1872px", "PhysicalWidth: 1404px")
                        .replace("PhysicalHeight: 1404px", "PhysicalHeight: 1872px");
            }
            mapping = BooxMapping.parse(candidate, "/dev/input/event2", 0, 20966, 0, 15725);
            check(mapping.rotation == (4 - orientation) % 4, "inverse orientation " + orientation);
        }
        reject(text.replace("/dev/input/event2", "/dev/input/event7"), "wrong event path");
        reject(text.replace("Device 9:", "Device 8:"), "wrong reader id");
        reject(text.replace("X scale: 1.000", "X scale: 1.100"), "nonidentity affine");
        reject(text.replace("X ymix: 0.000", "X ymix: 0.010"), "affine mixing");
        reject(text.replace("X offset: 0.000", "X offset: 2.000"), "affine offset");
        reject(text.replace("OrientationAware: true", "OrientationAware: false"), "fixed axes");
        reject(text.replace("SurfaceOrientation: 1", "SurfaceOrientation: 2"), "orientation mismatch");
        reject(text.replace("max=20966", "max=20965"), "raw range mismatch");
        reject(text.replace("physicalFrame=[0, 0, 1404, 1872]", "physicalFrame=[0, 0, 1400, 1872]"), "viewport scaling");
        reject(text.replace("displayId=0,", "displayId=1,"), "other display");
        reject(text.replace("SurfaceLeft: 0", "SurfaceLeft: 1"), "surface offset");
        reject(text.replace("SurfaceWidth: 1872px", "SurfaceWidth: 1800px"), "surface mismatch");
        reject(text.replace("X scale: 1.000", "X scale: NaN"), "invalid affine number");
        reject(text + text, "duplicate device metadata");
        reject("", "empty metadata");
        System.out.println("BooxMapping: " + checks + " behavioral checks passed");
    }
}
