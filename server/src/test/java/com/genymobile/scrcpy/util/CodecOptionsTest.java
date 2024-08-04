package com.genymobile.scrcpy.util;

import org.junit.Assert;
import org.junit.Test;

import java.util.List;

public class CodecOptionsTest {

    @Test
    public void testIntegerImplicit() {
        List<CodecOption> codecOptions = CodecOption.parse("some_key=5");

        Assert.assertEquals(1, codecOptions.size());

        CodecOption option = codecOptions.get(0);
        Assert.assertEquals("some_key", option.getKey());
        Assert.assertEquals(5, option.getValue());
    }

    @Test
    public void testInteger() {
        List<CodecOption> codecOptions = CodecOption.parse("some_key:int=5");

        Assert.assertEquals(1, codecOptions.size());

        CodecOption option = codecOptions.get(0);
        Assert.assertEquals("some_key", option.getKey());
        Assert.assertTrue(option.getValue() instanceof Integer);
        Assert.assertEquals(5, option.getValue());
    }

    @Test
    public void testLong() {
        List<CodecOption> codecOptions = CodecOption.parse("some_key:long=5");

        Assert.assertEquals(1, codecOptions.size());

        CodecOption option = codecOptions.get(0);
        Assert.assertEquals("some_key", option.getKey());
        Assert.assertTrue(option.getValue() instanceof Long);
        Assert.assertEquals(5L, option.getValue());
    }

    @Test
    public void testFloat() {
        List<CodecOption> codecOptions = CodecOption.parse("some_key:float=4.5");

        Assert.assertEquals(1, codecOptions.size());

        CodecOption option = codecOptions.get(0);
        Assert.assertEquals("some_key", option.getKey());
        Assert.assertTrue(option.getValue() instanceof Float);
        Assert.assertEquals(4.5f, option.getValue());
    }

    @Test
    public void testString() {
        List<CodecOption> codecOptions = CodecOption.parse("some_key:string=some_value");

        Assert.assertEquals(1, codecOptions.size());

        CodecOption option = codecOptions.get(0);
        Assert.assertEquals("some_key", option.getKey());
        Assert.assertTrue(option.getValue() instanceof String);
        Assert.assertEquals("some_value", option.getValue());
    }

    @Test
    public void testStringEscaped() {
        List<CodecOption> codecOptions = CodecOption.parse("some_key:string=warning\\,this_is_not=a_new_key");

        Assert.assertEquals(1, codecOptions.size());

        CodecOption option = codecOptions.get(0);
        Assert.assertEquals("some_key", option.getKey());
        Assert.assertTrue(option.getValue() instanceof String);
        Assert.assertEquals("warning,this_is_not=a_new_key", option.getValue());
    }

    @Test
    public void testList() {
        List<CodecOption> codecOptions = CodecOption.parse("a=1,b:int=2,c:long=3,d:float=4.5,e:string=a\\,b=c");

        Assert.assertEquals(5, codecOptions.size());

        CodecOption option;

        option = codecOptions.get(0);
        Assert.assertEquals("a", option.getKey());
        Assert.assertTrue(option.getValue() instanceof Integer);
        Assert.assertEquals(1, option.getValue());

        option = codecOptions.get(1);
        Assert.assertEquals("b", option.getKey());
        Assert.assertTrue(option.getValue() instanceof Integer);
        Assert.assertEquals(2, option.getValue());

        option = codecOptions.get(2);
        Assert.assertEquals("c", option.getKey());
        Assert.assertTrue(option.getValue() instanceof Long);
        Assert.assertEquals(3L, option.getValue());

        option = codecOptions.get(3);
        Assert.assertEquals("d", option.getKey());
        Assert.assertTrue(option.getValue() instanceof Float);
        Assert.assertEquals(4.5f, option.getValue());

        option = codecOptions.get(4);
        Assert.assertEquals("e", option.getKey());
        Assert.assertTrue(option.getValue() instanceof String);
        Assert.assertEquals("a,b=c", option.getValue());
    }
}
