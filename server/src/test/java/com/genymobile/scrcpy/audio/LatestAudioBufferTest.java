package com.genymobile.scrcpy.audio;

import org.junit.Assert;
import org.junit.Test;

public class LatestAudioBufferTest {

    @Test
    public void testRoundTripAcrossRingBoundary() throws Exception {
        LatestAudioBuffer buffer = new LatestAudioBuffer(6);
        buffer.write(new byte[] {1, 2, 3, 4});

        byte[] first = new byte[3];
        Assert.assertEquals(3, buffer.read(first));
        Assert.assertArrayEquals(new byte[] {1, 2, 3}, first);

        buffer.write(new byte[] {5, 6, 7, 8});
        byte[] second = new byte[5];
        Assert.assertEquals(5, buffer.read(second));
        Assert.assertArrayEquals(new byte[] {4, 5, 6, 7, 8}, second);
    }

    @Test
    public void testOverflowKeepsNewestSamples() throws Exception {
        LatestAudioBuffer buffer = new LatestAudioBuffer(4);
        buffer.write(new byte[] {1, 2, 3});
        buffer.write(new byte[] {4, 5, 6});

        byte[] actual = new byte[4];
        Assert.assertEquals(4, buffer.read(actual));
        Assert.assertArrayEquals(new byte[] {3, 4, 5, 6}, actual);
    }

    @Test
    public void testOversizedWriteKeepsTail() throws Exception {
        LatestAudioBuffer buffer = new LatestAudioBuffer(3);
        buffer.write(new byte[] {1, 2, 3, 4, 5});

        byte[] actual = new byte[3];
        Assert.assertEquals(3, buffer.read(actual));
        Assert.assertArrayEquals(new byte[] {3, 4, 5}, actual);
    }

    @Test
    public void testClosedEmptyBufferReturnsEndOfStream() throws Exception {
        LatestAudioBuffer buffer = new LatestAudioBuffer(4);
        buffer.close();
        Assert.assertEquals(-1, buffer.read(new byte[4]));
    }
}
