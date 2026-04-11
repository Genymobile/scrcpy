package com.genymobile.scrcpy.device;

import com.genymobile.scrcpy.video.VideoConstraints;

import org.junit.Assert;
import org.junit.Test;

public class SizeTest {

    private VideoConstraints createVideoConstraints(int alignment) {
        return new VideoConstraints(alignment);
    }

    @Test
    public void testConstrainSize() {
        Size size = new Size(207, 209);

        Assert.assertEquals(size, size.constrain(0, createVideoConstraints(1)));
        Assert.assertEquals(new Size(208, 210), size.constrain(0, createVideoConstraints(2)));
        Assert.assertEquals(new Size(208, 208), size.constrain(0, createVideoConstraints(4)));

        Size s = size.constrain(208, createVideoConstraints(2));
        Assert.assertEquals(208, s.getHeight());
        Assert.assertTrue(s.getWidth() >= 206 && s.getWidth() <= 208 && s.getWidth() % 2 == 0);

        Assert.assertEquals(new Size(207 * 208 / 209, 208), size.constrain(208, createVideoConstraints(2)));
        Assert.assertEquals(new Size(208, 208), size.constrain(208, createVideoConstraints(4)));
    }

    @Test
    public void testConstrainSizeUnchanged() {
        Size size = new Size(256, 512);

        Assert.assertEquals(size, size.constrain(0, createVideoConstraints(1)));
        Assert.assertEquals(size, size.constrain(512, createVideoConstraints(1)));
        Assert.assertEquals(size, size.constrain(515, createVideoConstraints(1)));
        Assert.assertEquals(size, size.constrain(512, createVideoConstraints(16)));
        Assert.assertEquals(size, size.constrain(515, createVideoConstraints(16)));
        Assert.assertEquals(size, size.constrain(0, createVideoConstraints(16)));
        Assert.assertEquals(size, size.constrain(4096, createVideoConstraints(16)));
    }
}
