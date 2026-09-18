package com.genymobile.scrcpy.android;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

public final class AndroidClientValidationTest {
    @Test
    public void acceptsValidPackageNames() {
        assertTrue(AndroidClientValidation.isValidPackageName("com.example.target"));
        assertTrue(AndroidClientValidation.isValidPackageName("a1.b_2"));
    }

    @Test
    public void rejectsPackageNamesThatCouldBreakShellArguments() {
        assertFalse(AndroidClientValidation.isValidPackageName("com.example;id"));
        assertFalse(AndroidClientValidation.isValidPackageName("com.example app"));
        assertFalse(AndroidClientValidation.isValidPackageName("1com.example"));
        assertFalse(AndroidClientValidation.isValidPackageName("com.example/target"));
    }

    @Test
    public void validatesConnectionProfiles() {
        assertTrue(AndroidClientValidation.isValidProfile(
                "Target", "192.168.1.20", 5555, "com.example.target"));
        assertTrue(AndroidClientValidation.isValidProfile(
                "Target", "phone.local", 443, ""));
        assertFalse(AndroidClientValidation.isValidProfile(
                "", "192.168.1.20", 5555, ""));
        assertFalse(AndroidClientValidation.isValidProfile(
                "   ", "192.168.1.20", 5555, ""));
        assertFalse(AndroidClientValidation.isValidProfile(
                "Target", "192.168.1.20;id", 5555, ""));
        assertFalse(AndroidClientValidation.isValidProfile(
                "Target", "192.168.1.20", 0, ""));
    }
}
