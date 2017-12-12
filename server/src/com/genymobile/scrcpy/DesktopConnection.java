package com.genymobile.scrcpy;

import android.net.LocalSocket;
import android.net.LocalSocketAddress;

import java.io.Closeable;
import java.io.IOException;

public class DesktopConnection implements Closeable {

    private static final String SOCKET_NAME = "scrcpy";

    private final LocalSocket socket;

    private DesktopConnection(LocalSocket socket) throws IOException {
        this.socket = socket;
    }

    private static LocalSocket connect(String abstractName) throws IOException {
        LocalSocket localSocket = new LocalSocket();
        localSocket.connect(new LocalSocketAddress(abstractName));
        return localSocket;
    }

    public static DesktopConnection open(int width, int height) throws IOException {
        LocalSocket socket = connect(SOCKET_NAME);
        send(socket, width, height);
        return new DesktopConnection(socket);
    }

    public void close() throws IOException {
        socket.shutdownInput();
        socket.shutdownOutput();
        socket.close();
    }

    private static void send(LocalSocket socket, int width, int height) throws IOException {
        assert width < 0x10000 : "width may not be stored on 16 bits";
        assert height < 0x10000 : "height may not be stored on 16 bits";
        byte[] buffer = new byte[4];
        buffer[0] = (byte) (width >> 8);
        buffer[1] = (byte) width;
        buffer[2] = (byte) (height >> 8);
        buffer[3] = (byte) height;
        socket.getOutputStream().write(buffer, 0, 4);
    }

    public void sendVideoStream(byte[] videoStreamBuffer, int len) throws IOException {
        socket.getOutputStream().write(videoStreamBuffer, 0, len);
    }
}

