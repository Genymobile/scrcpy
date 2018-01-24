package com.genymobile.scrcpy;

import android.net.LocalSocket;
import android.net.LocalSocketAddress;

import java.io.Closeable;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;

public class DesktopConnection implements Closeable {

    private static final int DEVICE_NAME_FIELD_LENGTH = 64;

    private static final String SOCKET_NAME = "scrcpy";

    private final LocalSocket socket;
    private final InputStream inputStream;
    private final OutputStream outputStream;

    private final ControlEventReader reader = new ControlEventReader();

    private DesktopConnection(LocalSocket socket) throws IOException {
        this.socket = socket;
        inputStream = socket.getInputStream();
        outputStream = socket.getOutputStream();
    }

    private static LocalSocket connect(String abstractName) throws IOException {
        LocalSocket localSocket = new LocalSocket();
        localSocket.connect(new LocalSocketAddress(abstractName));
        return localSocket;
    }

    public static DesktopConnection open(Device device) throws IOException {
        LocalSocket socket = connect(SOCKET_NAME);

        ScreenInfo initialScreenInfo = device.getScreenInfo();
        int width = initialScreenInfo.getLogicalWidth();
        int height = initialScreenInfo.getLogicalHeight();

        DesktopConnection connection = new DesktopConnection(socket);
        connection.send(Device.getDeviceName(), width, height);
        return connection;
    }

    public void close() throws IOException {
        socket.shutdownInput();
        socket.shutdownOutput();
        socket.close();
    }

    private void send(String deviceName, int width, int height) throws IOException {
        assert width < 0x10000 : "width may not be stored on 16 bits";
        assert height < 0x10000 : "height may not be stored on 16 bits";
        byte[] buffer = new byte[DEVICE_NAME_FIELD_LENGTH + 4];

        byte[] deviceNameBytes = deviceName.getBytes(StandardCharsets.UTF_8);
        int len = Math.min(DEVICE_NAME_FIELD_LENGTH - 1, deviceNameBytes.length);
        System.arraycopy(deviceNameBytes, 0, buffer, 0, len);
        // byte[] are always 0-initialized in java, no need to set '\0' explicitly

        buffer[DEVICE_NAME_FIELD_LENGTH] = (byte) (width >> 8);
        buffer[DEVICE_NAME_FIELD_LENGTH + 1] = (byte) width;
        buffer[DEVICE_NAME_FIELD_LENGTH + 2] = (byte) (height >> 8);
        buffer[DEVICE_NAME_FIELD_LENGTH + 3] = (byte) height;
        outputStream.write(buffer, 0, buffer.length);
    }

    public void sendVideoStream(byte[] videoStreamBuffer, int len) throws IOException {
        outputStream.write(videoStreamBuffer, 0, len);
    }

    public ControlEvent receiveControlEvent() throws IOException {
        ControlEvent event = reader.next();
        while (event == null) {
            reader.readFrom(inputStream);
            event = reader.next();
        }
        return event;
    }
}

