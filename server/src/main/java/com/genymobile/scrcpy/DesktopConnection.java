package com.genymobile.scrcpy;

import android.net.LocalServerSocket;
import android.net.LocalSocket;
import android.net.LocalSocketAddress;
import android.os.ParcelFileDescriptor;

import java.io.Closeable;
import java.io.FileDescriptor;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.net.ServerSocket;
import java.net.Socket;

public final class DesktopConnection implements Closeable {

    private static final int DEVICE_NAME_FIELD_LENGTH = 64;

    private static final String SOCKET_NAME_PREFIX = "scrcpy";

    private final LocalSocket videoSocket;
    private final Socket videoExtSocket;
    private final FileDescriptor videoFd;

    private final LocalSocket audioSocket;
    private final Socket audioExtSocket;
    private final FileDescriptor audioFd;

    private final LocalSocket controlSocket;
    private final Socket controlExtSocket;
    private final InputStream controlInputStream;
    private final OutputStream controlOutputStream;

    private final ControlMessageReader reader = new ControlMessageReader();
    private final DeviceMessageWriter writer = new DeviceMessageWriter();

    private DesktopConnection(LocalSocket videoSocket, LocalSocket audioSocket, LocalSocket controlSocket) throws IOException {
        this.videoExtSocket = null;
        this.audioExtSocket = null;
        this.controlExtSocket = null;

        this.videoSocket = videoSocket;
        this.controlSocket = controlSocket;
        this.audioSocket = audioSocket;

        if (controlSocket != null) {
            controlInputStream = controlSocket.getInputStream();
            controlOutputStream = controlSocket.getOutputStream();
        } else {
            controlInputStream = null;
            controlOutputStream = null;
        }

        videoFd = videoSocket != null ? videoSocket.getFileDescriptor() : null;

        audioFd = audioSocket != null ? audioSocket.getFileDescriptor() : null;
    }

    private DesktopConnection(Socket videoExtSocket, Socket audioExtSocket, Socket controlExtSocket) throws IOException {
        this.videoSocket = null;
        this.controlSocket = null;
        this.audioSocket = null;

        this.videoExtSocket = videoExtSocket;
        this.audioExtSocket = audioExtSocket;
        this.controlExtSocket = controlExtSocket;

        if (controlExtSocket != null) {
            controlInputStream = controlExtSocket.getInputStream();
            controlOutputStream = controlExtSocket.getOutputStream();
        } else {
            controlInputStream = null;
            controlOutputStream = null;
        }

        if (videoExtSocket != null) {
            ParcelFileDescriptor pfd = ParcelFileDescriptor.fromSocket(videoExtSocket);
            videoFd = pfd.getFileDescriptor();
        } else {
            videoFd = null;
        }

        if (audioExtSocket != null) {
            ParcelFileDescriptor pfd = ParcelFileDescriptor.fromSocket(audioExtSocket);
            audioFd = pfd.getFileDescriptor();
        } else {
            audioFd = null;
        }
    }

    private static LocalSocket connect(String abstractName) throws IOException {
        LocalSocket localSocket = new LocalSocket();
        localSocket.connect(new LocalSocketAddress(abstractName));
        return localSocket;
    }

    private static String getSocketName(int scid) {
        if (scid == -1) {
            // If no SCID is set, use "scrcpy" to simplify using scrcpy-server alone
            return SOCKET_NAME_PREFIX;
        }

        return SOCKET_NAME_PREFIX + String.format("_%08x", scid);
    }

    public static DesktopConnection open(int scid, boolean tunnelForward, boolean video, boolean audio, boolean control, boolean sendDummyByte, int serverPort)
            throws IOException {
        String socketName = getSocketName(scid);

        LocalSocket videoSocket = null;
        LocalSocket audioSocket = null;
        LocalSocket controlSocket = null;

        Socket videoExtSocket = null;
        Socket audioExtSocket = null;
        Socket controlExtSocket = null;

        try {
            if (tunnelForward) {
                if (serverPort == -1) {
                    try (LocalServerSocket localServerSocket = new LocalServerSocket(socketName)) {
                        if (video) {
                            videoSocket = localServerSocket.accept();
                            if (sendDummyByte) {
                                // send one byte so the client may read() to detect a connection error
                                videoSocket.getOutputStream().write(0);
                                sendDummyByte = false;
                            }
                        }
                        if (audio) {
                            audioSocket = localServerSocket.accept();
                            if (sendDummyByte) {
                                // send one byte so the client may read() to detect a connection error
                                audioSocket.getOutputStream().write(0);
                                sendDummyByte = false;
                            }
                        }
                        if (control) {
                            controlSocket = localServerSocket.accept();
                            if (sendDummyByte) {
                                // send one byte so the client may read() to detect a connection error
                                controlSocket.getOutputStream().write(0);
                                sendDummyByte = false;
                            }
                        }
                    }
                } else {
                    try (ServerSocket serverSocket = new ServerSocket(serverPort)) {
                        if (video) {
                            videoExtSocket = serverSocket.accept();
                            if (sendDummyByte) {
                                // send one byte so the client may read() to detect a connection error
                                videoExtSocket.getOutputStream().write(0);
                                sendDummyByte = false;
                            }
                        }
                        if (audio) {
                            audioExtSocket = serverSocket.accept();
                            if (sendDummyByte) {
                                // send one byte so the client may read() to detect a connection error
                                audioExtSocket.getOutputStream().write(0);
                                sendDummyByte = false;
                            }
                        }
                        if (control) {
                            controlExtSocket = serverSocket.accept();
                            if (sendDummyByte) {
                                // send one byte so the client may read() to detect a connection error
                                controlExtSocket.getOutputStream().write(0);
                                sendDummyByte = false;
                            }
                        }
                    }

                    return new DesktopConnection(videoExtSocket, audioExtSocket, controlExtSocket);
                }
            } else {
                if (video) {
                    videoSocket = connect(socketName);
                }
                if (audio) {
                    audioSocket = connect(socketName);
                }
                if (control) {
                    controlSocket = connect(socketName);
                }
            }
        } catch (IOException | RuntimeException e) {
            if (videoSocket != null) {
                videoSocket.close();
            }
            if (audioSocket != null) {
                audioSocket.close();
            }
            if (controlSocket != null) {
                controlSocket.close();
            }
            if (videoExtSocket != null) {
                videoExtSocket.close();
            }
            if (audioExtSocket != null) {
                audioExtSocket.close();
            }
            if (controlExtSocket != null) {
                controlExtSocket.close();
            }
            throw e;
        }

        return new DesktopConnection(videoSocket, audioSocket, controlSocket);
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

    private Socket getFirstExtSocket() {
        if (videoExtSocket != null) {
            return videoExtSocket;
        }
        if (audioExtSocket != null) {
            return audioExtSocket;
        }
        return controlExtSocket;
    }

    public void close() throws IOException {
        if (videoSocket != null) {
            videoSocket.shutdownInput();
            videoSocket.shutdownOutput();
            videoSocket.close();
        }
        if (audioSocket != null) {
            audioSocket.shutdownInput();
            audioSocket.shutdownOutput();
            audioSocket.close();
        }
        if (controlSocket != null) {
            controlSocket.shutdownInput();
            controlSocket.shutdownOutput();
            controlSocket.close();
        }
        if (videoExtSocket != null) {
            videoExtSocket.shutdownInput();
            videoExtSocket.shutdownOutput();
            videoExtSocket.close();
        }
        if (audioExtSocket != null) {
            audioExtSocket.shutdownInput();
            audioExtSocket.shutdownOutput();
            audioExtSocket.close();
        }
        if (controlExtSocket != null) {
            controlExtSocket.shutdownInput();
            controlExtSocket.shutdownOutput();
            controlExtSocket.close();
        }
    }

    public void sendDeviceMeta(String deviceName) throws IOException {
        byte[] buffer = new byte[DEVICE_NAME_FIELD_LENGTH];

        byte[] deviceNameBytes = deviceName.getBytes(StandardCharsets.UTF_8);
        int len = StringUtils.getUtf8TruncationIndex(deviceNameBytes, DEVICE_NAME_FIELD_LENGTH - 1);
        System.arraycopy(deviceNameBytes, 0, buffer, 0, len);
        // byte[] are always 0-initialized in java, no need to set '\0' explicitly

        LocalSocket firstSocket = getFirstSocket();
        if (firstSocket == null) {
            ParcelFileDescriptor pfd = ParcelFileDescriptor.fromSocket(getFirstExtSocket());
            FileDescriptor fd = pfd.getFileDescriptor();
            IO.writeFully(fd, buffer, 0, buffer.length);
        } else {
            FileDescriptor fd = getFirstSocket().getFileDescriptor();
            IO.writeFully(fd, buffer, 0, buffer.length);
        }
    }

    public FileDescriptor getVideoFd() {
        return videoFd;
    }

    public FileDescriptor getAudioFd() {
        return audioFd;
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
