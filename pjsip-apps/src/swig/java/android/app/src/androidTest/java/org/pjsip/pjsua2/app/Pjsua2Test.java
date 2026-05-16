/*
 * Copyright (C) 2025 Teluu Inc. (http://www.teluu.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
package org.pjsip.pjsua2.app;

import org.junit.Assert;
import org.pjsip.pjsua2.*;
import org.pjsip.pjsua2.app.BuildConfig;

import java.io.File;
import java.util.HashMap;

import java.util.concurrent.TimeUnit;

import android.Manifest;
import android.content.Context;
import android.os.Environment;
import android.util.Log;
import android.util.SparseBooleanArray;
import android.widget.ListView;

import androidx.test.espresso.IdlingPolicies;
import androidx.test.espresso.IdlingRegistry;
import androidx.test.espresso.IdlingResource;
import androidx.test.espresso.NoMatchingViewException;
import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.uiautomator.UiDevice;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.Espresso.onData;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static org.hamcrest.Matchers.anything;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import static org.junit.Assert.*;

class Holder<T> {
    T value;
}

@RunWith(AndroidJUnit4.class)
public class Pjsua2Test {
    final static String TAG = "PJSUA2Test";
    final static int TIMEOUT_RESOURCE = 10;

    private UiDevice device;

    @Rule
    public ActivityScenarioRule<MainActivity> activityScenarioRule =
            new ActivityScenarioRule<>(MainActivity.class);

    @Before
    public void setup() {
        device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());

        InstrumentationRegistry.getInstrumentation().getUiAutomation().grantRuntimePermission(
            InstrumentationRegistry.getInstrumentation().getTargetContext().getPackageName(),
            "android.permission.CAMERA"
        );
        InstrumentationRegistry.getInstrumentation().getUiAutomation().grantRuntimePermission(
            InstrumentationRegistry.getInstrumentation().getTargetContext().getPackageName(),
            "android.permission.RECORD_AUDIO"
        );

        Context appContext =
            InstrumentationRegistry.getInstrumentation().getTargetContext();
        assertEquals(BuildConfig.APPLICATION_ID, appContext.getPackageName());

        activityScenarioRule.getScenario().onActivity(activity -> {
            IdlingRegistry.getInstance().register(activity.getIdlingResource());
        });

        IdlingPolicies.setIdlingResourceTimeout(TIMEOUT_RESOURCE, TimeUnit.SECONDS);
    }

    @After
    public void unregisterIdlingResource() {
        activityScenarioRule.getScenario().onActivity(activity -> {
            IdlingRegistry.getInstance().unregister(activity.getIdlingResource());
        });
    }

    private static void checkIsDisplayed(final int viewId, final String viewName) {
        try {
            onView(withId(viewId)).check(matches(isDisplayed()));
        } catch (NoMatchingViewException e) {
            Assert.fail("View with ID R.id." + viewName + " was not found");
        }
    }

    public static void waitForView(final int viewId,
                                   final boolean isDisplayed) throws Exception
    {
        try {
            if (isDisplayed) {
                onView(withId(viewId)).check(matches(isDisplayed()));
            } else {
                onView(withId(viewId)).check(matches(
                              withEffectiveVisibility(Visibility.VISIBLE)));
            }
            return;
        } catch (NoMatchingViewException | AssertionError e) {
           Log.e(TAG, "Exception in waitForView " + e.getMessage());
        }

        throw new Exception("View with ID " + viewId + " not visible after " +
                            TIMEOUT_RESOURCE + " seconds");
    }
    private void takeScreenshot(final String fileName) throws Exception {
        UiDevice device = UiDevice.getInstance(
                                  InstrumentationRegistry.getInstrumentation());
        File path = new File(Environment.getExternalStoragePublicDirectory(
                             Environment.DIRECTORY_PICTURES).getAbsolutePath(),
                       "screenshots");
        if (!path.exists()) {
            if (!path.mkdirs()) {
                Log.e(TAG, "Failed to create "+ path.getAbsolutePath() +
                      " directory");
                return;
            }
        }
        Log.d(TAG, "Taking screenshot to : " + path.getAbsolutePath() +
              "//" + fileName);
        device.takeScreenshot(new File(path, fileName));
    }

    @Test
    public void addBuddy() throws Exception{
        String localUri = "sip:localhost";

        localUri += ":" + Integer.toString(BuildConfig.SIP_PORT);
        Log.d(TAG, "Starting addBuddy()");

        checkIsDisplayed(R.id.buttonAddBuddy, "buttonAddBuddy");

        activityScenarioRule.getScenario().onActivity(activity -> {
            activity.findViewById(R.id.buttonAddBuddy).setTag(BuildConfig.TEST_TAG);
        });
        onView(withId(R.id.buttonAddBuddy)).perform(click());

        Log.d(TAG, "Wait for the dialog to be shown");
        waitForView(R.id.editTextUri, true);

        Log.d(TAG, "Change the buddy URI");
        onView(withId(R.id.editTextUri)).perform(replaceText(localUri));

        Log.d(TAG, "Click confirm");
        onView(withText("OK")).perform(click());
        Log.d(TAG, "Done addBuddy()");
    }
    @Test
    public void callBuddy() throws Exception{
        ListView listView;
        final Holder<ListView> lvHolder = new Holder<>();

        Log.d(TAG, "Starting callBuddy()");

        String localUri = "sip:localhost";
        localUri += ":" + Integer.toString(BuildConfig.SIP_PORT);

        checkIsDisplayed(R.id.listViewBuddy, "listViewBuddy");

        onData(anything())
            .inAdapterView(withId(R.id.listViewBuddy))
            .atPosition(0)
            .perform(click());

        // Retrieve the ListView and the checked item position
        activityScenarioRule.getScenario().onActivity(activity -> {
            lvHolder.value = activity.findViewById(R.id.listViewBuddy);
        });
        listView = lvHolder.value;
        assert(listView != null);
        listView.setTag(BuildConfig.TEST_TAG);

        int checkedPosition = listView.getCheckedItemPosition();
        SparseBooleanArray checkedItems = listView.getCheckedItemPositions();
        if (checkedItems != null) {
            for (int i=0; i<checkedItems.size(); i++) {
                if (checkedItems.valueAt(i)) {
                    String item = listView.getAdapter().getItem(
                                             checkedItems.keyAt(i)). toString();
                }
            }
        }
        assertTrue("No item is checked",
                   checkedPosition != ListView.INVALID_POSITION);

        //Get the text of the checked item
        HashMap<String, String> checkedBuddy = (HashMap<String, String>)
                                 listView.getAdapter().getItem(checkedPosition);

        String checkedURI = checkedBuddy.get("uri");

        Log.d(TAG, "Selected URI is: " + checkedURI);
        assertEquals(localUri, checkedURI);

        checkIsDisplayed(R.id.buttonCall, "buttonCall");
        activityScenarioRule.getScenario().onActivity(activity -> {
            activity.findViewById(R.id.buttonCall).setTag(BuildConfig.TEST_TAG);
        });
        onView(withId(R.id.buttonCall)).perform(click());

        Log.d(TAG, "Wait for the incoming video to be shown");
        waitForView(R.id.surfaceIncomingVideo, false);

        onView(withId(R.id.surfaceIncomingVideo))
              .check(matches(withEffectiveVisibility(Visibility.VISIBLE)));
        try {
            takeScreenshot("make_call_sc.png");
        } catch (Exception e) {
            Log.e(TAG, e.getMessage());
        };
        Log.d(TAG, "Done callBuddy()");
    }

}
