package com.genymobile.scrcpy;

import org.junit.Assert;
import org.junit.Test;

public class BinaryTest {

    @Test
    public void testU16FixedPointToFloat() {
        final float delta = 0.0f; // on these values, there MUST be no rounding error
        Assert.assertEquals(0.0f, Binary.u16FixedPointToFloat((short) 0), delta);
        Assert.assertEquals(0.03125f, Binary.u16FixedPointToFloat((short) 0x800), delta);
        Assert.assertEquals(0.0625f, Binary.u16FixedPointToFloat((short) 0x1000), delta);
        Assert.assertEquals(0.125f, Binary.u16FixedPointToFloat((short) 0x2000), delta);
        Assert.assertEquals(0.25f, Binary.u16FixedPointToFloat((short) 0x4000), delta);
        Assert.assertEquals(0.5f, Binary.u16FixedPointToFloat((short) 0x8000), delta);
        Assert.assertEquals(0.75f, Binary.u16FixedPointToFloat((short) 0xc000), delta);
        Assert.assertEquals(1.0f, Binary.u16FixedPointToFloat((short) 0xffff), delta);
    }

    @Test
    public void testI16FixedPointToFloat() {
        final float delta = 0.0f; // on these values, there MUST be no rounding error

        Assert.assertEquals(0.0f, Binary.i16FixedPointToFloat((short) 0), delta);
        Assert.assertEquals(0.03125f, Binary.i16FixedPointToFloat((short) 0x400), delta);
        Assert.assertEquals(0.0625f, Binary.i16FixedPointToFloat((short) 0x800), delta);
        Assert.assertEquals(0.125f, Binary.i16FixedPointToFloat((short) 0x1000), delta);
        Assert.assertEquals(0.25f, Binary.i16FixedPointToFloat((short) 0x2000), delta);
        Assert.assertEquals(0.5f, Binary.i16FixedPointToFloat((short) 0x4000), delta);
        Assert.assertEquals(0.75f, Binary.i16FixedPointToFloat((short) 0x6000), delta);
        Assert.assertEquals(1.0f, Binary.i16FixedPointToFloat((short) 0x7fff), delta);

        Assert.assertEquals(-0.03125f, Binary.i16FixedPointToFloat((short) -0x400), delta);
        Assert.assertEquals(-0.0625f, Binary.i16FixedPointToFloat((short) -0x800), delta);
        Assert.assertEquals(-0.125f, Binary.i16FixedPointToFloat((short) -0x1000), delta);
        Assert.assertEquals(-0.25f, Binary.i16FixedPointToFloat((short) -0x2000), delta);
        Assert.assertEquals(-0.5f, Binary.i16FixedPointToFloat((short) -0x4000), delta);
        Assert.assertEquals(-0.75f, Binary.i16FixedPointToFloat((short) -0x6000), delta);
        Assert.assertEquals(-1.0f, Binary.i16FixedPointToFloat((short) -0x8000), delta);
    }
}
