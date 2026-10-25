package com.genymobile.scrcpy;

import org.junit.Assert;
import org.junit.Test;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.RandomAccessFile;
import java.nio.channels.FileLock;

public class FreeformSupportTest {

    private static final class MemoryStore implements FreeformSupport.SettingStore {
        private String value;
        private int writes;
        private int deletes;
        private boolean failAfterWrite;
        private boolean ignoreWrites;

        MemoryStore(String value) {
            this.value = value;
        }

        @Override
        public String get() {
            return value;
        }

        @Override
        public void put(String newValue) throws IOException {
            ++writes;
            if (!ignoreWrites) {
                value = newValue;
            }
            if (failAfterWrite) {
                failAfterWrite = false;
                throw new IOException("Write applied but response failed");
            }
        }

        @Override
        public void delete() {
            ++deletes;
            value = null;
        }
    }

    @Test
    public void testMissingValueIsDeletedOnExit() throws IOException {
        MemoryStore store = new MemoryStore(null);
        ByteArrayOutputStream ready = new ByteArrayOutputStream();
        FreeformSupport.runSession(store, new ByteArrayInputStream(new byte[0]), ready);
        Assert.assertArrayEquals(new byte[]{1}, ready.toByteArray());
        Assert.assertNull(store.value);
        Assert.assertEquals(1, store.writes);
        Assert.assertEquals(1, store.deletes);
    }

    @Test
    public void testOriginalValueIsRestoredExactly() throws IOException {
        for (String original : new String[]{"0", "2", "01", "", "invalid"}) {
            MemoryStore store = new MemoryStore(original);
            try (FreeformSupport.SettingSession session = new FreeformSupport.SettingSession(store)) {
                session.enable();
                Assert.assertEquals("1", store.value);
            }
            Assert.assertEquals(original, store.value);
            Assert.assertEquals(2, store.writes);
            Assert.assertEquals(0, store.deletes);
        }
    }

    @Test
    public void testAlreadyEnabledValueIsNotWritten() throws IOException {
        MemoryStore store = new MemoryStore("1");
        FreeformSupport.runSession(store, new ByteArrayInputStream(new byte[0]), new ByteArrayOutputStream());
        Assert.assertEquals("1", store.value);
        Assert.assertEquals(0, store.writes);
        Assert.assertEquals(0, store.deletes);
    }

    @Test
    public void testExternalChangeIsPreserved() throws IOException {
        for (String external : new String[]{null, "0", "2"}) {
            MemoryStore store = new MemoryStore("0");
            try (FreeformSupport.SettingSession session = new FreeformSupport.SettingSession(store)) {
                session.enable();
                store.value = external;
            }
            Assert.assertEquals(external, store.value);
            Assert.assertEquals(1, store.writes);
            Assert.assertEquals(0, store.deletes);
        }
    }

    @Test
    public void testFailedWriteStillRestoresIfApplied() {
        MemoryStore store = new MemoryStore(null);
        store.failAfterWrite = true;
        ByteArrayOutputStream ready = new ByteArrayOutputStream();
        Assert.assertThrows(IOException.class,
                () -> FreeformSupport.runSession(store, new ByteArrayInputStream(new byte[0]), ready));
        Assert.assertEquals(0, ready.size());
        Assert.assertNull(store.value);
        Assert.assertEquals(1, store.deletes);
    }

    @Test
    public void testIgnoredWriteDoesNotReportReady() {
        MemoryStore store = new MemoryStore("0");
        store.ignoreWrites = true;
        ByteArrayOutputStream ready = new ByteArrayOutputStream();
        Assert.assertThrows(IOException.class,
                () -> FreeformSupport.runSession(store, new ByteArrayInputStream(new byte[0]), ready));
        Assert.assertEquals(0, ready.size());
        Assert.assertEquals("0", store.value);
    }

    @Test
    public void testParentLostBeforeReadyRestoresOriginal() {
        MemoryStore store = new MemoryStore("0");
        OutputStream disconnectedParent = new OutputStream() {
            @Override
            public void write(int value) throws IOException {
                throw new IOException("Parent disconnected before readiness");
            }
        };
        Assert.assertThrows(IOException.class,
                () -> FreeformSupport.runSession(store, new ByteArrayInputStream(new byte[0]), disconnectedParent));
        Assert.assertEquals("0", store.value);
    }

    @Test
    public void testLifetimeReadFailureStillRestores() {
        MemoryStore store = new MemoryStore(null);
        InputStream failedLifetime = new InputStream() {
            @Override
            public int read() throws IOException {
                throw new IOException("Lifetime pipe failed");
            }
        };
        Assert.assertThrows(IOException.class,
                () -> FreeformSupport.runSession(store, failedLifetime, new ByteArrayOutputStream()));
        Assert.assertNull(store.value);
        Assert.assertEquals(1, store.deletes);
    }

    @Test
    public void testSettingRemainsEnabledUntilLifetimeEnds() throws IOException {
        MemoryStore store = new MemoryStore("0");
        InputStream lifetime = new InputStream() {
            private boolean read;

            @Override
            public int read() {
                Assert.assertEquals("1", store.value);
                if (!read) {
                    read = true;
                    return 0;
                }
                return -1;
            }
        };
        FreeformSupport.runSession(store, lifetime, new ByteArrayOutputStream());
        Assert.assertEquals("0", store.value);
    }

    @Test
    public void testFailedRestorationIsReported() throws IOException {
        MemoryStore store = new MemoryStore("0");
        FreeformSupport.SettingSession session = new FreeformSupport.SettingSession(store);
        session.enable();
        store.ignoreWrites = true;
        Assert.assertThrows(IOException.class, session::close);
        Assert.assertEquals("1", store.value);
    }

    @Test
    public void testConcurrentOwnerIsRejectedAndLockIsReleased() throws IOException {
        File path = File.createTempFile("scrcpy-freeform", ".lock");
        try (RandomAccessFile first = new RandomAccessFile(path, "rw");
                RandomAccessFile second = new RandomAccessFile(path, "rw")) {
            try (FileLock lock = FreeformSupport.acquireLock(first.getChannel())) {
                Assert.assertTrue(lock.isValid());
                Assert.assertThrows(IOException.class, () -> FreeformSupport.acquireLock(second.getChannel()));
            }
            try (FileLock lock = FreeformSupport.acquireLock(second.getChannel())) {
                Assert.assertTrue(lock.isValid());
            }
        } finally {
            Assert.assertTrue(path.delete());
        }
    }
}
