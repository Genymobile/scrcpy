package com.genymobile.scrcpy.control;

import com.genymobile.scrcpy.model.Point;
import com.genymobile.scrcpy.model.Size;
import com.genymobile.scrcpy.util.AffineMatrix;

import org.junit.Assert;
import org.junit.Test;

public class PositionMapperTest {
    @Test
    public void testUnmap() {
        Size videoSize = new Size(100, 200);
        AffineMatrix videoToDevice = new AffineMatrix(2, 0, 0, 3, 10, 20);
        PositionMapper mapper = new PositionMapper(videoSize, videoToDevice);

        Assert.assertEquals(new Point(25, 50), mapper.unmap(new Point(60, 170)));
        Assert.assertNull(mapper.unmap(new Point(8, 20)));
    }

    @Test
    public void testUnmapIdentity() {
        PositionMapper mapper = new PositionMapper(new Size(100, 200), null);
        Assert.assertEquals(new Point(25, 50), mapper.unmap(new Point(25, 50)));
    }

    @Test
    public void testUnmapNonInvertible() {
        PositionMapper mapper = new PositionMapper(
                new Size(100, 200), new AffineMatrix(0, 0, 0, 0, 0, 0));
        Assert.assertNull(mapper.unmap(new Point(25, 50)));
    }
}
