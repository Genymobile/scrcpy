package com.genymobile.scrcpy;

import org.junit.Assert;
import org.junit.Test;

import java.nio.charset.StandardCharsets;

public class StringUtilsTest {

    @Test
    public void testUtf8Truncate() {
        String s = "aÉbÔc";
        byte[] utf8 = s.getBytes(StandardCharsets.UTF_8);
        Assert.assertEquals(7, utf8.length);

        int count;

        count = StringUtils.getUtf8TruncationIndex(utf8, 1);
        Assert.assertEquals(1, count);

        count = StringUtils.getUtf8TruncationIndex(utf8, 2);
        Assert.assertEquals(1, count); // É is 2 bytes-wide

        count = StringUtils.getUtf8TruncationIndex(utf8, 3);
        Assert.assertEquals(3, count);

        count = StringUtils.getUtf8TruncationIndex(utf8, 4);
        Assert.assertEquals(4, count);

        count = StringUtils.getUtf8TruncationIndex(utf8, 5);
        Assert.assertEquals(4, count); // Ô is 2 bytes-wide

        count = StringUtils.getUtf8TruncationIndex(utf8, 6);
        Assert.assertEquals(6, count);

        count = StringUtils.getUtf8TruncationIndex(utf8, 7);
        Assert.assertEquals(7, count);

        count = StringUtils.getUtf8TruncationIndex(utf8, 8);
        Assert.assertEquals(7, count); // no more chars
    }
}
