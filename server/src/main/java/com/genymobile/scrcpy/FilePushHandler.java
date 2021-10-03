package com.genymobile.scrcpy;

import org.java_websocket.WebSocket;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.HashMap;

public final class FilePushHandler {
    private static final int NEW_PUSH_ID = 1;
    private static final int NO_ERROR = 0;
    private static final int ERROR_INVALID_NAME = -1;
    private static final int ERROR_NO_SPACE = -2;
    private static final int ERROR_FAILED_TO_DELETE = -3;
    private static final int ERROR_FAILED_TO_CREATE = -4;
    private static final int ERROR_FILE_NOT_FOUND = -5;
    private static final int ERROR_FAILED_TO_WRITE = -6;
    private static final int ERROR_FILE_IS_BUSY = -7;
    private static final int ERROR_INVALID_STATE = -8;
    private static final int ERROR_UNKNOWN_ID = -9;
    private static final int ERROR_NO_FREE_ID = -10;
    private static final int ERROR_INCORRECT_SIZE = -11;

    private static final String PUSH_PATH = "/data/local/tmp";

    private FilePushHandler() {
    }

    private static final class FilePush {
        private static final HashMap<String, FilePush> INSTANCES_BY_NAME = new HashMap<>();
        private static final HashMap<Short, FilePush> INSTANCES_BY_ID = new HashMap<>();
        private static short nextPushId = 0;

        private final FileOutputStream stream;
        private final WebSocket conn;
        private final short pushId;
        private final String fileName;
        private final long fileSize;
        private long processedBytes = 0;


        FilePush(short pushId, long fileSize, String fileName, FileOutputStream stream, WebSocket conn) {
            this.pushId = pushId;
            this.fileSize = fileSize;
            this.fileName = fileName;
            this.stream = stream;
            this.conn = conn;
            INSTANCES_BY_ID.put(pushId, this);
            INSTANCES_BY_NAME.put(fileName, this);
        }
        public static FilePush getInstance(String fileName) {
            return INSTANCES_BY_NAME.get(fileName);
        }
        public static FilePush getInstance(short id) {
            return INSTANCES_BY_ID.get(id);
        }
        public static short getNextPushId() {
            short current = nextPushId;
            while (INSTANCES_BY_ID.containsKey(++nextPushId)) {
                if (nextPushId == Short.MAX_VALUE) {
                    nextPushId = 0;
                }
                if (nextPushId == current) {
                    return -1;
                }
            }
            return nextPushId;
        }
        public static void releaseByConnection(WebSocket conn) {
            ArrayList<FilePush> related = new ArrayList<>();
            for (FilePush filePush: INSTANCES_BY_ID.values()) {
                if (filePush.conn.equals(conn)) {
                    related.add(filePush);
                }
            }
            for (FilePush filePush: related) {
                try {
                    filePush.release();
                } catch (IOException e) {
                    Ln.w("Failed to release stream for file: \"" + filePush.getFileName() + "\"");
                }
            }
        }
        public void write(byte[] chunk, int len) throws IOException {
            processedBytes += len;
            stream.write(chunk, 0, len);
        }
        public String getFileName() {
            return fileName;
        }
        public boolean isComplete() {
            return processedBytes == fileSize;
        }
        public void release() throws IOException {
            INSTANCES_BY_ID.remove(pushId);
            INSTANCES_BY_NAME.remove(fileName);
            this.stream.close();
        }
    }


    private static ByteBuffer pushFilePushResponse(short id, int result) {
        DeviceMessage msg = DeviceMessage.createPushResponse(id, result);
        return WebSocketConnection.deviceMessageToByteBuffer(msg);
    }

    private static FilePush checkPushId(WebSocket conn, ControlMessage msg) {
        short pushId = msg.getPushId();
        FilePush filePush = FilePush.getInstance(pushId);
        if (filePush == null) {
            conn.send(pushFilePushResponse(pushId, ERROR_UNKNOWN_ID));
        }
        return filePush;
    }

    private static void handleNew(WebSocket conn) {
        short newPushId = FilePush.getNextPushId();
        if (newPushId == -1) {
            conn.send(FilePushHandler.pushFilePushResponse(newPushId, ERROR_NO_FREE_ID));
        } else {
            conn.send(FilePushHandler.pushFilePushResponse(newPushId, NEW_PUSH_ID));
        }
    }
    private static void handleStart(WebSocket conn, ControlMessage msg) {
        short pushId = msg.getPushId();
        String fileName = msg.getFileName();
        int fileSize = msg.getFileSize();
        if (FilePush.getInstance(fileName) != null) {
            conn.send(pushFilePushResponse(pushId, ERROR_FILE_IS_BUSY));
            return;
        }
        if (fileName.contains("/")) {
            conn.send(pushFilePushResponse(pushId, ERROR_INVALID_NAME));
            return;
        }
        File file = new File(PUSH_PATH, fileName);

//        long freeSpace = file.getFreeSpace();
//        if (freeSpace < fileSize) {
//            sender.send(pushFilePushResponse(pushId, ERROR_NO_SPACE));
//            return;
//        }

        try {
            if (!file.createNewFile()) {
                if (!file.delete()) {
                    conn.send(pushFilePushResponse(pushId, ERROR_FAILED_TO_DELETE));
                    return;
                }
            }
        } catch (IOException e) {
            conn.send(pushFilePushResponse(pushId, ERROR_FAILED_TO_CREATE));
            return;
        }

        FileOutputStream stream;
        try {
            stream = new FileOutputStream(file);
        } catch (FileNotFoundException e) {
            conn.send(pushFilePushResponse(pushId, ERROR_FILE_NOT_FOUND));
            return;
        }

        new FilePush(pushId, fileSize, fileName, stream, conn);
        conn.send(pushFilePushResponse(pushId, NO_ERROR));

    }
    private static void handleAppend(WebSocket conn, ControlMessage msg) {
        FilePush filePush = checkPushId(conn, msg);
        if (filePush == null) {
            return;
        }
        short pushId = msg.getPushId();
        try {
            filePush.write(msg.getPushChunk(), msg.getPushChunkSize());
            conn.send(pushFilePushResponse(pushId, NO_ERROR));
        } catch (IOException e) {
            conn.send(pushFilePushResponse(pushId, ERROR_FAILED_TO_WRITE));
            try {
                filePush.release();
            } catch (IOException ex) {
                Ln.w("Failed to release stream for file: \"" + filePush.getFileName() + "\"");
            }
        }
    }
    private static void handleFinish(WebSocket conn, ControlMessage msg) {
        FilePush filePush = checkPushId(conn, msg);
        if (filePush == null) {
            return;
        }
        short pushId = msg.getPushId();
        if (filePush.isComplete()) {
            try {
                filePush.release();
                conn.send(pushFilePushResponse(pushId, NO_ERROR));
            } catch (IOException e) {
                conn.send(pushFilePushResponse(pushId, ERROR_FAILED_TO_WRITE));
            }
        } else {
            conn.send(pushFilePushResponse(pushId, ERROR_INCORRECT_SIZE));
        }
    }
    private static void handleCancel(WebSocket conn, ControlMessage msg) {
        FilePush filePush = checkPushId(conn, msg);
        if (filePush == null) {
            return;
        }
        short pushId = msg.getPushId();
        try {
            filePush.release();
            conn.send(pushFilePushResponse(pushId, NO_ERROR));
        } catch (IOException e) {
            conn.send(pushFilePushResponse(pushId, ERROR_FAILED_TO_WRITE));
        }
    }

    public static void cancelAllForConnection(WebSocket conn) {
        FilePush.releaseByConnection(conn);
    }

    public static void handlePush(WebSocket conn, ControlMessage msg) {
        int state = msg.getPushState();
        switch (state) {
            case ControlMessage.PUSH_STATE_NEW:
                handleNew(conn);
                break;
            case ControlMessage.PUSH_STATE_APPEND:
                handleAppend(conn, msg);
                break;
            case ControlMessage.PUSH_STATE_START:
                handleStart(conn, msg);
                break;
            case ControlMessage.PUSH_STATE_FINISH:
                handleFinish(conn, msg);
                break;
            case ControlMessage.PUSH_STATE_CANCEL:
                handleCancel(conn, msg);
                break;
            default:
                conn.send(pushFilePushResponse(msg.getPushId(), ERROR_INVALID_STATE));
        }
    }
}
