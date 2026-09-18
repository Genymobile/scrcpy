package com.genymobile.scrcpy.android;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Build;
import android.os.SystemClock;
import android.text.InputType;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.IOException;
import java.io.InputStream;
import java.net.ConnectException;
import java.net.NoRouteToHostException;
import java.net.SocketTimeoutException;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

public final class MainActivity extends Activity {
    private static final String TAG = "MainActivity";
    private static final String PREF_SECURITY_WARNING_ACKNOWLEDGED =
            "plaintext-adb-warning-acknowledged";
    private static final long MIN_LOADING_MILLIS = 300L;

    private FrameLayout root;
    private ListView connectionList;
    private LinearLayout connectionBanner;
    private ArrayAdapter<String> connectionAdapter;
    private Button focusBackButton;
    private Button focusOpenAppButton;
    private Button focusDisconnectButton;
    private FrameLayout videoContainer;
    private TextView loadingOverlay;
    private ScrcpySurfaceView videoView;
    private ScrcpyClient client;
    private AdbAuthKey authKey;
    private SharedPreferences preferences;
    private final List<ConnectionProfile> profiles = new ArrayList<>();
    private boolean focusedMode;
    private int topInset;
    private int bottomInset;
    private int loadingGeneration;
    private long loadingShownAt;
    private final SessionGuard sessionGuard = new SessionGuard();

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        preferences = getSharedPreferences("connection-profiles", MODE_PRIVATE);
        loadProfiles();
        try {
            authKey = new AdbAuthKey(this);
        } catch (IOException e) {
            Log.e(TAG, "Could not create the ADB authentication key", e);
            authKey = null;
        }
        buildUi();
    }

    private void buildUi() {
        root = new FrameLayout(this);
        root.setBackgroundColor(Color.rgb(8, 10, 12));
        root.setOnApplyWindowInsetsListener((view, insets) -> {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                android.graphics.Insets systemBars = insets.getInsets(WindowInsets.Type.systemBars());
                topInset = systemBars.top;
                bottomInset = systemBars.bottom;
            } else {
                topInset = getLegacyTopInset(insets);
                bottomInset = getLegacyBottomInset(insets);
            }
            applyWindowInsets();
            return insets;
        });

        videoContainer = new FrameLayout(this);
        videoContainer.setBackgroundColor(Color.BLACK);
        root.addView(videoContainer, new FrameLayout.LayoutParams(-1, -1));

        videoView = new ScrcpySurfaceView(this);
        videoView.setSurfaceListener(new ScrcpySurfaceView.SurfaceListener() {
            @Override
            public void onSurfaceCreated() {
            }

            @Override
            public void onSurfaceDestroyed() {
                if (client != null) {
                    disconnect();
                }
            }
        });
        videoView.setTouchListener((action, x, y, pressure, width, height) -> {
            if (client != null) {
                client.touch(action, x, y, pressure, width, height);
            }
        });
        videoContainer.addView(videoView, new FrameLayout.LayoutParams(-1, -1, Gravity.CENTER));

        loadingOverlay = new TextView(this);
        loadingOverlay.setGravity(Gravity.CENTER);
        loadingOverlay.setTextColor(Color.WHITE);
        loadingOverlay.setTextSize(16);
        loadingOverlay.setBackgroundColor(Color.BLACK);
        loadingOverlay.setVisibility(View.GONE);

        connectionList = new ListView(this);
        connectionList.setId(R.id.connection_list);
        connectionList.setBackgroundColor(Color.rgb(8, 10, 12));
        connectionList.setDivider(new android.graphics.drawable.ColorDrawable(Color.rgb(42, 48, 52)));
        connectionList.setDividerHeight(dp(1));
        connectionAdapter = new ArrayAdapter<String>(this, android.R.layout.simple_list_item_2,
                android.R.id.text1, new ArrayList<>()) {
            @Override
            public View getView(int position, View convertView, android.view.ViewGroup parent) {
                LinearLayout row = new LinearLayout(MainActivity.this);
                row.setOrientation(LinearLayout.HORIZONTAL);
                row.setGravity(Gravity.CENTER_VERTICAL);
                row.setBackgroundColor(Color.rgb(8, 10, 12));
                row.setPadding(dp(16), dp(10), dp(8), dp(10));

                LinearLayout text = new LinearLayout(MainActivity.this);
                text.setOrientation(LinearLayout.VERTICAL);
                TextView title = new TextView(MainActivity.this);
                title.setTextSize(18);
                TextView subtitle = new TextView(MainActivity.this);
                subtitle.setTextSize(14);
                text.addView(title, new LinearLayout.LayoutParams(-1, dp(32)));
                text.addView(subtitle, new LinearLayout.LayoutParams(-1, dp(28)));
                row.addView(text, new LinearLayout.LayoutParams(0, -2, 1));

                if (position < profiles.size()) {
                    ConnectionProfile profile = profiles.get(position);
                    title.setText(profile.name);
                    title.setTextColor(Color.WHITE);
                    String destination = profile.host + ":" + profile.port;
                    if (!profile.appPackage.isEmpty()) {
                        destination += "  •  " + profile.appPackage;
                    }
                    subtitle.setText(destination);
                    subtitle.setTextColor(Color.LTGRAY);

                    row.setOnClickListener(view -> connectToProfile(profile));

                    Button actions = new Button(MainActivity.this);
                    actions.setText(R.string.more_actions);
                    actions.setTextSize(22);
                    actions.setTextColor(Color.LTGRAY);
                    actions.setAllCaps(false);
                    actions.setBackgroundColor(Color.rgb(8, 10, 12));
                    actions.setMinWidth(0);
                    actions.setMinHeight(0);
                    actions.setPadding(0, 0, 0, 0);
                    actions.setContentDescription(getString(R.string.actions_for, profile.name));
                    actions.setOnClickListener(view -> showProfileActions(position));
                    row.addView(actions, new LinearLayout.LayoutParams(dp(48), dp(56)));
                } else if (position == profiles.size()) {
                    title.setText(R.string.add_connection);
                    title.setTextColor(Color.rgb(128, 203, 196));
                    subtitle.setText(R.string.add_connection_subtitle);
                    subtitle.setTextColor(Color.LTGRAY);
                } else {
                    title.setText(R.string.reset_auth_key);
                    title.setTextColor(Color.rgb(255, 183, 77));
                    subtitle.setText(R.string.reset_auth_key_subtitle);
                    subtitle.setTextColor(Color.LTGRAY);
                }
                return row;
            }
        };
        connectionAdapter.setNotifyOnChange(false);
        connectionList.setAdapter(connectionAdapter);
        connectionList.setOnItemClickListener((parent, view, position, id) -> {
            if (position == profiles.size()) {
                showConnectionEditor(null);
            } else if (position == profiles.size() + 1) {
                confirmResetAuthKey();
            } else if (position >= 0 && position < profiles.size()) {
                connectToProfile(profiles.get(position));
            }
        });
        connectionList.setOnItemLongClickListener((parent, view, position, id) -> {
            if (position < 0 || position >= profiles.size()) {
                return true;
            }
            showProfileActions(position);
            return true;
        });

        connectionBanner = new LinearLayout(this);
        connectionBanner.setOrientation(LinearLayout.VERTICAL);
        connectionBanner.setGravity(Gravity.CENTER_VERTICAL);
        connectionBanner.setPadding(dp(18), dp(9), dp(18), dp(9));
        connectionBanner.setBackgroundColor(Color.rgb(16, 20, 24));

        TextView bannerTitle = new TextView(this);
        bannerTitle.setText(R.string.app_name);
        bannerTitle.setTextColor(Color.WHITE);
        bannerTitle.setTextSize(20);
        connectionBanner.addView(bannerTitle, new LinearLayout.LayoutParams(-1, dp(30)));

        TextView bannerSubtitle = new TextView(this);
        bannerSubtitle.setText(R.string.saved_connections);
        bannerSubtitle.setTextColor(Color.LTGRAY);
        bannerSubtitle.setTextSize(13);
        connectionBanner.addView(bannerSubtitle, new LinearLayout.LayoutParams(-1, dp(22)));

        FrameLayout.LayoutParams bannerParams = new FrameLayout.LayoutParams(-1, dp(70));
        root.addView(connectionBanner, bannerParams);

        FrameLayout.LayoutParams listParams = new FrameLayout.LayoutParams(-1, -1);
        listParams.topMargin = dp(70);
        root.addView(connectionList, listParams);
        root.addView(loadingOverlay, new FrameLayout.LayoutParams(-1, -1));

        focusBackButton = createOverlayButton(getString(R.string.target_back), Color.rgb(45, 55, 62));
        FrameLayout.LayoutParams backParams = new FrameLayout.LayoutParams(
                dp(112), dp(44), Gravity.TOP | Gravity.START);
        backParams.setMargins(dp(10), dp(10), 0, 0);
        focusBackButton.setVisibility(View.GONE);
        root.addView(focusBackButton, backParams);

        focusOpenAppButton = createOverlayButton(getString(R.string.open_app), Color.rgb(45, 55, 62));
        FrameLayout.LayoutParams appParams = new FrameLayout.LayoutParams(
                dp(104), dp(44), Gravity.TOP | Gravity.END);
        appParams.setMargins(0, dp(10), dp(114), 0);
        focusOpenAppButton.setVisibility(View.GONE);
        root.addView(focusOpenAppButton, appParams);

        focusDisconnectButton = createOverlayButton(getString(R.string.disconnect), Color.rgb(170, 35, 35));
        FrameLayout.LayoutParams disconnectParams = new FrameLayout.LayoutParams(
                dp(104), dp(44), Gravity.TOP | Gravity.END);
        disconnectParams.setMargins(0, dp(10), dp(10), 0);
        focusDisconnectButton.setVisibility(View.GONE);
        root.addView(focusDisconnectButton, disconnectParams);

        setContentView(root);
        refreshConnectionList();
        focusBackButton.setOnClickListener(view -> {
            if (client != null) {
                client.back();
            }
        });
        focusOpenAppButton.setOnClickListener(view -> showAppPicker());
        focusDisconnectButton.setOnClickListener(view -> disconnect());
        root.requestApplyInsets();
    }

    private Button createOverlayButton(String text, int color) {
        Button button = new Button(this);
        button.setText(text);
        button.setTextColor(Color.WHITE);
        button.setTextSize(11);
        button.setAllCaps(false);
        button.setMinHeight(0);
        button.setPadding(dp(6), 0, dp(6), 0);
        button.setBackgroundColor(color);
        return button;
    }

    private void connectToProfile(ConnectionProfile profile) {
        if (!preferences.getBoolean(PREF_SECURITY_WARNING_ACKNOWLEDGED, false)) {
            new AlertDialog.Builder(this)
                    .setTitle(R.string.security_warning_title)
                    .setMessage(R.string.security_warning_message)
                    .setNegativeButton(R.string.cancel, null)
                    .setPositiveButton(R.string.security_warning_acknowledge, (dialog, which) -> {
                        preferences.edit().putBoolean(PREF_SECURITY_WARNING_ACKNOWLEDGED, true)
                                .apply();
                        startConnection(profile);
                    })
                    .show();
            return;
        }
        startConnection(profile);
    }

    private void startConnection(ConnectionProfile profile) {
        if (client != null) {
            return;
        }
        if (!videoView.isReady()) {
            showToast(R.string.video_surface_not_ready);
            return;
        }
        AdbAuthKey currentAuthKey = loadAuthKey();
        if (currentAuthKey == null) {
            showToast(R.string.auth_key_unavailable);
            return;
        }
        showLoading(getString(R.string.status_connecting_short));
        final int loadingGeneration = this.loadingGeneration;
        final long session = sessionGuard.start();
        InputStream server;
        try {
            server = getAssets().open("scrcpy-server.jar");
        } catch (IOException e) {
            Log.e(TAG, "Could not load the bundled scrcpy server", e);
            loadingOverlay.setVisibility(View.GONE);
            showToast(R.string.server_load_failed);
            return;
        }
        ScrcpyClient nextClient = new ScrcpyClient(server, currentAuthKey,
                new ScrcpyClient.Listener() {
                    private boolean failed;

                    @Override
                    public void onConnected(int width, int height) {
                        runOnUiThread(() -> {
                            if (!isCurrentSession(session)) {
                                return;
                            }
                            videoView.setVideoSize(width, height);
                            enterFocusedMode();
                            if (!profile.appPackage.isEmpty()) {
                                launchTargetApp(profile.appPackage);
                            }
                        });
                    }

                    @Override
                    public void onVideoStarted() {
                        runOnUiThread(() -> {
                            if (isCurrentSession(session)) {
                                hideLoadingWhenReady(loadingGeneration);
                            }
                        });
                    }

                    @Override
                    public void onStatus(int messageId, Object... arguments) {
                        runOnUiThread(() -> {
                            if (isCurrentSession(session) && focusedMode
                                    && loadingOverlay.getVisibility() == View.VISIBLE) {
                                loadingOverlay.setText(getString(messageId, arguments));
                            }
                        });
                    }

                    @Override
                    public void onError(Throwable error) {
                        failed = true;
                        runOnUiThread(() -> {
                            if (isCurrentSession(session)) {
                                loadingOverlay.setText(R.string.connection_failed);
                                showToast(getString(R.string.error_connection,
                                        formatError(error)));
                            }
                        });
                    }

                    @Override
                    public void onDisconnected() {
                        runOnUiThread(() -> {
                            if (isCurrentSession(session)) {
                                client = null;
                                exitFocusedMode();
                                if (failed) {
                                    showToast(getString(R.string.disconnected_from, profile.name));
                                }
                            }
                        });
                    }
                });
        client = nextClient;
        enterFocusedMode();
        nextClient.connect(profile.host, profile.port, videoView.getHolder().getSurface());
    }

    private boolean isCurrentSession(long session) {
        return sessionGuard.isCurrent(session) && client != null;
    }

    private boolean isCurrentSession(long session, ScrcpyClient expected) {
        return sessionGuard.isCurrent(session) && client == expected;
    }

    private AdbAuthKey loadAuthKey() {
        if (authKey != null) {
            return authKey;
        }
        try {
            authKey = new AdbAuthKey(this);
        } catch (IOException e) {
            Log.e(TAG, "Could not create the ADB authentication key", e);
        }
        return authKey;
    }

    private void disconnect() {
        if (client == null) {
            return;
        }
        ScrcpyClient current = client;
        sessionGuard.invalidate();
        client = null;
        exitFocusedMode();
        current.close();
    }

    private void showAppPicker() {
        ScrcpyClient current = client;
        if (current == null) {
            return;
        }
        final long session = sessionGuard.current();
        focusOpenAppButton.setEnabled(false);
        current.listInstalledApps(new ScrcpyClient.AppListListener() {
            @Override
            public void onApps(List<String> packages) {
                runOnUiThread(() -> {
                    if (!isCurrentSession(session, current)) {
                        return;
                    }
                    focusOpenAppButton.setEnabled(true);
                    if (packages.isEmpty()) {
                        showPackageDialog();
                        return;
                    }
                    String[] items = packages.toArray(new String[0]);
                    new AlertDialog.Builder(MainActivity.this)
                            .setTitle(R.string.open_target_app)
                            .setItems(items, (dialog, which) -> launchTargetApp(items[which]))
                            .setNeutralButton(R.string.type_package, (dialog, which) -> showPackageDialog())
                            .setNegativeButton(R.string.cancel, null)
                            .show();
                });
            }

            @Override
            public void onError(Throwable error) {
                runOnUiThread(() -> {
                    if (!isCurrentSession(session, current)) {
                        return;
                    }
                    focusOpenAppButton.setEnabled(true);
                    showToast(getString(R.string.error_list_apps, formatError(error)));
                });
            }
        });
    }

    private void showPackageDialog() {
        EditText packageInput = new EditText(this);
        packageInput.setHint(R.string.app_package_hint);
        packageInput.setSingleLine(true);
        packageInput.setInputType(InputType.TYPE_CLASS_TEXT);
        new AlertDialog.Builder(this)
                .setTitle(R.string.enter_app_package)
                .setView(packageInput)
                .setNegativeButton(R.string.cancel, null)
                .setPositiveButton(R.string.open, (dialog, which) ->
                        launchTargetApp(packageInput.getText().toString().trim()))
                .show();
    }

    private void launchTargetApp(String packageName) {
        ScrcpyClient current = client;
        if (current == null) {
            return;
        }
        final long session = sessionGuard.current();
        focusOpenAppButton.setEnabled(false);
        current.wake();
        current.launchApp(packageName, new ScrcpyClient.ActionListener() {
            @Override
            public void onSuccess() {
                runOnUiThread(() -> {
                    if (isCurrentSession(session, current)) {
                        focusOpenAppButton.setEnabled(true);
                    }
                });
            }

            @Override
            public void onError(Throwable error) {
                runOnUiThread(() -> {
                    if (!isCurrentSession(session, current)) {
                        return;
                    }
                    focusOpenAppButton.setEnabled(true);
                    showToast(getString(R.string.error_open_app, formatError(error)));
                });
            }
        });
    }

    private void enterFocusedMode() {
        focusedMode = true;
        connectionBanner.setVisibility(View.GONE);
        connectionList.setVisibility(View.GONE);
        focusBackButton.setVisibility(View.VISIBLE);
        focusOpenAppButton.setVisibility(View.VISIBLE);
        focusDisconnectButton.setVisibility(View.VISIBLE);
        applyWindowInsets();
        setImmersiveMode(true);
    }

    private void exitFocusedMode() {
        if (!focusedMode) {
            return;
        }
        focusedMode = false;
        connectionBanner.setVisibility(View.VISIBLE);
        connectionList.setVisibility(View.VISIBLE);
        focusBackButton.setVisibility(View.GONE);
        focusOpenAppButton.setVisibility(View.GONE);
        focusDisconnectButton.setVisibility(View.GONE);
        loadingOverlay.setVisibility(View.GONE);
        loadingGeneration++;
        applyWindowInsets();
        setImmersiveMode(false);
        root.requestApplyInsets();
    }

    private void setImmersiveMode(boolean immersive) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                if (immersive) {
                    controller.hide(WindowInsets.Type.systemBars());
                    controller.setSystemBarsBehavior(
                            WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                } else {
                    controller.show(WindowInsets.Type.systemBars());
                }
                return;
            }
        }
        setLegacyImmersiveMode(immersive);
    }

    @SuppressWarnings("deprecation")
    private void setLegacyImmersiveMode(boolean immersive) {
        getWindow().getDecorView().setSystemUiVisibility(immersive
                ? View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                : View.SYSTEM_UI_FLAG_VISIBLE);
    }

    @SuppressWarnings("deprecation")
    private static int getLegacyTopInset(WindowInsets insets) {
        return insets.getSystemWindowInsetTop();
    }

    @SuppressWarnings("deprecation")
    private static int getLegacyBottomInset(WindowInsets insets) {
        return insets.getSystemWindowInsetBottom();
    }

    private void applyWindowInsets() {
        if (root == null) {
            return;
        }
        if (focusedMode) {
            root.setPadding(0, 0, 0, 0);
        } else {
            root.setPadding(0, topInset, 0, bottomInset);
        }
    }

    private void showLoading(String message) {
        loadingGeneration++;
        loadingShownAt = SystemClock.uptimeMillis();
        loadingOverlay.setText(message);
        loadingOverlay.setVisibility(View.VISIBLE);
    }

    private void hideLoadingWhenReady(int generation) {
        if (generation != loadingGeneration) {
            return;
        }
        long remaining = MIN_LOADING_MILLIS - (SystemClock.uptimeMillis() - loadingShownAt);
        if (remaining <= 0) {
            loadingOverlay.setVisibility(View.GONE);
        } else {
            loadingOverlay.postDelayed(() -> hideLoadingWhenReady(generation), remaining);
        }
    }

    private void refreshConnectionList() {
        connectionAdapter.clear();
        for (int i = 0; i < profiles.size() + 2; ++i) {
            connectionAdapter.add("");
        }
        connectionAdapter.notifyDataSetChanged();
    }

    private void showProfileActions(int position) {
        ConnectionProfile profile = profiles.get(position);
        new AlertDialog.Builder(this)
                .setTitle(profile.name)
                .setItems(new String[]{getString(R.string.connect), getString(R.string.edit),
                        getString(R.string.delete)}, (dialog, which) -> {
                    if (which == 0) {
                        connectToProfile(profile);
                    } else if (which == 1) {
                        showConnectionEditor(profile);
                    } else {
                        confirmDeleteProfile(position);
                    }
                })
                .show();
    }

    private void showConnectionEditor(ConnectionProfile existing) {
        LinearLayout fields = new LinearLayout(this);
        fields.setOrientation(LinearLayout.VERTICAL);
        fields.setPadding(dp(24), dp(8), dp(24), dp(8));

        TextView securityNotice = new TextView(this);
        securityNotice.setText(R.string.security_notice);
        securityNotice.setTextColor(Color.LTGRAY);
        securityNotice.setTextSize(13);
        securityNotice.setPadding(0, 0, 0, dp(10));
        fields.addView(securityNotice, new LinearLayout.LayoutParams(-1, -2));

        EditText nameInput = new EditText(this);
        nameInput.setHint(R.string.connection_name_hint);
        nameInput.setSingleLine(true);
        nameInput.setText(existing == null ? "" : existing.name);
        fields.addView(nameInput, new LinearLayout.LayoutParams(-1, dp(56)));

        EditText hostInput = new EditText(this);
        hostInput.setHint(R.string.host_hint);
        hostInput.setSingleLine(true);
        hostInput.setInputType(InputType.TYPE_CLASS_TEXT);
        hostInput.setText(existing == null ? "" : existing.host);
        fields.addView(hostInput, new LinearLayout.LayoutParams(-1, dp(56)));

        EditText portInput = new EditText(this);
        portInput.setHint(R.string.port_hint);
        portInput.setSingleLine(true);
        portInput.setInputType(InputType.TYPE_CLASS_NUMBER);
        portInput.setText(existing == null ? "" : String.valueOf(existing.port));
        fields.addView(portInput, new LinearLayout.LayoutParams(-1, dp(56)));

        EditText appInput = new EditText(this);
        appInput.setHint(R.string.app_package_hint);
        appInput.setSingleLine(true);
        appInput.setInputType(InputType.TYPE_CLASS_TEXT);
        appInput.setText(existing == null ? "" : existing.appPackage);
        fields.addView(appInput, new LinearLayout.LayoutParams(-1, dp(56)));

        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.addView(fields);

        AlertDialog dialog = new AlertDialog.Builder(this)
                .setTitle(existing == null ? R.string.add_connection_title : R.string.edit_connection_title)
                .setView(scroll)
                .setNegativeButton(R.string.cancel, null)
                .setPositiveButton(R.string.save, null)
                .create();
        dialog.setOnShowListener(ignored -> dialog.getButton(AlertDialog.BUTTON_POSITIVE)
                .setOnClickListener(view -> {
                    String name = nameInput.getText().toString().trim();
                    String host = hostInput.getText().toString().trim();
                    int port;
                    try {
                        port = Integer.parseInt(portInput.getText().toString().trim());
                    } catch (NumberFormatException e) {
                        showToast(R.string.invalid_port);
                        return;
                    }
                    String appPackage = appInput.getText().toString().trim();
                    if (!AndroidClientValidation.isValidProfile(name, host, port, appPackage)) {
                        showToast(R.string.invalid_connection);
                        return;
                    }
                    if (existing == null) {
                        profiles.add(new ConnectionProfile(name, host, port, appPackage));
                    } else {
                        existing.name = name;
                        existing.host = host;
                        existing.port = port;
                        existing.appPackage = appPackage;
                    }
                    saveProfiles();
                    refreshConnectionList();
                    dialog.dismiss();
                }));
        dialog.show();
    }

    private void confirmDeleteProfile(int position) {
        ConnectionProfile profile = profiles.get(position);
        new AlertDialog.Builder(this)
                .setTitle(R.string.delete_connection)
                .setMessage(profile.name)
                .setNegativeButton(R.string.cancel, null)
                .setPositiveButton(R.string.delete, (dialog, which) -> {
                    profiles.remove(position);
                    saveProfiles();
                    refreshConnectionList();
                })
                .show();
    }

    private void confirmResetAuthKey() {
        new AlertDialog.Builder(this)
                .setTitle(R.string.reset_auth_key_title)
                .setMessage(R.string.reset_auth_key_message)
                .setNegativeButton(R.string.cancel, null)
                .setPositiveButton(R.string.reset_auth_key_confirm, (dialog, which) -> {
                    try {
                        AdbAuthKey.reset(this);
                        authKey = new AdbAuthKey(this);
                        showToast(R.string.reset_auth_key_success);
                    } catch (IOException error) {
                        authKey = null;
                        Log.e(TAG, "Could not reset the ADB authentication key", error);
                        showToast(getString(R.string.reset_auth_key_failed,
                                formatError(error)));
                    }
                })
                .show();
    }

    private void loadProfiles() {
        String encoded = preferences.getString("profiles", "[]");
        profiles.clear();
        try {
            JSONArray array = new JSONArray(encoded);
            for (int i = 0; i < array.length(); ++i) {
                JSONObject item = array.getJSONObject(i);
                String name = item.getString("name");
                String host = item.getString("host");
                int port = item.getInt("port");
                String appPackage = item.optString("appPackage", "");
                if (AndroidClientValidation.isValidProfile(name, host, port, appPackage)) {
                    profiles.add(new ConnectionProfile(name, host, port, appPackage));
                }
            }
        } catch (Exception ignored) {
            profiles.clear();
        }
    }

    private void saveProfiles() {
        JSONArray array = new JSONArray();
        for (ConnectionProfile profile : profiles) {
            if (!AndroidClientValidation.isValidProfile(profile.name, profile.host,
                    profile.port, profile.appPackage)) {
                continue;
            }
            JSONObject item = new JSONObject();
            try {
                item.put("name", profile.name);
                item.put("host", profile.host);
                item.put("port", profile.port);
                item.put("appPackage", profile.appPackage);
                array.put(item);
            } catch (Exception ignored) {
            }
        }
        preferences.edit().putString("profiles", array.toString()).apply();
    }

    private void showToast(String message) {
        Toast.makeText(this, message, Toast.LENGTH_LONG).show();
    }

    private void showToast(int messageId) {
        showToast(getString(messageId));
    }

    private String formatError(Throwable error) {
        Log.e(TAG, "Operation failed", error);
        Throwable root = error;
        while (root.getCause() != null) {
            root = root.getCause();
        }
        if (root instanceof SocketTimeoutException) {
            return getString(R.string.error_target_timeout);
        }
        if (root instanceof ConnectException || root instanceof NoRouteToHostException
                || root instanceof UnknownHostException) {
            return getString(R.string.error_target_unreachable);
        }
        String message = root.getMessage();
        String normalized = message == null ? "" : message.toLowerCase(Locale.US);
        if (normalized.contains("rejected") && normalized.contains("key")
                || normalized.contains("authorize")) {
            return getString(R.string.error_adb_authorization);
        }
        if (normalized.contains("no launchable activity")
                || normalized.contains("no activities found")) {
            return getString(R.string.error_no_launchable_app);
        }
        if (normalized.contains("video") || normalized.contains("mediacodec")
                || normalized.contains("codec")) {
            return getString(R.string.error_video);
        }
        if (normalized.contains("packet") || normalized.contains("handshake")
                || normalized.contains("protocol") || normalized.contains("scrcpy response")
                || normalized.contains("push failed") || normalized.contains("push returned")) {
            return getString(R.string.error_protocol);
        }
        if (normalized.contains("shell") || normalized.contains("command")) {
            return getString(R.string.error_target_command);
        }
        if (normalized.contains("closed") || normalized.contains("stopped")) {
            return getString(R.string.error_connection_closed);
        }
        return getString(R.string.error_unknown);
    }

    @Override
    protected void onStop() {
        if (!isChangingConfigurations()) {
            disconnect();
        }
        super.onStop();
    }

    @Override
    protected void onDestroy() {
        disconnect();
        super.onDestroy();
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    private static final class ConnectionProfile {
        String name;
        String host;
        int port;
        String appPackage;

        ConnectionProfile(String name, String host, int port, String appPackage) {
            this.name = name;
            this.host = host;
            this.port = port;
            this.appPackage = appPackage == null ? "" : appPackage;
        }
    }
}
