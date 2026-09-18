package com.genymobile.scrcpy.android;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertThrows;

import java.io.IOException;
import java.net.Socket;
import java.net.SocketAddress;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;

import org.junit.Test;

public final class AdbTransportLifecycleTest {
    @Test
    public void doesNotCreateASocketAfterClose() {
        boolean[] created = {false};
        AdbTransport transport = new AdbTransport(() -> {
            created[0] = true;
            return new Socket();
        });
        transport.close();

        assertThrows(IOException.class, () -> transport.connect("127.0.0.1", 5555, null));
        assertFalse(created[0]);
    }

    @Test
    public void closesSocketWhenCloseWinsDuringConnect() throws Exception {
        BlockingSocket socket = new BlockingSocket();
        AdbTransport transport = new AdbTransport(() -> socket);
        ExecutorService executor = Executors.newSingleThreadExecutor();
        try {
            Future<?> connection = executor.submit(() -> {
                try {
                    transport.connect("127.0.0.1", 5555, null);
                } catch (IOException expected) {
                }
            });
            assertTrue(socket.connectStarted.await(2, TimeUnit.SECONDS));
            transport.close();
            socket.allowConnect.countDown();
            connection.get(2, TimeUnit.SECONDS);
            assertTrue(socket.closed);
        } finally {
            socket.allowConnect.countDown();
            transport.close();
            executor.shutdownNow();
        }
    }

    private static final class BlockingSocket extends Socket {
        private final CountDownLatch connectStarted = new CountDownLatch(1);
        private final CountDownLatch allowConnect = new CountDownLatch(1);
        private volatile boolean closed;

        @Override
        public void connect(SocketAddress endpoint, int timeout) throws IOException {
            connectStarted.countDown();
            try {
                allowConnect.await(2, TimeUnit.SECONDS);
            } catch (InterruptedException error) {
                Thread.currentThread().interrupt();
                throw new IOException("Interrupted", error);
            }
            if (closed) {
                throw new IOException("Socket closed");
            }
        }

        @Override
        public void close() {
            closed = true;
        }

        @Override
        public boolean isClosed() {
            return closed;
        }
    }
}
