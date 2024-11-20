package com.genymobile.scrcpy;

import com.genymobile.scrcpy.device.NewDisplay;
import com.genymobile.scrcpy.device.Size;

import org.junit.Assert;
import org.junit.Test;

public class OptionsTest {

    @Test
    public void testParseNewDisplayEmpty() {
        NewDisplay newDisplay = Options.parseNewDisplay("");
        Assert.assertFalse(newDisplay.hasExplicitSize());
        Assert.assertFalse(newDisplay.hasExplicitDpi());
        Assert.assertFalse(newDisplay.hasExplicitFps());
        Assert.assertNull(newDisplay.getSize());
        Assert.assertEquals(0, newDisplay.getDpi());
        Assert.assertEquals(0, newDisplay.getFps(), 0);
    }

    @Test
    public void testParseNewDisplaySizeOnly() {
        NewDisplay newDisplay = Options.parseNewDisplay("1920x1080");
        Assert.assertTrue(newDisplay.hasExplicitSize());
        Assert.assertFalse(newDisplay.hasExplicitDpi());
        Assert.assertFalse(newDisplay.hasExplicitFps());
        Assert.assertEquals(new Size(1920, 1080), newDisplay.getSize());
        Assert.assertEquals(0, newDisplay.getDpi());
        Assert.assertEquals(0, newDisplay.getFps(), 0);
    }

    @Test
    public void testParseNewDisplayDpiOnly() {
        NewDisplay newDisplay = Options.parseNewDisplay("/240");
        Assert.assertFalse(newDisplay.hasExplicitSize());
        Assert.assertTrue(newDisplay.hasExplicitDpi());
        Assert.assertFalse(newDisplay.hasExplicitFps());
        Assert.assertNull(newDisplay.getSize());
        Assert.assertEquals(240, newDisplay.getDpi());
        Assert.assertEquals(0, newDisplay.getFps(), 0);
    }

    @Test
    public void testParseNewDisplayFpsOnly() {
        NewDisplay newDisplay = Options.parseNewDisplay("@30");
        Assert.assertFalse(newDisplay.hasExplicitSize());
        Assert.assertFalse(newDisplay.hasExplicitDpi());
        Assert.assertTrue(newDisplay.hasExplicitFps());
        Assert.assertNull(newDisplay.getSize());
        Assert.assertEquals(0, newDisplay.getDpi());
        Assert.assertEquals(30, newDisplay.getFps(), 0);
    }

    @Test
    public void testParseNewDisplaySizeAndDpi() {
        NewDisplay newDisplay = Options.parseNewDisplay("1920x1080/240");
        Assert.assertTrue(newDisplay.hasExplicitSize());
        Assert.assertTrue(newDisplay.hasExplicitDpi());
        Assert.assertFalse(newDisplay.hasExplicitFps());
        Assert.assertEquals(new Size(1920, 1080), newDisplay.getSize());
        Assert.assertEquals(240, newDisplay.getDpi());
        Assert.assertEquals(0, newDisplay.getFps(), 0);
    }

    @Test
    public void testParseNewDisplaySizeAndFps() {
        NewDisplay newDisplay = Options.parseNewDisplay("1920x1080@30");
        Assert.assertTrue(newDisplay.hasExplicitSize());
        Assert.assertFalse(newDisplay.hasExplicitDpi());
        Assert.assertTrue(newDisplay.hasExplicitFps());
        Assert.assertEquals(new Size(1920, 1080), newDisplay.getSize());
        Assert.assertEquals(0, newDisplay.getDpi());
        Assert.assertEquals(30, newDisplay.getFps(), 0);
    }

    @Test
    public void testParseNewDisplaySizeAndDpiAndFps() {
        NewDisplay newDisplay = Options.parseNewDisplay("1920x1080/240@30");
        Assert.assertTrue(newDisplay.hasExplicitSize());
        Assert.assertTrue(newDisplay.hasExplicitDpi());
        Assert.assertTrue(newDisplay.hasExplicitFps());
        Assert.assertEquals(new Size(1920, 1080), newDisplay.getSize());
        Assert.assertEquals(240, newDisplay.getDpi());
        Assert.assertEquals(30, newDisplay.getFps(), 0);
    }

    @Test
    public void testParseNewDisplaySizeAndFpsAndDpi() {
        NewDisplay newDisplay = Options.parseNewDisplay("1920x1080@30/240");
        Assert.assertTrue(newDisplay.hasExplicitSize());
        Assert.assertTrue(newDisplay.hasExplicitDpi());
        Assert.assertTrue(newDisplay.hasExplicitFps());
        Assert.assertEquals(new Size(1920, 1080), newDisplay.getSize());
        Assert.assertEquals(240, newDisplay.getDpi());
        Assert.assertEquals(30, newDisplay.getFps(), 0);
    }

    @Test
    public void testParseNewDisplayDpiAndFps() {
        NewDisplay newDisplay = Options.parseNewDisplay("/240@30");
        Assert.assertFalse(newDisplay.hasExplicitSize());
        Assert.assertTrue(newDisplay.hasExplicitDpi());
        Assert.assertTrue(newDisplay.hasExplicitFps());
        Assert.assertNull(newDisplay.getSize());
        Assert.assertEquals(240, newDisplay.getDpi());
        Assert.assertEquals(30, newDisplay.getFps(), 0);
    }

    @Test
    public void testParseNewDisplayFpsAndDpi() {
        NewDisplay newDisplay = Options.parseNewDisplay("@30/240");
        Assert.assertFalse(newDisplay.hasExplicitSize());
        Assert.assertTrue(newDisplay.hasExplicitDpi());
        Assert.assertTrue(newDisplay.hasExplicitFps());
        Assert.assertNull(newDisplay.getSize());
        Assert.assertEquals(240, newDisplay.getDpi());
        Assert.assertEquals(30, newDisplay.getFps(), 0);
    }
}
