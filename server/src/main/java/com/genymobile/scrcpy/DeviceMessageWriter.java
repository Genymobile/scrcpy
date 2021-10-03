package com.genymobile.scrcpy;

import java.io.IOException;
import java.io.OutputStream;

public class DeviceMessageWriter {

    private final byte[] rawBuffer = new byte[DeviceMessage.MAX_EVENT_SIZE];

    public void writeTo(DeviceMessage msg, OutputStream output) throws IOException {
        msg.writeToByteArray(rawBuffer);
        output.write(rawBuffer, 0, msg.getLen());
    }
}
