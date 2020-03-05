package com.genymobile.scrcpy;

import android.util.Xml;
import android.view.accessibility.AccessibilityNodeInfo;

import org.xmlpull.v1.XmlSerializer;

import java.io.IOException;
import java.io.OutputStream;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import android.os.Build;

import android.app.UiAutomation;
import android.os.Looper;
import android.os.HandlerThread;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityWindowInfo;

import java.lang.reflect.Constructor;
import java.lang.reflect.Method;

import java.io.ByteArrayOutputStream;
import java.io.FileDescriptor;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

import android.os.Handler;

public class AccessibilityNodeInfoDumper {
    private UiAutomation mUiAutomation;
    private HandlerThread mHandlerThread;

    private Handler handler;
    private final Device device;
    private final DesktopConnection connection;
    private AccessibilityEventListenerImpl accessibilityEventListenerImpl;

    private static final String[] NAF_EXCLUDED_CLASSES = new String[]{
            android.widget.GridView.class.getName(), android.widget.GridLayout.class.getName(),
            android.widget.ListView.class.getName(), android.widget.TableLayout.class.getName()
    };

    public AccessibilityNodeInfoDumper(Handler handler, Device device, DesktopConnection connection) {
        this.handler = handler;
        this.device = device;
        this.connection = connection;
        this.accessibilityEventListenerImpl = new AccessibilityEventListenerImpl();
    }

    private class AccessibilityEventListenerImpl implements UiAutomation.OnAccessibilityEventListener {
        @Override
        public void onAccessibilityEvent(AccessibilityEvent event) {
//            Ln.i("EventType: " + AccessibilityEvent.eventTypeToString(event.getEventType()));
            switch (event.getEventType()) {
                case AccessibilityEvent.TYPE_ANNOUNCEMENT:
                case AccessibilityEvent.TYPE_ASSIST_READING_CONTEXT:
                case AccessibilityEvent.TYPE_GESTURE_DETECTION_END:
                case AccessibilityEvent.TYPE_GESTURE_DETECTION_START:
                case AccessibilityEvent.TYPE_NOTIFICATION_STATE_CHANGED:
                case AccessibilityEvent.TYPE_TOUCH_EXPLORATION_GESTURE_END:
                case AccessibilityEvent.TYPE_TOUCH_EXPLORATION_GESTURE_START:
                case AccessibilityEvent.TYPE_TOUCH_INTERACTION_END:
                case AccessibilityEvent.TYPE_TOUCH_INTERACTION_START:
                case AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED:
                case AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED:
                case AccessibilityEvent.TYPE_VIEW_CLICKED:
                case AccessibilityEvent.TYPE_VIEW_CONTEXT_CLICKED:
                case AccessibilityEvent.TYPE_VIEW_FOCUSED:
                case AccessibilityEvent.TYPE_VIEW_HOVER_ENTER:
                case AccessibilityEvent.TYPE_VIEW_HOVER_EXIT:
                case AccessibilityEvent.TYPE_VIEW_LONG_CLICKED:
                case AccessibilityEvent.TYPE_VIEW_SCROLLED:
                case AccessibilityEvent.TYPE_VIEW_SELECTED:
                case AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED:
                case AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED:
                case AccessibilityEvent.TYPE_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY:
                case AccessibilityEvent.TYPE_WINDOWS_CHANGED:
//                case AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED:
                case AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED:
                    ByteArrayOutputStream bos = new ByteArrayOutputStream();
                    try {
                        dumpWindowHierarchy(device, bos);
                    } catch (IOException e) {
                        e.printStackTrace();
                        Ln.e("onAccessibilityEvent error: " + e.getMessage());
                    } finally {
                        if (bos != null) {
                            try {
                                bos.close();
                            } catch (IOException e) {
                            }
                        }
                    }
                    Ln.i("dumpHierarchy size: " + bos.toByteArray().length);
                    ByteBuffer b = ByteBuffer.allocate(4 + bos.toByteArray().length);
                    b.order(ByteOrder.LITTLE_ENDIAN);
                    b.putInt(bos.toByteArray().length);
                    b.put(bos.toByteArray());
                    byte[] hierarchy = b.array();
                    try {
                        IO.writeFully(connection.getVideoChannel(), hierarchy, 0, hierarchy.length);// IOException
                    } catch (IOException e) {
                        Common.stopScrcpy(handler, "hierarchy");
                    }
                    break;
            }
        }
    }

    public void start() {
        Object connection = null;
        mHandlerThread = new HandlerThread("ScrcpyUiAutomationHandlerThread");
        mHandlerThread.start();
        try {
            Class<?> UiAutomationConnection = Class.forName("android.app.UiAutomationConnection");
            Constructor<?> newInstance = UiAutomationConnection.getDeclaredConstructor();
            newInstance.setAccessible(true);
            connection = newInstance.newInstance();
            Class<?> IUiAutomationConnection = Class.forName("android.app.IUiAutomationConnection");
            Constructor<?> newUiAutomation = UiAutomation.class.getDeclaredConstructor(Looper.class, IUiAutomationConnection);
            mUiAutomation = (UiAutomation) newUiAutomation.newInstance(mHandlerThread.getLooper(), connection);
            Method connect = UiAutomation.class.getDeclaredMethod("connect");
            connect.invoke(mUiAutomation);
            Ln.i("mUiAutomation: " + mUiAutomation);
            if (mUiAutomation != null) {
                mUiAutomation.setOnAccessibilityEventListener(accessibilityEventListenerImpl);
            }
        } catch (Exception e) {
            e.printStackTrace();
            Ln.e("AccessibilityNodeInfoDumper start: " + e.getMessage());
            stop();
        }
    }

    public void stop() {
        if (mUiAutomation != null) {
            mUiAutomation.setOnAccessibilityEventListener(null);
            try {
                Method disconnect = UiAutomation.class.getDeclaredMethod("disconnect");
                disconnect.invoke(mUiAutomation);
            } catch (Exception e) {
                e.printStackTrace();
                Ln.e("disconnect: " + e.getMessage());
            }
            mUiAutomation = null;
        }
        if (mHandlerThread != null) {
            mHandlerThread.quit();
        }
    }

    private void restart() {
        stop();
        start();
    }

    public void dumpWindowHierarchy(Device device, OutputStream out) throws IOException {
        XmlSerializer serializer = Xml.newSerializer();
//        serializer.setFeature("http://xmlpull.org/v1/doc/features.html#indent-output", true);
        serializer.setOutput(out, "UTF-8");

        serializer.startDocument("UTF-8", true);
        serializer.startTag("", "hierarchy"); // TODO(allenhair): Should we use a namespace?
        serializer.attribute("", "rotation", Integer.toString(device.getRotation()));

        int width = device.getScreenInfo().getContentRect().width();
        int height = device.getScreenInfo().getContentRect().height();

        if (mUiAutomation == null) {
            restart();
        }

        if (mUiAutomation != null) {
            Set<AccessibilityNodeInfo> roots = new HashSet();
            // Start with the active window, which seems to sometimes be missing from the list returned
            // by the UiAutomation.
            AccessibilityNodeInfo activeRoot = mUiAutomation.getRootInActiveWindow();
            if (activeRoot != null) {
                roots.add(activeRoot);
            }
            // Support multi-window searches for API level 21 and up.
            if (android.os.Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                for (AccessibilityWindowInfo window : mUiAutomation.getWindows()) {
                    AccessibilityNodeInfo root = window.getRoot();
                    if (root == null) {
                        Ln.w(String.format("Skipping null root node for window: %s", window.toString()));
                        continue;
                    }
                    roots.add(root);
                }
            }
            AccessibilityNodeInfo[] nodeInfos = roots.toArray(new AccessibilityNodeInfo[roots.size()]);
            for (int i = 0; i < nodeInfos.length; i++) {
                dumpNodeRec(nodeInfos[i], serializer, 0, width, height, nodeInfos.length - 1 - i);
            }
        }

        serializer.endTag("", "hierarchy");
        serializer.endDocument();
    }

    private static void dumpNodeRec(AccessibilityNodeInfo node, XmlSerializer serializer, int index,
                                    int width, int height, int zIndex) throws IOException {
        serializer.startTag("", "node");
        if (!nafExcludedClass(node) && !nafCheck(node))
            serializer.attribute("", "NAF", Boolean.toString(true));
        serializer.attribute("", "index", Integer.toString(index));
        serializer.attribute("", "text", safeCharSeqToString(node.getText()));
        serializer.attribute("", "resource-id", safeCharSeqToString(node.getViewIdResourceName()));
        serializer.attribute("", "class", safeCharSeqToString(node.getClassName()));
        serializer.attribute("", "package", safeCharSeqToString(node.getPackageName()));
        serializer.attribute("", "content-desc", safeCharSeqToString(node.getContentDescription()));
        serializer.attribute("", "checkable", Boolean.toString(node.isCheckable()));
        serializer.attribute("", "checked", Boolean.toString(node.isChecked()));
        serializer.attribute("", "clickable", Boolean.toString(node.isClickable()));
        serializer.attribute("", "enabled", Boolean.toString(node.isEnabled()));
        serializer.attribute("", "focusable", Boolean.toString(node.isFocusable()));
        serializer.attribute("", "focused", Boolean.toString(node.isFocused()));
        serializer.attribute("", "scrollable", Boolean.toString(node.isScrollable()));
        serializer.attribute("", "long-clickable", Boolean.toString(node.isLongClickable()));
        serializer.attribute("", "password", Boolean.toString(node.isPassword()));
        serializer.attribute("", "selected", Boolean.toString(node.isSelected()));
        serializer.attribute("", "visible-to-user", Boolean.toString(node.isVisibleToUser()));
        serializer.attribute("", "bounds", AccessibilityNodeInfoHelper.getVisibleBoundsInScreen(
                node, width, height).toShortString());
        serializer.attribute("", "z-index", Integer.toString(zIndex));
        int count = node.getChildCount();
        for (int i = 0; i < count; i++) {
            AccessibilityNodeInfo child = node.getChild(i);
            if (child != null) {
                if (child.isVisibleToUser()) {
                    dumpNodeRec(child, serializer, i, width, height, zIndex);
                    child.recycle();
                }
//                else {
//                    Ln.i(String.format("Skipping invisible child: %s", child.toString()));
//                }
            } else {
                Ln.i(String.format("Null child %d/%d, parent: %s", i, count, node.toString()));
            }
        }
        serializer.endTag("", "node");
    }

    /**
     * The list of classes to exclude my not be complete. We're attempting to
     * only reduce noise from standard layout classes that may be falsely
     * configured to accept clicks and are also enabled.
     *
     * @param node
     * @return true if node is excluded.
     */
    private static boolean nafExcludedClass(AccessibilityNodeInfo node) {
        String className = safeCharSeqToString(node.getClassName());
        for (String excludedClassName : NAF_EXCLUDED_CLASSES) {
            if (className.endsWith(excludedClassName))
                return true;
        }
        return false;
    }

    /**
     * We're looking for UI controls that are enabled, clickable but have no
     * text nor content-description. Such controls configuration indicate an
     * interactive control is present in the UI and is most likely not
     * accessibility friendly. We refer to such controls here as NAF controls
     * (Not Accessibility Friendly)
     *
     * @param node
     * @return false if a node fails the check, true if all is OK
     */
    private static boolean nafCheck(AccessibilityNodeInfo node) {
        boolean isNaf = node.isClickable() && node.isEnabled()
                && safeCharSeqToString(node.getContentDescription()).isEmpty()
                && safeCharSeqToString(node.getText()).isEmpty();

        if (!isNaf)
            return true;

        // check children since sometimes the containing element is clickable
        // and NAF but a child's text or description is available. Will assume
        // such layout as fine.
        return childNafCheck(node);
    }

    /**
     * This should be used when it's already determined that the node is NAF and
     * a further check of its children is in order. A node maybe a container
     * such as LinerLayout and may be set to be clickable but have no text or
     * content description but it is counting on one of its children to fulfill
     * the requirement for being accessibility friendly by having one or more of
     * its children fill the text or content-description. Such a combination is
     * considered by this dumper as acceptable for accessibility.
     *
     * @param node
     * @return false if node fails the check.
     */
    private static boolean childNafCheck(AccessibilityNodeInfo node) {
        int childCount = node.getChildCount();
        for (int x = 0; x < childCount; x++) {
            AccessibilityNodeInfo childNode = node.getChild(x);
            if (childNode != null) {//wen add childNode null
                if (!safeCharSeqToString(childNode.getContentDescription()).isEmpty()
                        || !safeCharSeqToString(childNode.getText()).isEmpty())
                    return true;

                if (childNafCheck(childNode))
                    return true;
            }
        }
        return false;
    }

    private static String safeCharSeqToString(CharSequence cs) {
        if (cs == null)
            return "";
        else {
            return stripInvalidXMLChars(cs);
        }
    }

    private static String stripInvalidXMLChars(CharSequence cs) {
        StringBuffer ret = new StringBuffer();
        char ch;
        /* http://www.w3.org/TR/xml11/#charsets
        [#x1-#x8], [#xB-#xC], [#xE-#x1F], [#x7F-#x84], [#x86-#x9F], [#xFDD0-#xFDDF],
        [#x1FFFE-#x1FFFF], [#x2FFFE-#x2FFFF], [#x3FFFE-#x3FFFF],
        [#x4FFFE-#x4FFFF], [#x5FFFE-#x5FFFF], [#x6FFFE-#x6FFFF],
        [#x7FFFE-#x7FFFF], [#x8FFFE-#x8FFFF], [#x9FFFE-#x9FFFF],
        [#xAFFFE-#xAFFFF], [#xBFFFE-#xBFFFF], [#xCFFFE-#xCFFFF],
        [#xDFFFE-#xDFFFF], [#xEFFFE-#xEFFFF], [#xFFFFE-#xFFFFF],
        [#x10FFFE-#x10FFFF].
         */
        for (int i = 0; i < cs.length(); i++) {
            ch = cs.charAt(i);

            if ((ch >= 0x1 && ch <= 0x8) || (ch >= 0xB && ch <= 0xC) || (ch >= 0xE && ch <= 0x1F) ||
                    (ch >= 0x7F && ch <= 0x84) || (ch >= 0x86 && ch <= 0x9f) ||
                    (ch >= 0xFDD0 && ch <= 0xFDDF) || (ch >= 0x1FFFE && ch <= 0x1FFFF) ||
                    (ch >= 0x2FFFE && ch <= 0x2FFFF) || (ch >= 0x3FFFE && ch <= 0x3FFFF) ||
                    (ch >= 0x4FFFE && ch <= 0x4FFFF) || (ch >= 0x5FFFE && ch <= 0x5FFFF) ||
                    (ch >= 0x6FFFE && ch <= 0x6FFFF) || (ch >= 0x7FFFE && ch <= 0x7FFFF) ||
                    (ch >= 0x8FFFE && ch <= 0x8FFFF) || (ch >= 0x9FFFE && ch <= 0x9FFFF) ||
                    (ch >= 0xAFFFE && ch <= 0xAFFFF) || (ch >= 0xBFFFE && ch <= 0xBFFFF) ||
                    (ch >= 0xCFFFE && ch <= 0xCFFFF) || (ch >= 0xDFFFE && ch <= 0xDFFFF) ||
                    (ch >= 0xEFFFE && ch <= 0xEFFFF) || (ch >= 0xFFFFE && ch <= 0xFFFFF) ||
                    (ch >= 0x10FFFE && ch <= 0x10FFFF))
                ret.append(".");
            else
                ret.append(ch);
        }
        return ret.toString();
    }
}
