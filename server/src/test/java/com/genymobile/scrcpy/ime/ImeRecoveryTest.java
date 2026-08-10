package com.genymobile.scrcpy.ime;

import org.junit.Assert;
import org.junit.Test;

public class ImeRecoveryTest {
    @Test
    public void testFindFallbackIme() {
        String otherIme = "com.example/.InputMethod";
        String scrcpyIme = "com.genymobile.scrcpy.ime/.ScrcpyInputMethodService";
        String imeList = scrcpyIme + "\n" + otherIme + "\n";

        Assert.assertEquals(otherIme, ImeRecovery.findFallbackIme(imeList, scrcpyIme));
        Assert.assertEquals(otherIme, ImeRecovery.findFallbackIme(otherIme + "\r\n" + scrcpyIme, scrcpyIme));
        Assert.assertNull(ImeRecovery.findFallbackIme(scrcpyIme, scrcpyIme));
        Assert.assertNull(ImeRecovery.findFallbackIme("\n", scrcpyIme));
    }
}
