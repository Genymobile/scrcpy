package com.genymobile.scrcpy.ime;

import org.junit.Assert;
import org.junit.Test;

import android.view.inputmethod.InputConnection;

import java.lang.reflect.Proxy;

public class ScrcpyInputMethodServiceTest {
    @Test
    public void testCommitUnicodeText() {
        String expected = "中文🙂 café";
        boolean[] called = {false};
        InputConnection connection = (InputConnection) Proxy.newProxyInstance(
                InputConnection.class.getClassLoader(),
                new Class<?>[]{InputConnection.class},
                (proxy, method, args) -> {
                    if ("commitText".equals(method.getName())) {
                        Assert.assertEquals(expected, args[0].toString());
                        Assert.assertEquals(1, args[1]);
                        called[0] = true;
                        return true;
                    }
                    return null;
                });

        Assert.assertTrue(ScrcpyInputMethodService.commitText(connection, expected));
        Assert.assertTrue(called[0]);
        Assert.assertFalse(ScrcpyInputMethodService.commitText(null, expected));
    }

    @Test
    public void testSetComposingText() {
        String expected = "ni";
        boolean[] called = {false};
        InputConnection connection = (InputConnection) Proxy.newProxyInstance(
                InputConnection.class.getClassLoader(),
                new Class<?>[]{InputConnection.class},
                (proxy, method, args) -> {
                    if ("setComposingText".equals(method.getName())) {
                        Assert.assertEquals(expected, args[0].toString());
                        Assert.assertEquals(1, args[1]);
                        called[0] = true;
                        return true;
                    }
                    return null;
                });

        Assert.assertTrue(ScrcpyInputMethodService.setComposingText(connection, expected));
        Assert.assertTrue(called[0]);
        Assert.assertFalse(ScrcpyInputMethodService.setComposingText(null, expected));
    }
}
