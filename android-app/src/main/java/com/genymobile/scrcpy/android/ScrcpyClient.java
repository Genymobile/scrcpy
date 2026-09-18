package com.genymobile.scrcpy.android;

import android.media.MediaCodec;
import android.media.MediaFormat;
import android.os.Build;
import android.os.SystemClock;
import android.util.Log;
import android.view.MotionEvent;
import android.view.Surface;

import java.io.ByteArrayOutputStream;
import java.io.Closeable;
import java.io.EOFException;
import java.io.IOException;
import java.io.InputStream;
import java.net.SocketTimeoutException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

final class ScrcpyClient implements Closeable {
    private static final String TAG = "ScrcpyClient";
    private static final long SHELL_COMMAND_TIMEOUT_MS = 10_000L;
    private static final int VIDEO_BIT_RATE = 8_000_000;
    private static final int VIDEO_MAX_FPS = 60;
    private static final int VIDEO_MIN_AUTO_SIZE = 720;
    private static final int VIDEO_AUTO_STEP = 160;
    private static final long VIDEO_SLOW_PACKET_MILLIS = 350L;
    private static final long VIDEO_UPGRADE_STABLE_MILLIS = 30_000L;
    private static final long VIDEO_RESOLUTION_COOLDOWN_MILLIS = 5_000L;
    interface Listener {
        void onConnected(int width, int height);
        void onVideoSizeChanged(int width, int height);
        void onVideoStarted();
        void onStatus(int messageId, Object... arguments);
        void onError(Throwable error);
        void onDisconnected();
    }

    interface AppListListener {
        void onApps(List<String> packages);
        void onError(Throwable error);
    }

    interface ActionListener {
        void onSuccess();
        void onError(Throwable error);
    }

    private static final String SERVER_PATH = "/data/local/tmp/scrcpy-server.jar";
    private final InputStream serverAsset;
    private final Listener listener;
    private final AdbAuthKey authKey;
    private final boolean automaticResolution;
    private final int configuredMaxSize;
    private final AtomicBoolean stopped = new AtomicBoolean();
    private final Object lifecycleLock = new Object();
    private final ExecutorService controlExecutor = Executors.newSingleThreadExecutor();
    private final Object touchLock = new Object();
    private TouchEvent pendingMove;
    private boolean moveDrainQueued;
    private volatile AdbTransport adb;
    private volatile AdbTransport.AdbStream shell;
    private volatile AdbTransport.AdbStream video;
    private volatile AdbTransport.AdbStream control;
    private volatile ControlWriter controlWriter;

    ScrcpyClient(InputStream serverAsset, AdbAuthKey authKey, boolean automaticResolution,
            int configuredMaxSize, Listener listener) {
        this.serverAsset = serverAsset;
        this.authKey = authKey;
        this.automaticResolution = automaticResolution;
        this.configuredMaxSize = configuredMaxSize;
        this.listener = listener;
    }

    void connect(String host, int port, Surface surface) {
        new Thread(() -> {
            try {
                listener.onStatus(R.string.status_connecting, host, port);
                AdbTransport transport = new AdbTransport();
                synchronized (lifecycleLock) {
                    if (stopped.get()) {
                        transport.close();
                        return;
                    }
                    adb = transport;
                }
                transport.connect(host, port, authKey);
                ensureActive();
                listener.onStatus(R.string.status_pushing_server);
                pushServer(transport);

                int scid = (int) (System.nanoTime() & 0x7fffffff);
                String socketName = "scrcpy_" + String.format(Locale.US, "%08x", scid);
                String command = "CLASSPATH=" + SERVER_PATH + " app_process / "
                        + "com.genymobile.scrcpy.Server 4.1"
                        + " scid=" + String.format(Locale.US, "%08x", scid)
                        + " tunnel_forward=true"
                        + " video_codec=h264 video_bit_rate=" + VIDEO_BIT_RATE
                        + " max_size=" + configuredMaxSize + " max_fps=" + VIDEO_MAX_FPS
                        + " audio=false control=true send_dummy_byte=false"
                        + " send_device_meta=false send_stream_meta=true send_frame_meta=true"
                        + " power_on=true power_off_on_close=true cleanup=true";
                AdbTransport.AdbStream openedShell = transport.open("shell:" + command);
                adoptShell(openedShell);
                Thread shellDrain = new Thread(this::drainShell, "scrcpy-shell-drain");
                shellDrain.start();

                listener.onStatus(R.string.status_opening_streams);
                AdbTransport.AdbStream openedVideo = openLocalSocket(transport, socketName);
                adoptVideo(openedVideo);
                AdbTransport.AdbStream openedControl = openLocalSocket(transport, socketName);
                adoptControl(openedControl);

                int codecId = readIntBE(openedVideo);
                if (codecId != 0x68323634) {
                    throw new IOException("Target returned unsupported video codec: 0x" + Integer.toHexString(codecId));
                }
                byte[] session = new byte[12];
                openedVideo.readFully(session, 0, session.length);
                if ((session[0] & 0x80) == 0) {
                    throw new IOException("Target did not send a scrcpy video session header");
                }
                int width = readIntBE(session, 4);
                int height = readIntBE(session, 8);
                AdbLimits.videoPixels(width, height);
                listener.onConnected(width, height);

                new Thread(() -> decodeVideo(surface, width, height, openedVideo), "scrcpy-video").start();
            } catch (Throwable error) {
                fail(error);
            }
        }, "scrcpy-connect").start();
    }

    private void ensureActive() throws IOException {
        if (stopped.get()) {
            throw new IOException("Connection stopped");
        }
    }

    private void adoptShell(AdbTransport.AdbStream stream) throws IOException {
        synchronized (lifecycleLock) {
            if (stopped.get()) {
                closeQuietly(stream);
                throw new IOException("Connection stopped");
            }
            shell = stream;
        }
    }

    private void adoptVideo(AdbTransport.AdbStream stream) throws IOException {
        synchronized (lifecycleLock) {
            if (stopped.get()) {
                closeQuietly(stream);
                throw new IOException("Connection stopped");
            }
            video = stream;
        }
    }

    private void adoptControl(AdbTransport.AdbStream stream) throws IOException {
        synchronized (lifecycleLock) {
            if (stopped.get()) {
                closeQuietly(stream);
                throw new IOException("Connection stopped");
            }
            control = stream;
            controlWriter = new ControlWriter(stream);
        }
    }

    private void pushServer(AdbTransport transport) throws IOException {
        try (AdbTransport.AdbStream sync = transport.open("sync:")) {
            byte[] path = (SERVER_PATH + ",33204").getBytes(StandardCharsets.UTF_8);
            sendSync(sync, "SEND", path);
            byte[] buffer = new byte[64 * 1024];
            int read;
            while ((read = serverAsset.read(buffer)) != -1) {
                byte[] chunk = new byte[read];
                System.arraycopy(buffer, 0, chunk, 0, read);
                sendSync(sync, "DATA", chunk);
            }
            byte[] done = new byte[12];
            byte[] doneId = "DONE".getBytes(StandardCharsets.US_ASCII);
            System.arraycopy(doneId, 0, done, 0, doneId.length);
            writeIntLE(done, 4, 0);
            writeIntLE(done, 8, 33204);
            sync.write(done);

            byte[] result = new byte[8];
            sync.readFully(result, 0, result.length);
            String status = new String(result, 0, 4, StandardCharsets.US_ASCII);
            int length = readIntLE(result, 4);
            if (!"OKAY".equals(status)) {
                if (length < 0 || length > 4096) {
                    throw new IOException("ADB push returned an invalid error length: " + length);
                }
                byte[] message = new byte[length];
                sync.readFully(message, 0, message.length);
                throw new IOException("ADB push failed: " + new String(message, StandardCharsets.UTF_8));
            }
        }
    }

    private AdbTransport.AdbStream openLocalSocket(AdbTransport transport, String socketName) throws IOException {
        IOException lastError = null;
        for (int attempt = 0; attempt < 30 && !stopped.get(); ++attempt) {
            try {
                return transport.open("localabstract:" + socketName);
            } catch (IOException error) {
                lastError = error;
                try {
                    Thread.sleep(100);
                } catch (InterruptedException interrupted) {
                    Thread.currentThread().interrupt();
                    throw new IOException("Interrupted while opening scrcpy socket", interrupted);
                }
            }
        }
        throw lastError != null ? lastError : new IOException("Connection stopped");
    }

    private static void sendSync(AdbTransport.AdbStream stream, String id, byte[] payload) throws IOException {
        byte[] message = new byte[8 + payload.length];
        byte[] idBytes = id.getBytes(StandardCharsets.US_ASCII);
        System.arraycopy(idBytes, 0, message, 0, 4);
        writeIntLE(message, 4, payload.length);
        System.arraycopy(payload, 0, message, 8, payload.length);
        stream.write(message);
    }

    private void drainShell() {
        try {
            while (!stopped.get()) {
                shell.readChunk();
            }
        } catch (IOException error) {
            if (!stopped.get()) {
                fail(new IOException("The scrcpy server shell stopped", error));
            }
        }
    }

    private void decodeVideo(Surface surface, int width, int height,
            AdbTransport.AdbStream videoStream) {
        MediaCodec decoder = null;
        boolean firstFrameReported = false;
        Throwable failure = null;
        int currentMaxSize = configuredMaxSize;
        int slowPacketCount = 0;
        long stableSince = SystemClock.uptimeMillis();
        long lastResolutionChange = 0L;
        try {
            decoder = createDecoder(surface, width, height);
            MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
            byte[] header = new byte[12];
            byte[] packet = new byte[0];
            while (!stopped.get()) {
                long packetStart = SystemClock.uptimeMillis();
                videoStream.readFully(header, 0, header.length);
                long ptsAndFlags = readLongBE(header, 0);
                int length = readIntBE(header, 8);

                if ((ptsAndFlags & Long.MIN_VALUE) != 0) {
                    int newWidth = (int) ptsAndFlags;
                    int newHeight = length;
                    AdbLimits.videoPixels(newWidth, newHeight);
                    releaseDecoder(decoder);
                    decoder = createDecoder(surface, newWidth, newHeight);
                    listener.onVideoSizeChanged(newWidth, newHeight);
                    slowPacketCount = 0;
                    stableSince = SystemClock.uptimeMillis();
                    packet = new byte[0];
                    continue;
                }

                AdbLimits.checkVideoPacketLength(length);
                if (packet.length < length) {
                    packet = new byte[length];
                }
                videoStream.readFully(packet, 0, length);
                long packetReadMillis = SystemClock.uptimeMillis() - packetStart;
                if (automaticResolution) {
                    if (packetReadMillis >= VIDEO_SLOW_PACKET_MILLIS) {
                        ++slowPacketCount;
                    } else {
                        slowPacketCount = 0;
                    }
                    long now = SystemClock.uptimeMillis();
                    if (slowPacketCount >= 5 && currentMaxSize > VIDEO_MIN_AUTO_SIZE
                            && now - lastResolutionChange >= VIDEO_RESOLUTION_COOLDOWN_MILLIS) {
                        currentMaxSize = Math.max(VIDEO_MIN_AUTO_SIZE,
                                currentMaxSize - VIDEO_AUTO_STEP);
                        requestVideoMaxSize(currentMaxSize);
                        slowPacketCount = 0;
                        stableSince = now;
                        lastResolutionChange = now;
                    } else if (slowPacketCount == 0
                            && currentMaxSize < configuredMaxSize
                            && now - stableSince >= VIDEO_UPGRADE_STABLE_MILLIS
                            && now - lastResolutionChange >= VIDEO_RESOLUTION_COOLDOWN_MILLIS) {
                        currentMaxSize = Math.min(configuredMaxSize,
                                currentMaxSize + VIDEO_AUTO_STEP);
                        requestVideoMaxSize(currentMaxSize);
                        stableSince = now;
                        lastResolutionChange = now;
                    }
                }

                int index;
                do {
                    index = decoder.dequeueInputBuffer(10000);
                    if (index < 0) {
                        drainDecoder(decoder, info);
                    }
                } while (index < 0 && !stopped.get());
                if (index < 0) {
                    break;
                }
                java.nio.ByteBuffer input = decoder.getInputBuffer(index);
                if (input == null || input.capacity() < length) {
                    throw new IOException("MediaCodec input buffer is too small");
                }
                input.clear();
                input.put(packet, 0, length);
                int flags = (ptsAndFlags & (1L << 62)) != 0 ? MediaCodec.BUFFER_FLAG_CODEC_CONFIG : 0;
                long ptsUs = ptsAndFlags & ((1L << 61) - 1);
                decoder.queueInputBuffer(index, 0, length, ptsUs, flags);
                int rendered = drainDecoder(decoder, info);
                if (!firstFrameReported && rendered > 0) {
                    firstFrameReported = true;
                    listener.onVideoStarted();
                    listener.onStatus(R.string.status_video_rendering, width, height);
                }
            }
        } catch (Throwable error) {
            failure = error;
        } finally {
            releaseDecoder(decoder);
            if (failure != null) {
                fail(failure);
            } else if (!stopped.get()) {
                fail(new IOException("The scrcpy video stream stopped"));
            }
        }
    }

    private static MediaCodec createDecoder(Surface surface, int width, int height)
            throws IOException {
        MediaCodec decoder = null;
        try {
            MediaFormat format = MediaFormat.createVideoFormat(
                    MediaFormat.MIMETYPE_VIDEO_AVC, width, height);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                format.setInteger(MediaFormat.KEY_LOW_LATENCY, 1);
            }
            decoder = MediaCodec.createDecoderByType(MediaFormat.MIMETYPE_VIDEO_AVC);
            decoder.configure(format, surface, null, 0);
            decoder.start();
            return decoder;
        } catch (Exception error) {
            releaseDecoder(decoder);
            throw new IOException("Could not configure the video decoder", error);
        }
    }

    private static void releaseDecoder(MediaCodec decoder) {
        if (decoder == null) {
            return;
        }
        try {
            decoder.stop();
        } catch (Exception ignored) {
        }
        try {
            decoder.release();
        } catch (Exception ignored) {
        }
    }

    private static int drainDecoder(MediaCodec decoder, MediaCodec.BufferInfo info) {
        int rendered = 0;
        for (;;) {
            int output = decoder.dequeueOutputBuffer(info, 0);
            if (output >= 0) {
                decoder.releaseOutputBuffer(output, true);
                ++rendered;
            } else if (output == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED
                    || output == MediaCodec.INFO_TRY_AGAIN_LATER) {
                return rendered;
            }
        }
    }

    void touch(int action, float x, float y, float pressure, int width, int height) {
        ControlWriter writer = controlWriter;
        if (writer == null) {
            return;
        }
        TouchEvent event = new TouchEvent(action, x, y, pressure, width, height);
        if (action == MotionEvent.ACTION_MOVE) {
            synchronized (touchLock) {
                pendingMove = event;
                if (moveDrainQueued) {
                    return;
                }
                moveDrainQueued = true;
            }
            enqueueControl(() -> drainTouchMoves(writer));
            return;
        }
        enqueueControl(() -> writer.touch(action, x, y, pressure, width, height));
    }

    private void drainTouchMoves(ControlWriter writer) throws IOException {
        for (;;) {
            TouchEvent event;
            synchronized (touchLock) {
                event = pendingMove;
                pendingMove = null;
                if (event == null) {
                    moveDrainQueued = false;
                    return;
                }
            }
            writer.touch(event.action, event.x, event.y, event.pressure, event.width, event.height);
        }
    }

    private static final class TouchEvent {
        final int action;
        final float x;
        final float y;
        final float pressure;
        final int width;
        final int height;

        TouchEvent(int action, float x, float y, float pressure, int width, int height) {
            this.action = action;
            this.x = x;
            this.y = y;
            this.pressure = pressure;
            this.width = width;
            this.height = height;
        }
    }

    void back() {
        ControlWriter writer = controlWriter;
        if (writer == null) {
            return;
        }
        enqueueControl(writer::sendBack);
    }

    void wake() {
        ControlWriter writer = controlWriter;
        if (writer == null) {
            return;
        }
        enqueueControl(writer::wake);
    }

    private void requestVideoMaxSize(int maxSize) {
        ControlWriter writer = controlWriter;
        if (writer != null) {
            enqueueControl(() -> writer.setVideoMaxSize(maxSize));
        }
    }

    void listInstalledApps(AppListListener listener) {
        new Thread(() -> {
            try {
                String output = runShellCommand("pm list packages -3");
                List<String> packages = new ArrayList<>();
                for (String line : output.split("\\r?\\n")) {
                    if (line.startsWith("package:")) {
                        String packageName = line.substring("package:".length()).trim();
                        if (AndroidClientValidation.isValidPackageName(packageName)) {
                            packages.add(packageName);
                        }
                    }
                }
                Collections.sort(packages);
                listener.onApps(packages);
            } catch (Throwable error) {
                listener.onError(error);
            }
        }, "scrcpy-list-apps").start();
    }

    void launchApp(String packageName, ActionListener listener) {
        if (!AndroidClientValidation.isValidPackageName(packageName)) {
            listener.onError(new IOException("Invalid Android package name"));
            return;
        }
        new Thread(() -> {
            try {
                String output = runShellCommand("monkey -p " + packageName
                        + " -c android.intent.category.LAUNCHER 1");
                String lower = output.toLowerCase(Locale.US);
                if (lower.contains("no activities found") || lower.contains("monkey aborted")) {
                    throw new IOException("No launchable activity found for " + packageName);
                }
                listener.onSuccess();
            } catch (Throwable error) {
                listener.onError(error);
            }
        }, "scrcpy-launch-app").start();
    }

    private String runShellCommand(String command) throws IOException {
        AdbTransport transport = adb;
        if (transport == null || stopped.get()) {
            throw new IOException("ADB connection is not active");
        }
        long deadline = System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(SHELL_COMMAND_TIMEOUT_MS);
        AdbTransport.AdbStream stream = transport.open("shell:" + command, SHELL_COMMAND_TIMEOUT_MS);
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        try {
            while (!stopped.get()) {
                long remainingMillis = AdbLimits.remainingMillis(deadline, System.nanoTime());
                if (remainingMillis == 0) {
                    throw new SocketTimeoutException("Timed out waiting for target shell command");
                }
                byte[] chunk = stream.readChunk(remainingMillis);
                AdbLimits.appendShellOutput(output, chunk);
            }
            throw new IOException("ADB connection stopped");
        } catch (EOFException end) {
            // the shell command completed and closed its ADB stream
        } finally {
            closeQuietly(stream);
        }
        return output.toString(StandardCharsets.UTF_8.name());
    }

    private void enqueueControl(ControlAction action) {
        if (stopped.get()) {
            return;
        }
        try {
            controlExecutor.execute(() -> {
                try {
                    action.execute();
                } catch (IOException error) {
                    if (!stopped.get()) {
                        fail(error);
                    }
                }
            });
        } catch (RejectedExecutionException ignored) {
            // A close may win the race after the stopped check.
        }
    }

    @Override
    public void close() {
        if (!stopped.compareAndSet(false, true)) {
            return;
        }
        controlExecutor.shutdownNow();
        new Thread(() -> {
            closeResources();
            listener.onDisconnected();
        }, "scrcpy-close").start();
    }

    private void fail(Throwable error) {
        if (stopped.get()) {
            return;
        }
        Log.e(TAG, "Connection failed", error);
        closeWithError(error);
    }

    private void closeWithError(Throwable error) {
        if (!stopped.compareAndSet(false, true)) {
            return;
        }
        controlExecutor.shutdownNow();
        new Thread(() -> {
            try {
                listener.onError(error);
            } finally {
                closeResources();
                listener.onDisconnected();
            }
        }, "scrcpy-failure-close").start();
    }

    private void closeResources() {
        AdbTransport.AdbStream currentControl;
        AdbTransport.AdbStream currentVideo;
        AdbTransport.AdbStream currentShell;
        AdbTransport currentAdb;
        synchronized (lifecycleLock) {
            currentControl = control;
            currentVideo = video;
            currentShell = shell;
            currentAdb = adb;
            control = null;
            video = null;
            shell = null;
            adb = null;
            controlWriter = null;
        }
        closeQuietly(currentControl);
        closeQuietly(currentVideo);
        closeQuietly(currentShell);
        closeQuietly(serverAsset);
        if (currentAdb != null) {
            currentAdb.close();
        }
    }

    private static void closeQuietly(Closeable closeable) {
        if (closeable == null) {
            return;
        }
        try {
            closeable.close();
        } catch (IOException ignored) {
        }
    }

    private interface ControlAction {
        void execute() throws IOException;
    }

    private static int readIntBE(AdbTransport.AdbStream stream) throws IOException {
        byte[] data = new byte[4];
        stream.readFully(data, 0, 4);
        return readIntBE(data, 0);
    }

    private static int readIntBE(byte[] data, int offset) {
        return ((data[offset] & 0xff) << 24) | ((data[offset + 1] & 0xff) << 16)
                | ((data[offset + 2] & 0xff) << 8) | (data[offset + 3] & 0xff);
    }

    private static long readLongBE(byte[] data, int offset) {
        long value = 0;
        for (int i = 0; i < 8; ++i) {
            value = (value << 8) | (data[offset + i] & 0xffL);
        }
        return value;
    }

    private static int readIntLE(byte[] data, int offset) {
        return (data[offset] & 0xff) | ((data[offset + 1] & 0xff) << 8)
                | ((data[offset + 2] & 0xff) << 16) | ((data[offset + 3] & 0xff) << 24);
    }

    private static void writeIntLE(byte[] data, int offset, int value) {
        data[offset] = (byte) value;
        data[offset + 1] = (byte) (value >> 8);
        data[offset + 2] = (byte) (value >> 16);
        data[offset + 3] = (byte) (value >> 24);
    }
}
