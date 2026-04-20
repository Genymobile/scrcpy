package com.genymobile.scrcpy.control;

import com.genymobile.scrcpy.util.Ln;

import android.os.IBinder;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

/**
 * Registers a TaskStackListener via reflection (hidden API) to be notified when the
 * task stack changes. Falls back to polling if registration fails.
 */
public final class TaskStackMonitor {

    public interface Listener {
        void onTaskStackChanged();
    }

    private final Listener listener;
    private Object activityTaskManager; // IActivityTaskManager
    private Object taskStackListenerProxy; // ITaskStackListener
    private Method registerMethod;
    private Method unregisterMethod;

    public TaskStackMonitor(Listener listener) {
        this.listener = listener;
    }

    public boolean start() {
        try {
            // android.app.ActivityTaskManager.getService()
            Class<?> atmClazz = Class.forName("android.app.ActivityTaskManager");
            Method getService = atmClazz.getDeclaredMethod("getService");
            activityTaskManager = getService.invoke(null);
            if (activityTaskManager == null) {
                Ln.w("ActivityTaskManager.getService() returned null");
                return false;
            }

            // Interface android.app.ITaskStackListener
            Class<?> listenerInterface = Class.forName("android.app.ITaskStackListener");

            // Create dynamic proxy for ITaskStackListener
            taskStackListenerProxy = Proxy.newProxyInstance(
                ClassLoader.getSystemClassLoader(),
                new Class<?>[] { listenerInterface },
                new InvocationHandler() {
                    @Override
                    public Object invoke(Object proxy, Method method, Object[] args) {
                        String name = method.getName();
                        if ("onTaskStackChanged".equals(name)
                                || "onTaskMovedToFront".equals(name)
                                || "onTaskFocusChanged".equals(name)
                                || "onTaskStackChangedBackground".equals(name)) {
                            try {
                                listener.onTaskStackChanged();
                            } catch (Throwable t) {
                                Ln.w("TaskStack listener callback error: " + t.getMessage());
                            }
                        }
                        return null;
                    }
                }
            );

            // Methods: register/unregisterTaskStackListener(ITaskStackListener)
            registerMethod = activityTaskManager.getClass().getMethod(
                "registerTaskStackListener", listenerInterface);
            unregisterMethod = activityTaskManager.getClass().getMethod(
                "unregisterTaskStackListener", listenerInterface);

            registerMethod.invoke(activityTaskManager, taskStackListenerProxy);
            Ln.i("TaskStackMonitor registered");
            return true;
        } catch (Throwable e) {
            Ln.w("Could not register TaskStackListener: " + e.getMessage());
            return false;
        }
    }

    public void stop() {
        try {
            if (activityTaskManager != null && taskStackListenerProxy != null && unregisterMethod != null) {
                unregisterMethod.invoke(activityTaskManager, taskStackListenerProxy);
                Ln.i("TaskStackMonitor unregistered");
            }
        } catch (Throwable e) {
            Ln.w("Could not unregister TaskStackListener: " + e.getMessage());
        }
        activityTaskManager = null;
        taskStackListenerProxy = null;
        registerMethod = null;
        unregisterMethod = null;
    }
}


