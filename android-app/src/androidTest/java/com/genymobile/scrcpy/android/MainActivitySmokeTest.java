package com.genymobile.scrcpy.android;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withHint;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public final class MainActivitySmokeTest {
    @Rule
    public ActivityScenarioRule<MainActivity> activityRule =
            new ActivityScenarioRule<>(MainActivity.class);

    @Test
    public void mainScreenOpensAddConnectionEditor() {
        onView(withId(R.id.connection_list)).check(matches(isDisplayed()));
        onView(withText(R.string.add_connection))
                .check(matches(isDisplayed()))
                .perform(click());
        onView(withText(R.string.add_connection_title)).check(matches(isDisplayed()));
        onView(withHint(R.string.connection_name_hint)).check(matches(isDisplayed()));
    }
}
