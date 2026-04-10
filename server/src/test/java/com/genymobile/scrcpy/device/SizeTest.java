package com.genymobile.scrcpy.device;

import org.junit.Assert;
import org.junit.Test;

public class SizeTest {
    @Test
    public void testConstrainSize() {
        Size size = new Size(207, 209);

        Assert.assertEquals(size, size.constrain(0, 1));
        Assert.assertEquals(new Size(208, 210), size.constrain(0, 2));
        Assert.assertEquals(new Size(208, 208), size.constrain(0, 4));

        Size s = size.constrain(208, 2);
        Assert.assertEquals(208, s.getHeight());
        Assert.assertTrue(s.getWidth() >= 206 && s.getWidth() <= 208 && s.getWidth() % 2 == 0);

        Assert.assertEquals(new Size(207 * 208 / 209, 208), size.constrain(208, 2));
        Assert.assertEquals(new Size(208, 208), size.constrain(208, 4));
    }

    @Test
    public void testConstrainSizeUnchanged() {
        Size size = new Size(256, 512);

        Assert.assertEquals(size, size.constrain(0, 1));
        Assert.assertEquals(size, size.constrain(512, 1));
        Assert.assertEquals(size, size.constrain(515, 1));
        Assert.assertEquals(size, size.constrain(512, 16));
        Assert.assertEquals(size, size.constrain(515, 16));
        Assert.assertEquals(size, size.constrain(0, 16));
        Assert.assertEquals(size, size.constrain(4096, 16));
    }
}
