package com.genymobile.scrcpy.device;

import com.genymobile.scrcpy.video.VideoConstraints;

import org.junit.Assert;
import org.junit.Test;

public class SizeTest {

    private VideoConstraints createVideoConstraints(int maxSize, int alignment) {
        return new VideoConstraints(maxSize, alignment);
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
}
