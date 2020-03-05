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

import java.net.Socket;
import java.net.ServerSocket;
import java.net.InetSocketAddress;
import java.nio.channels.SocketChannel;
import java.nio.channels.ServerSocketChannel;

public final class DesktopConnection implements Closeable {

    private static final int DEVICE_NAME_FIELD_LENGTH = 64;

    private static final String SOCKET_NAME = "scrcpy";
    private static final int SOCKET_PORT = 6612;

    private final Socket videoSocket;
    private final FileDescriptor videoFd;
    private final SocketChannel videoChannel;

    private final Socket controlSocket;
    private final InputStream controlInputStream;
    private final OutputStream controlOutputStream;

    private final ControlMessageReader reader = new ControlMessageReader();
    private final DeviceMessageWriter writer = new DeviceMessageWriter();

    private DesktopConnection(SocketChannel videoSocket, SocketChannel controlSocket) throws IOException {
        this.videoSocket = videoSocket.socket();
        this.controlSocket = controlSocket.socket();
        controlInputStream = controlSocket.socket().getInputStream();
        controlOutputStream = controlSocket.socket().getOutputStream();
//        videoFd = videoSocket.getFileDescriptor();
        videoFd = null;//no use
        videoChannel = videoSocket.socket().getChannel();
        Ln.i("videoChannel: " + videoChannel);
    }

    private static LocalSocket connect(String abstractName) throws IOException {
        LocalSocket localSocket = new LocalSocket();
        localSocket.connect(new LocalSocketAddress(abstractName));
        return localSocket;
    }

    public static DesktopConnection open(Device device, boolean tunnelForward) throws IOException {
        SocketChannel videoSocket = null;
        SocketChannel controlSocket = null;
        if (tunnelForward) {
            ServerSocketChannel localServerSocket = ServerSocketChannel.open();
            localServerSocket.socket().bind(new InetSocketAddress(SOCKET_PORT));
            try {
                videoSocket = localServerSocket.accept();
                // send one byte so the client may read() to detect a connection error
//                videoSocket.getOutputStream().write(0);//wen disable
                try {
                    controlSocket = localServerSocket.accept();
                } catch (IOException | RuntimeException e) {
                    videoSocket.close();
                    throw e;
                }
            } finally {
                localServerSocket.close();
            }
        } else {
//            videoSocket = connect(SOCKET_NAME);
//            try {
//                controlSocket = connect(SOCKET_NAME);
//            } catch (IOException | RuntimeException e) {
//                videoSocket.close();
//                throw e;
//            }
        }

        DesktopConnection connection = new DesktopConnection(videoSocket, controlSocket);
        Size videoSize = device.getScreenInfo().getVideoSize();
//        connection.send(Device.getDeviceName(), videoSize.getWidth(), videoSize.getHeight());//wen disable
        return connection;
    }

    public void close() throws IOException {
        videoSocket.shutdownInput();
        videoSocket.shutdownOutput();
        videoSocket.close();
        controlSocket.shutdownInput();
        controlSocket.shutdownOutput();
        controlSocket.close();
    }

//    @SuppressWarnings("checkstyle:MagicNumber")
//    private void send(String deviceName, int width, int height) throws IOException {
//        byte[] buffer = new byte[DEVICE_NAME_FIELD_LENGTH + 4];
//
//        byte[] deviceNameBytes = deviceName.getBytes(StandardCharsets.UTF_8);
//        int len = StringUtils.getUtf8TruncationIndex(deviceNameBytes, DEVICE_NAME_FIELD_LENGTH - 1);
//        System.arraycopy(deviceNameBytes, 0, buffer, 0, len);
//        // byte[] are always 0-initialized in java, no need to set '\0' explicitly
//
//        buffer[DEVICE_NAME_FIELD_LENGTH] = (byte) (width >> 8);
//        buffer[DEVICE_NAME_FIELD_LENGTH + 1] = (byte) width;
//        buffer[DEVICE_NAME_FIELD_LENGTH + 2] = (byte) (height >> 8);
//        buffer[DEVICE_NAME_FIELD_LENGTH + 3] = (byte) height;
//        IO.writeFully(videoFd, buffer, 0, buffer.length);
//    }

    public FileDescriptor getVideoFd() {
        return videoFd;
    }

    public SocketChannel getVideoChannel() {
        return videoChannel;
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
