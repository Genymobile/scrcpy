package com.genymobile.scrcpy;

import android.graphics.Rect;
import android.os.Build;
import android.view.accessibility.AccessibilityNodeInfo;

public class AccessibilityNodeInfoHelper {
    /**
     * Returns the node's bounds clipped to the size of the display
     *
     * @param node
     * @param width pixel width of the display
     * @param height pixel height of the display
     * @return null if node is null, else a Rect containing visible bounds
     */
    static Rect getVisibleBoundsInScreen(AccessibilityNodeInfo node, int width, int height) {
        if (node == null) {
            return null;
        }
        // targeted node's bounds
        Rect nodeRect = new Rect();
        node.getBoundsInScreen(nodeRect);

        Rect displayRect = new Rect();
        displayRect.top = 0;
        displayRect.left = 0;
        displayRect.right = width;
        displayRect.bottom = height;

        nodeRect.intersect(displayRect);

        // On platforms that give us access to the node's window
        if (android.os.Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            // Trim any portion of the bounds that are outside the window
            Rect window = new Rect();
            if (node.getWindow() != null) {
                node.getWindow().getBoundsInScreen(window);
                nodeRect.intersect(window);
            }
        }

        return nodeRect;
    }
}
