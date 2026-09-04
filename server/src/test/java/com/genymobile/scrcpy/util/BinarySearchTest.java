package com.genymobile.scrcpy.util;

import org.junit.Assert;
import org.junit.Test;

public class BinarySearchTest {

    @Test
    public void testFindHighestTrue() {
        Assert.assertEquals(4, BinarySearch.findHighestTrue(5, 15, i -> false));
        Assert.assertEquals(5, BinarySearch.findHighestTrue(5, 15, i -> i < 6));
        Assert.assertEquals(6, BinarySearch.findHighestTrue(5, 15, i -> i < 7));
        Assert.assertEquals(7, BinarySearch.findHighestTrue(5, 15, i -> i < 8));
        Assert.assertEquals(8, BinarySearch.findHighestTrue(5, 15, i -> i < 9));
        Assert.assertEquals(9, BinarySearch.findHighestTrue(5, 15, i -> i < 10));
        Assert.assertEquals(10, BinarySearch.findHighestTrue(5, 15, i -> i < 11));
        Assert.assertEquals(11, BinarySearch.findHighestTrue(5, 15, i -> i < 12));
        Assert.assertEquals(12, BinarySearch.findHighestTrue(5, 15, i -> i < 13));
        Assert.assertEquals(13, BinarySearch.findHighestTrue(5, 15, i -> i < 14));
        Assert.assertEquals(14, BinarySearch.findHighestTrue(5, 15, i -> i < 15));
        Assert.assertEquals(15, BinarySearch.findHighestTrue(5, 15, i -> true));
    }
}
