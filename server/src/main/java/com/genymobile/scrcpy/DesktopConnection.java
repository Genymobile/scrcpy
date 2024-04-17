package com.genymobile.scrcpy;

import android.net.LocalServerSocket;
import android.net.LocalSocket;
import android.net.LocalSocketAddress;
import android.os.SystemClock;

import java.io.FileDescriptor;
import java.io.IOException;

import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;

public final class DesktopConnection extends Connection {

    private static final String SOCKET_NAME_PREFIX = "scrcpy";

    private static final String SOCKET_NAME = "scrcpy";

    private final LocalSocket videoSocket;
    private final FileDescriptor videoFd;

    private final LocalSocket audioSocket;
    private final FileDescriptor audioFd;

    private final LocalSocket controlSocket;
    private final ControlChannel controlChannel;

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

    private static String getSocketName(int scid) {
        if (scid == -1) {
            // If no SCID is set, use "scrcpy" to simplify using scrcpy-server alone
            return SOCKET_NAME_PREFIX;
        }

        return SOCKET_NAME_PREFIX + String.format("_%08x", scid);
    }

    private LocalSocket getFirstSocket() {
        if (videoSocket != null) {
            return videoSocket;
        }
        if (audioSocket != null) {
            return audioSocket;
        }
        return controlSocket;
    }

    public void shutdown() throws IOException {
        if (videoSocket != null) {
            videoSocket.shutdownInput();
            videoSocket.shutdownOutput();
        }
        if (audioSocket != null) {
            audioSocket.shutdownInput();
            audioSocket.shutdownOutput();
        }
        if (controlSocket != null) {
            controlSocket.shutdownInput();
            controlSocket.shutdownOutput();
        }
    }

    public void close() throws IOException {
        if (videoSocket != null) {
            videoSocket.close();
        }
        if (audioSocket != null) {
            audioSocket.close();
        }
        if (controlSocket != null) {
            controlSocket.close();
        }
    }

    public void sendDeviceMeta(String deviceName) throws IOException {
        byte[] buffer = new byte[DEVICE_NAME_FIELD_LENGTH];

        byte[] deviceNameBytes = deviceName.getBytes(StandardCharsets.UTF_8);
        int len = StringUtils.getUtf8TruncationIndex(deviceNameBytes, DEVICE_NAME_FIELD_LENGTH - 1);
        System.arraycopy(deviceNameBytes, 0, buffer, 0, len);
        // byte[] are always 0-initialized in java, no need to set '\0' explicitly

        FileDescriptor fd = getFirstSocket().getFileDescriptor();
        IO.writeFully(fd, buffer, 0, buffer.length);
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
    
    public FileDescriptor getAudioFd() {
        return audioFd;
    }

    public ControlChannel getControlChannel() {
        return controlChannel;
    }
}
