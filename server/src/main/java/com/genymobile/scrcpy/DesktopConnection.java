package com.genymobile.scrcpy;

import android.net.LocalServerSocket;
import android.net.LocalSocket;
import android.net.LocalSocketAddress;

import java.io.Closeable;
import java.io.FileDescriptor;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;

public final class DesktopConnection implements Closeable {

    private static final int DEVICE_NAME_FIELD_LENGTH = 64;

    private static final String SOCKET_NAME = "scrcpy";

    private final LocalSocket videoSocket;
    private final FileDescriptor videoFd;

    private final LocalSocket controlSocket;
    private final InputStream controlInputStream;
    private final OutputStream controlOutputStream;

    private final ControlMessageReader reader = new ControlMessageReader();
    private final DeviceMessageWriter writer = new DeviceMessageWriter();

    private DesktopConnection(LocalSocket videoSocket, LocalSocket controlSocket) throws IOException {
        this.videoSocket = videoSocket;
        this.controlSocket = controlSocket;
        if (controlSocket != null) {
            controlInputStream = controlSocket.getInputStream();
            controlOutputStream = controlSocket.getOutputStream();
        } else {
            controlInputStream = null;
            controlOutputStream = null;
        }
        videoFd = videoSocket.getFileDescriptor();
    }

    private static LocalSocket connect(String abstractName) throws IOException {
        LocalSocket localSocket = new LocalSocket();
        localSocket.connect(new LocalSocketAddress(abstractName));
        return localSocket;
    }

    public static DesktopConnection open(boolean tunnelForward, boolean control, boolean sendDummyByte) throws IOException {
        LocalSocket videoSocket;
        LocalSocket controlSocket = null;
        if (tunnelForward) {
            LocalServerSocket localServerSocket = new LocalServerSocket(SOCKET_NAME);
            try {
                videoSocket = localServerSocket.accept();
                if (sendDummyByte) {
                    // send one byte so the client may read() to detect a connection error
                    videoSocket.getOutputStream().write(0);
                }
                if (control) {
                    try {
                        controlSocket = localServerSocket.accept();
                    } catch (IOException | RuntimeException e) {
                        videoSocket.close();
                        throw e;
                    }
                }
            } finally {
                localServerSocket.close();
            }
        } else {
            videoSocket = connect(SOCKET_NAME);
            if (control) {
                try {
                    controlSocket = connect(SOCKET_NAME);
                } catch (IOException | RuntimeException e) {
                    videoSocket.close();
                    throw e;
                }
            }
        }

        return new DesktopConnection(videoSocket, controlSocket);
    }

    public void close() throws IOException {
        videoSocket.shutdownInput();
        videoSocket.shutdownOutput();
        videoSocket.close();
        if (controlSocket != null) {
            controlSocket.shutdownInput();
            controlSocket.shutdownOutput();
            controlSocket.close();
        }
    }

    public void sendDeviceMeta(String deviceName, int width, int height) throws IOException {
        byte[] buffer = new byte[DEVICE_NAME_FIELD_LENGTH + 4];

        byte[] deviceNameBytes = deviceName.getBytes(StandardCharsets.UTF_8);
        int len = StringUtils.getUtf8TruncationIndex(deviceNameBytes, DEVICE_NAME_FIELD_LENGTH - 1);
        System.arraycopy(deviceNameBytes, 0, buffer, 0, len);
        // byte[] are always 0-initialized in java, no need to set '\0' explicitly

        buffer[DEVICE_NAME_FIELD_LENGTH] = (byte) (width >> 8);
        buffer[DEVICE_NAME_FIELD_LENGTH + 1] = (byte) width;
        buffer[DEVICE_NAME_FIELD_LENGTH + 2] = (byte) (height >> 8);
        buffer[DEVICE_NAME_FIELD_LENGTH + 3] = (byte) height;
        IO.writeFully(videoFd, buffer, 0, buffer.length);
    }

    public FileDescriptor getVideoFd() {
        return videoFd;
    }

    public ControlMessage receiveControlMessage() throws IOException {
        ControlMessage msg = reader.next();
        while (msg == null) {
            reader.readFrom(controlInputStream);
            msg = reader.next();
        }
        return msg;
    }

    public void sendDeviceMessage(DeviceMessage msg) throws IOException {
        writer.writeTo(msg, controlOutputStream);
    }
}
