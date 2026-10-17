package com.genymobile.scrcpy.control;

import java.io.IOException;

/** Complete discovery before opening evdev. A null result means a valid bound device became active. */
final class BooxPenPreparation {
    final BooxDevice device;
    final int rotation;
    final BooxMapping mapping;

    private BooxPenPreparation(BooxDevice device, int rotation, BooxMapping mapping) {
        this.device = device;
        this.rotation = rotation;
        this.mapping = mapping;
    }

    static BooxPenPreparation prepare(BooxPenInitialization.Probe probe, BooxPenInitialization.Probe metadata,
            int rotation, int width, int height, String[] expected) throws IOException, InterruptedException {
        try {
            BooxDevice device = BooxDevice.parse(probe.read());
            device.requireIdentity(expected);
            BooxMapping mapping = null;
            if (rotation == -2) {
                mapping = BooxMapping.parse(metadata.read(), device.path, device.minX, device.maxX, device.minY, device.maxY);
                mapping.requireDisplaySize(width, height);
                rotation = mapping.rotation;
                device = BooxDevice.parse(probe.read());
                device.requireIdentity(expected);
            }
            return new BooxPenPreparation(device, rotation, mapping);
        } catch (BooxDevice.NotIdleException e) {
            e.device.requireIdentity(expected);
            return null; // Valid physical activity before open: retry, never treat it as idle.
        }
    }
}
