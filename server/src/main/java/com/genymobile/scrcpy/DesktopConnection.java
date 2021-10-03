package com.genymobile.scrcpy;

import android.net.LocalServerSocket;
import android.net.LocalSocket;
import android.net.LocalSocketAddress;
import android.os.SystemClock;

import java.io.FileDescriptor;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;

public final class DesktopConnection extends Connection {

    private static final String SOCKET_NAME = "scrcpy";

    private final LocalSocket videoSocket;
    private final FileDescriptor videoFd;

    private final LocalSocket controlSocket;
    private final InputStream controlInputStream;
    private final OutputStream controlOutputStream;

    private final DeviceMessageWriter writer = new DeviceMessageWriter();

    private static LocalSocket connect(String abstractName) throws IOException {
        LocalSocket localSocket = new LocalSocket();
        localSocket.connect(new LocalSocketAddress(abstractName));
        return localSocket;
    }

    public DesktopConnection(Options options, VideoSettings videoSettings) throws IOException {
        super(options, videoSettings);
        boolean tunnelForward = options.isTunnelForward();
        if (tunnelForward) {
            LocalServerSocket localServerSocket = new LocalServerSocket(SOCKET_NAME);
            try {
                videoSocket = localServerSocket.accept();
                // send one byte so the client may read() to detect a connection error
                videoSocket.getOutputStream().write(0);
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
            videoSocket = connect(SOCKET_NAME);
            try {
                controlSocket = connect(SOCKET_NAME);
            } catch (IOException | RuntimeException e) {
                videoSocket.close();
                throw e;
            }
        }

        controlInputStream = controlSocket.getInputStream();
        controlOutputStream = controlSocket.getOutputStream();
        videoFd = videoSocket.getFileDescriptor();
        if (options.getControl()) {
            startEventController();
        }
        Size videoSize = device.getScreenInfo().getVideoSize();
        send(Device.getDeviceName(), videoSize.getWidth(), videoSize.getHeight());
        screenEncoder = new ScreenEncoder(videoSettings);
        screenEncoder.setDevice(device);
        screenEncoder.setConnection(this);
        screenEncoder.run();
    }

    public void close() throws IOException {
        videoSocket.shutdownInput();
        videoSocket.shutdownOutput();
        videoSocket.close();
        controlSocket.shutdownInput();
        controlSocket.shutdownOutput();
        controlSocket.close();
    }

    private void send(String deviceName, int width, int height) throws IOException {
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

    public void send(ByteBuffer data) {
        try {
            IO.writeFully(videoFd, data);
        } catch (IOException e) {
            Ln.e("Failed to send data", e);
        }
    }

    @Override
    boolean hasConnections() {
        return true;
    }

    public FileDescriptor getVideoFd() {
        return videoFd;
    }

    private void startEventController() {
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    // on start, power on the device
                    if (!Device.isScreenOn()) {
                        controller.turnScreenOn();

                        // dirty hack
                        // After POWER is injected, the device is powered on asynchronously.
                        // To turn the device screen off while mirroring, the client will send a message that
                        // would be handled before the device is actually powered on, so its effect would
                        // be "canceled" once the device is turned back on.
                        // Adding this delay prevents to handle the message before the device is actually
                        // powered on.
                        SystemClock.sleep(500);
                    }
                    while (true) {
                        ControlMessage controlEvent = receiveControlMessage();
                        if (controlEvent != null) {
                            controller.handleEvent(controlEvent);
                        }
                    }

                } catch (IOException e) {
                    // this is expected on close
                    Ln.d("Event controller stopped");
                }
            }
        }).start();
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
