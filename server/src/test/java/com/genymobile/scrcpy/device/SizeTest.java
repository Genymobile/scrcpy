package com.genymobile.scrcpy.device;

import com.genymobile.scrcpy.video.VideoConstraints;

import org.junit.Assert;
import org.junit.Test;

public class SizeTest {

    private VideoConstraints createVideoConstraints(int maxSize, int alignment) {
        Size maxCodecSize = new Size(4096, 4096);
        return createVideoConstraints(maxSize, alignment, maxCodecSize);
    }

    private VideoConstraints createVideoConstraints(int maxSize, int alignment, Size maxCodecSize) {
        return new VideoConstraints(maxSize, alignment, maxCodecSize, maxCodecSize, 0);
    }

    private VideoConstraints createVideoConstraints(int maxSize, int alignment, Size maxCodecSize, int minCodecSize) {
        return new VideoConstraints(maxSize, alignment, maxCodecSize, maxCodecSize, minCodecSize);
    }

    @Test
    public void testConstrainSize() {
        Size size = new Size(207, 209);

        Assert.assertEquals(size, size.constrain(createVideoConstraints(0, 1)));
        Assert.assertEquals(new Size(208, 210), size.constrain(createVideoConstraints(0, 2)));
        Assert.assertEquals(new Size(208, 208), size.constrain(createVideoConstraints(0, 4)));

        Size s = size.constrain(createVideoConstraints(208, 2));
        Assert.assertEquals(208, s.getHeight());
        Assert.assertTrue(s.getWidth() >= 206 && s.getWidth() <= 208 && s.getWidth() % 2 == 0);

        Assert.assertEquals(new Size(207 * 208 / 209, 208), size.constrain(createVideoConstraints(208, 2)));
        Assert.assertEquals(new Size(208, 208), size.constrain(createVideoConstraints(208, 4)));
    }

    @Test
    public void testConstrainSizeUnchanged() {
        Size size = new Size(256, 512);

        Assert.assertEquals(size, size.constrain(createVideoConstraints(0, 1)));
        Assert.assertEquals(size, size.constrain(createVideoConstraints(512, 1)));
        Assert.assertEquals(size, size.constrain(createVideoConstraints(515, 1)));
        Assert.assertEquals(size, size.constrain(createVideoConstraints(512, 16)));
        Assert.assertEquals(size, size.constrain(createVideoConstraints(515, 16)));
        Assert.assertEquals(size, size.constrain(createVideoConstraints(0, 16)));
        Assert.assertEquals(size, size.constrain(createVideoConstraints(4096, 16)));
    }

    @Test
    public void testConstrainMaxCodecSize() {
        Size maxCodecSize = new Size(400, 600);

        Size size = new Size(314, 617);
        Assert.assertEquals(new Size(314 * 600 / 617, 600), size.constrain(createVideoConstraints(0, 1, maxCodecSize)));
        Assert.assertEquals(new Size(314 * 550 / 617, 550), size.constrain(createVideoConstraints(550, 1, maxCodecSize)));

        size = new Size(512, 536);
        Assert.assertEquals(new Size(400, 536 * 400 / 512), size.constrain(createVideoConstraints(0, 1, maxCodecSize)));
        Assert.assertEquals(new Size(400, 536 * 400 / 512), size.constrain(createVideoConstraints(550, 1, maxCodecSize)));
        Assert.assertEquals(new Size(400, 536 * 400 / 512), size.constrain(createVideoConstraints(500, 1, maxCodecSize)));
        Assert.assertEquals(new Size(512 * 410 / 536, 410), size.constrain(createVideoConstraints(410, 1, maxCodecSize)));
    }

    @Test
    public void testConstrainMinCodecSize() {
        Size maxCodecSize = new Size(1024, 1024);

        Size size = new Size(800, 600);
        Assert.assertEquals(new Size(800, 600), size.constrain(createVideoConstraints(800, 1, maxCodecSize, 512)));
        Assert.assertEquals(new Size(512, 512), size.constrain(createVideoConstraints(400, 1, maxCodecSize, 512)));
    }
}
