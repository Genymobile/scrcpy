package com.genymobile.scrcpy;

import org.junit.Assert;
import org.junit.Test;

public class OptionsTest {
    @Test
    public void testUnlinkServerOption() {
        Options defaults = Options.parse(BuildConfig.VERSION_NAME);
        Assert.assertTrue(defaults.getCleanup());
        Assert.assertTrue(defaults.getUnlinkServer());

        Options keepServer = Options.parse(BuildConfig.VERSION_NAME, "cleanup=true", "unlink_server=false");
        Assert.assertTrue(keepServer.getCleanup());
        Assert.assertFalse(keepServer.getUnlinkServer());
    }
}
