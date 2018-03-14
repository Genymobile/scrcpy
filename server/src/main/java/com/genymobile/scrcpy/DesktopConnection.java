package com.genymobile.scrcpy;

import android.net.LocalServerSocket;
import android.net.LocalSocket;
import android.net.LocalSocketAddress;

import java.io.Closeable;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;

public final class DesktopConnection implements Closeable {

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

    private static LocalSocket listenAndAccept(String abstractName) throws IOException {
        LocalServerSocket localServerSocket = new LocalServerSocket(abstractName);
        try {
            return localServerSocket.accept();
        } finally {
            localServerSocket.close();
        }
    }

    public static DesktopConnection open(Device device, boolean tunnelForward) throws IOException {
        LocalSocket socket;
        if (tunnelForward) {
            socket = listenAndAccept(SOCKET_NAME);
            // send one byte so the client may read() to detect a connection error
            socket.getOutputStream().write(0);
        } else {
            socket = connect(SOCKET_NAME);
        }

        DesktopConnection connection = new DesktopConnection(socket);
        Size videoSize = device.getScreenInfo().getVideoSize();
        connection.send(Device.getDeviceName(), videoSize.getWidth(), videoSize.getHeight());
        return connection;
    }

    public void close() throws IOException {
        socket.shutdownInput();
        socket.shutdownOutput();
        socket.close();
    }

    @SuppressWarnings("checkstyle:MagicNumber")
    private void send(String deviceName, int width, int height) throws IOException {
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

    public OutputStream getOutputStream() {
        return outputStream;
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
