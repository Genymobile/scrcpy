package com.genymobile.scrcpy.android;

import java.io.Closeable;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.EOFException;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.util.Map;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

/** A small ADB transport client for devices exposing adbd on TCP port 5555. */
final class AdbTransport implements Closeable {
    private static final int VERSION = 0x01000000;
    private static final int MAX_DATA = 1024 * 1024;
    private static final int CONNECT_TIMEOUT_MS = 8000;
    private static final int HANDSHAKE_TIMEOUT_MS = 60000;
    private static final int STREAM_TIMEOUT_SECONDS = 15;
    private static final int MAX_INCOMING_PACKETS = 32;
    private static final int MAX_WRITE_ACKS = 16;
    private static final int CNXN = command("CNXN");
    private static final int AUTH = command("AUTH");
    private static final int OPEN = command("OPEN");
    private static final int OKAY = command("OKAY");
    private static final int WRTE = command("WRTE");
    private static final int CLSE = command("CLSE");
    private static final int AUTH_TOKEN = 1;

    private final Object outputLock = new Object();
    private final Object stateLock = new Object();
    private final Map<Integer, AdbStream> streams = new ConcurrentHashMap<>();
    private final AtomicInteger nextLocalId = new AtomicInteger(1);
    private final SocketFactory socketFactory;
    private Socket socket;
    private DataInputStream input;
    private DataOutputStream output;
    private volatile boolean closed;
    private volatile int maxData = MAX_DATA;

    AdbTransport() {
        this(Socket::new);
    }

    AdbTransport(SocketFactory socketFactory) {
        this.socketFactory = socketFactory;
    }

    void connect(String host, int port, AdbAuthKey authKey) throws IOException {
        Socket candidate;
        synchronized (stateLock) {
            if (closed) {
                throw new IOException("ADB transport is closed");
            }
            candidate = socketFactory.create();
            if (closed) {
                closeQuietly(candidate);
                throw new IOException("ADB transport was closed while opening the socket");
            }
            socket = candidate;
        }

        try {
            ensureOpen(candidate);
            candidate.setTcpNoDelay(true);
            candidate.connect(new InetSocketAddress(host, port), CONNECT_TIMEOUT_MS);
            ensureOpen(candidate);
            candidate.setSoTimeout(HANDSHAKE_TIMEOUT_MS);
            synchronized (stateLock) {
                if (closed || socket != candidate) {
                    throw new IOException("ADB transport was closed while connecting");
                }
                input = new DataInputStream(candidate.getInputStream());
                output = new DataOutputStream(candidate.getOutputStream());
            }

            sendPacket(CNXN, VERSION, MAX_DATA,
                    "host::features=shell_v2".getBytes(StandardCharsets.UTF_8));
            Packet packet;
            boolean signed = false;
            boolean publicKeySent = false;
            for (;;) {
                packet = readPacket();
                if (packet.command != AUTH) {
                    break;
                }
                if (packet.arg0 != AUTH_TOKEN) {
                    throw new IOException("Unsupported ADB authentication mode: " + packet.arg0);
                }
                if (!signed) {
                    sendPacket(AUTH, 2, 0, authKey.sign(packet.data));
                    signed = true;
                } else if (!publicKeySent) {
                    sendPacket(AUTH, 3, 0, authKey.publicKeyPayload());
                    publicKeySent = true;
                } else {
                    throw new IOException("The target rejected this ADB key. Authorize it on the target phone and retry.");
                }
            }
            if (packet.command != CNXN) {
                throw new IOException("ADB handshake failed: " + label(packet.command));
            }
            synchronized (stateLock) {
                if (closed || socket != candidate) {
                    throw new IOException("ADB transport was closed during the handshake");
                }
                candidate.setSoTimeout(0);
                maxData = packet.arg1 > 0 ? Math.min(packet.arg1, MAX_DATA) : MAX_DATA;
            }

            new Thread(this::readLoop, "adb-transport-reader").start();
        } catch (IOException | RuntimeException error) {
            closeCandidate(candidate);
            throw error;
        }
    }

    private void ensureOpen(Socket candidate) throws IOException {
        synchronized (stateLock) {
            if (closed || socket != candidate || candidate.isClosed()) {
                throw new IOException("ADB transport was closed while connecting");
            }
        }
    }

    AdbStream open(String destination) throws IOException {
        return open(destination, STREAM_TIMEOUT_SECONDS * 1000L);
    }

    AdbStream open(String destination, long timeoutMillis) throws IOException {
        if (closed) {
            throw new IOException("ADB transport is closed");
        }
        int localId = nextLocalId.getAndIncrement();
        AdbStream stream = new AdbStream(this, localId);
        streams.put(localId, stream);
        try {
            sendPacket(OPEN, localId, 0, (destination + "\0").getBytes(StandardCharsets.UTF_8));
            stream.awaitOpen(timeoutMillis);
            return stream;
        } catch (IOException e) {
            streams.remove(localId);
            try {
                stream.close();
            } catch (IOException ignored) {
            }
            throw e;
        }
    }

    private void readLoop() {
        try {
            while (!closed) {
                Packet packet = readPacket();
                AdbStream stream = streams.get(packet.arg1);
                if (packet.command == OKAY) {
                    if (stream != null) {
                        stream.onOkay(packet.arg0);
                    }
                } else if (packet.command == WRTE) {
                    if (stream != null) {
                        stream.onData(packet.data);
                        sendPacket(OKAY, packet.arg1, packet.arg0, new byte[0]);
                    } else {
                        sendPacket(CLSE, packet.arg1, packet.arg0, new byte[0]);
                    }
                } else if (packet.command == CLSE) {
                    if (stream != null) {
                        stream.onClose();
                        streams.remove(packet.arg1);
                    }
                    sendPacket(CLSE, packet.arg1, packet.arg0, new byte[0]);
                }
            }
        } catch (IOException e) {
            if (!closed) {
                for (AdbStream stream : streams.values()) {
                    stream.onError(e);
                }
            }
        } finally {
            for (AdbStream stream : streams.values()) {
                stream.onClose();
            }
        }
    }

    private Packet readPacket() throws IOException {
        int command = readIntLE(input);
        int arg0 = readIntLE(input);
        int arg1 = readIntLE(input);
        int length = readIntLE(input);
        int checksum = readIntLE(input);
        int magic = readIntLE(input);
        if (magic != (command ^ 0xffffffff)) {
            throw new IOException("Invalid ADB packet magic");
        }
        if (length < 0 || length > MAX_DATA) {
            throw new IOException("Invalid ADB packet length: " + length);
        }
        byte[] data = new byte[length];
        input.readFully(data);
        if (checksum(data) != checksum && command != CNXN) {
            throw new IOException("Invalid ADB packet checksum");
        }
        return new Packet(command, arg0, arg1, data);
    }

    void sendStreamData(AdbStream stream, byte[] data) throws IOException {
        if (data.length > maxData) {
            throw new IOException("ADB stream packet is too large: " + data.length);
        }
        sendPacket(WRTE, stream.localId, stream.remoteId, data);
    }

    void sendStreamOkay(AdbStream stream) throws IOException {
        sendPacket(OKAY, stream.localId, stream.remoteId, new byte[0]);
    }

    void closeStream(AdbStream stream) throws IOException {
        streams.remove(stream.localId);
        if (stream.remoteId != 0) {
            sendPacket(CLSE, stream.localId, stream.remoteId, new byte[0]);
        }
    }

    private void sendPacket(int command, int arg0, int arg1, byte[] data) throws IOException {
        synchronized (outputLock) {
            DataOutputStream currentOutput;
            synchronized (stateLock) {
                if (closed || output == null) {
                    throw new IOException("ADB transport is closed");
                }
                currentOutput = output;
            }
            writeIntLE(currentOutput, command);
            writeIntLE(currentOutput, arg0);
            writeIntLE(currentOutput, arg1);
            writeIntLE(currentOutput, data.length);
            writeIntLE(currentOutput, checksum(data));
            writeIntLE(currentOutput, command ^ 0xffffffff);
            currentOutput.write(data);
            currentOutput.flush();
        }
    }

    @Override
    public void close() {
        Socket currentSocket;
        synchronized (stateLock) {
            if (closed) {
                return;
            }
            closed = true;
            currentSocket = socket;
            socket = null;
        }
        for (AdbStream stream : streams.values()) {
            stream.onClose();
        }
        try {
            if (currentSocket != null) {
                currentSocket.close();
            }
        } catch (IOException ignored) {
        }
    }

    private void closeCandidate(Socket candidate) {
        synchronized (stateLock) {
            if (socket == candidate) {
                socket = null;
            }
        }
        closeQuietly(candidate);
    }

    private static void closeQuietly(Socket socket) {
        try {
            socket.close();
        } catch (IOException ignored) {
        }
    }

    interface SocketFactory {
        Socket create() throws IOException;
    }

    private static int command(String value) {
        return (value.charAt(0) & 0xff)
                | ((value.charAt(1) & 0xff) << 8)
                | ((value.charAt(2) & 0xff) << 16)
                | ((value.charAt(3) & 0xff) << 24);
    }

    private static String label(int command) {
        return new String(new byte[]{
                (byte) command,
                (byte) (command >> 8),
                (byte) (command >> 16),
                (byte) (command >> 24)
        }, StandardCharsets.US_ASCII);
    }

    private static int checksum(byte[] data) {
        int result = 0;
        for (byte value : data) {
            result += value & 0xff;
        }
        return result;
    }

    private static int readIntLE(DataInputStream input) throws IOException {
        int b0 = input.readUnsignedByte();
        int b1 = input.readUnsignedByte();
        int b2 = input.readUnsignedByte();
        int b3 = input.readUnsignedByte();
        return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
    }

    private static void writeIntLE(DataOutputStream output, int value) throws IOException {
        output.writeByte(value);
        output.writeByte(value >> 8);
        output.writeByte(value >> 16);
        output.writeByte(value >> 24);
    }

    private static final class Packet {
        final int command;
        final int arg0;
        final int arg1;
        final byte[] data;

        Packet(int command, int arg0, int arg1, byte[] data) {
            this.command = command;
            this.arg0 = arg0;
            this.arg1 = arg1;
            this.data = data;
        }
    }

    static final class AdbStream implements Closeable {
        private static final byte[] EOF = new byte[0];
        private final AdbTransport transport;
        private final ArrayBlockingQueue<byte[]> incoming = new ArrayBlockingQueue<>(MAX_INCOMING_PACKETS);
        private final ArrayBlockingQueue<Integer> writeAcks = new ArrayBlockingQueue<>(MAX_WRITE_ACKS);
        private final CountDownLatch opened = new CountDownLatch(1);
        private final int localId;
        private volatile int remoteId;
        private volatile IOException error;
        private volatile boolean closed;
        private byte[] pending;
        private int pendingOffset;

        AdbStream(AdbTransport transport, int localId) {
            this.transport = transport;
            this.localId = localId;
        }

        void awaitOpen() throws IOException {
            awaitOpen(STREAM_TIMEOUT_SECONDS * 1000L);
        }

        void awaitOpen(long timeoutMillis) throws IOException {
            try {
                if (!opened.await(timeoutMillis, TimeUnit.MILLISECONDS)) {
                    throw new IOException("Timed out opening ADB stream");
                }
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                throw new IOException("Interrupted opening ADB stream", e);
            }
            checkError();
            if (closed || remoteId == 0) {
                throw new IOException("ADB stream was closed while opening");
            }
        }

        void onOkay(int remoteId) {
            if (this.remoteId == 0) {
                this.remoteId = remoteId;
                opened.countDown();
            } else if (!writeAcks.offer(remoteId)) {
                onError(new IOException("ADB write acknowledgement buffer limit exceeded"));
            }
        }

        void onData(byte[] data) {
            if (!closed && !incoming.offer(data)) {
                onError(new IOException("ADB input buffer limit exceeded"));
            }
        }

        void onClose() {
            if (closed) {
                return;
            }
            closed = true;
            opened.countDown();
            incoming.clear();
            incoming.offer(EOF);
            writeAcks.clear();
            writeAcks.offer(0);
        }

        void onError(IOException error) {
            this.error = error;
            onClose();
        }

        void write(byte[] data) throws IOException {
            if (data.length == 0) {
                return;
            }
            checkError();
            transport.sendStreamData(this, data);
            try {
                Integer ack = writeAcks.poll(STREAM_TIMEOUT_SECONDS, TimeUnit.SECONDS);
                if (ack == null) {
                    throw new IOException("Timed out writing to ADB stream");
                }
                checkError();
                if (ack == 0 || closed) {
                    throw new IOException("ADB stream closed while writing");
                }
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                throw new IOException("Interrupted writing to ADB stream", e);
            }
        }

        int readUnsignedByte() throws IOException {
            byte[] one = new byte[1];
            readFully(one, 0, 1);
            return one[0] & 0xff;
        }

        void readFully(byte[] target, int offset, int length) throws IOException {
            int copied = 0;
            while (copied < length) {
                if (pending == null || pendingOffset == pending.length) {
                    pending = takeIncoming();
                    pendingOffset = 0;
                }
                int count = Math.min(length - copied, pending.length - pendingOffset);
                System.arraycopy(pending, pendingOffset, target, offset + copied, count);
                pendingOffset += count;
                copied += count;
            }
        }

        byte[] readChunk() throws IOException {
            if (pending != null && pendingOffset < pending.length) {
                byte[] result = new byte[pending.length - pendingOffset];
                System.arraycopy(pending, pendingOffset, result, 0, result.length);
                pending = null;
                pendingOffset = 0;
                return result;
            }
            return takeIncoming();
        }

        byte[] readChunk(long timeoutMillis) throws IOException {
            if (pending != null && pendingOffset < pending.length) {
                byte[] result = new byte[pending.length - pendingOffset];
                System.arraycopy(pending, pendingOffset, result, 0, result.length);
                pending = null;
                pendingOffset = 0;
                return result;
            }
            return takeIncoming(timeoutMillis);
        }

        private byte[] takeIncoming() throws IOException {
            try {
                byte[] result = incoming.take();
                if (result == EOF) {
                    checkError();
                    throw new EOFException("ADB stream closed");
                }
                return result;
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                throw new IOException("Interrupted reading ADB stream", e);
            }
        }

        private byte[] takeIncoming(long timeoutMillis) throws IOException {
            try {
                byte[] result = incoming.poll(timeoutMillis, TimeUnit.MILLISECONDS);
                if (result == null) {
                    throw new java.net.SocketTimeoutException("Timed out reading ADB stream");
                }
                if (result == EOF) {
                    checkError();
                    throw new EOFException("ADB stream closed");
                }
                return result;
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                throw new IOException("Interrupted reading ADB stream", e);
            }
        }

        private void checkError() throws IOException {
            if (error != null) {
                throw error;
            }
        }

        @Override
        public void close() throws IOException {
            if (closed) {
                return;
            }
            closed = true;
            try {
                transport.closeStream(this);
            } finally {
                incoming.clear();
                incoming.offer(EOF);
                writeAcks.clear();
                writeAcks.offer(0);
            }
        }
    }
}
