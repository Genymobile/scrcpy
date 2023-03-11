package com.genymobile.scrcpy;

import android.graphics.Rect;
import android.os.BatteryManager;
import android.os.Build;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

public final class Server {

    private Server() {
        // not instantiable
    }

    private static void initAndCleanUp(Options options) {
        boolean mustDisableShowTouchesOnCleanUp = false;
        int restoreStayOn = -1;
        boolean restoreNormalPowerMode = options.getControl(); // only restore power mode if control is enabled
        if (options.getShowTouches() || options.getStayAwake()) {
            if (options.getShowTouches()) {
                try {
                    String oldValue = Settings.getAndPutValue(Settings.TABLE_SYSTEM, "show_touches", "1");
                    // If "show touches" was disabled, it must be disabled back on clean up
                    mustDisableShowTouchesOnCleanUp = !"1".equals(oldValue);
                } catch (SettingsException e) {
                    Ln.e("Could not change \"show_touches\"", e);
                }
            }

            if (options.getStayAwake()) {
                int stayOn = BatteryManager.BATTERY_PLUGGED_AC | BatteryManager.BATTERY_PLUGGED_USB | BatteryManager.BATTERY_PLUGGED_WIRELESS;
                try {
                    String oldValue = Settings.getAndPutValue(Settings.TABLE_GLOBAL, "stay_on_while_plugged_in", String.valueOf(stayOn));
                    try {
                        restoreStayOn = Integer.parseInt(oldValue);
                        if (restoreStayOn == stayOn) {
                            // No need to restore
                            restoreStayOn = -1;
                        }
                    } catch (NumberFormatException e) {
                        restoreStayOn = 0;
                    }
                } catch (SettingsException e) {
                    Ln.e("Could not change \"stay_on_while_plugged_in\"", e);
                }
            }
        }

        if (options.getCleanup()) {
            try {
                CleanUp.configure(options.getDisplayId(), restoreStayOn, mustDisableShowTouchesOnCleanUp, restoreNormalPowerMode,
                        options.getPowerOffScreenOnClose());
            } catch (IOException e) {
                Ln.e("Could not configure cleanup", e);
            }
        }
    }

    private static void scrcpy(Options options) throws IOException, ConfigurationException {
        Ln.i("Device: " + Build.MANUFACTURER + " " + Build.MODEL + " (Android " + Build.VERSION.RELEASE + ")");
        final Device device = new Device(options);

        Thread initThread = startInitThread(options);

        int scid = options.getScid();
        boolean tunnelForward = options.isTunnelForward();
        boolean control = options.getControl();
        boolean audio = options.getAudio();
        boolean sendDummyByte = options.getSendDummyByte();

        Workarounds.prepareMainLooper();

        // Workarounds must be applied for Meizu phones:
        //  - <https://github.com/Genymobile/scrcpy/issues/240>
        //  - <https://github.com/Genymobile/scrcpy/issues/365>
        //  - <https://github.com/Genymobile/scrcpy/issues/2656>
        //
        // But only apply when strictly necessary, since workarounds can cause other issues:
        //  - <https://github.com/Genymobile/scrcpy/issues/940>
        //  - <https://github.com/Genymobile/scrcpy/issues/994>
        boolean mustFillAppInfo = Build.BRAND.equalsIgnoreCase("meizu");

        // Before Android 11, audio is not supported.
        // Since Android 12, we can properly set a context on the AudioRecord.
        // Only on Android 11 we must fill app info for the AudioRecord to work.
        mustFillAppInfo |= audio && Build.VERSION.SDK_INT == Build.VERSION_CODES.R;

        if (mustFillAppInfo) {
            Workarounds.fillAppInfo();
        }

        List<AsyncProcessor> asyncProcessors = new ArrayList<>();

        try (DesktopConnection connection = DesktopConnection.open(scid, tunnelForward, audio, control, sendDummyByte)) {
            if (options.getSendDeviceMeta()) {
                connection.sendDeviceMeta(Device.getDeviceName());
            }

            if (control) {
                Controller controller = new Controller(device, connection, options.getClipboardAutosync(), options.getPowerOn());
                device.setClipboardListener(text -> controller.getSender().pushClipboardText(text));
                asyncProcessors.add(controller);
            }

            if (audio) {
                AudioCodec audioCodec = options.getAudioCodec();
                Streamer audioStreamer = new Streamer(connection.getAudioFd(), audioCodec, options.getSendCodecMeta(),
                        options.getSendFrameMeta());
                AsyncProcessor audioRecorder;
                if (audioCodec == AudioCodec.RAW) {
                    audioRecorder = new AudioRawRecorder(audioStreamer);
                } else {
                    audioRecorder = new AudioEncoder(audioStreamer, options.getAudioBitRate(), options.getAudioCodecOptions(),
                            options.getAudioEncoder());
                }
                asyncProcessors.add(audioRecorder);
            }

            Streamer videoStreamer = new Streamer(connection.getVideoFd(), options.getVideoCodec(), options.getSendCodecMeta(),
                    options.getSendFrameMeta());
            ScreenEncoder screenEncoder = new ScreenEncoder(device, videoStreamer, options.getVideoBitRate(), options.getMaxFps(),
                    options.getVideoCodecOptions(), options.getVideoEncoder(), options.getDownsizeOnError());

            for (AsyncProcessor asyncProcessor : asyncProcessors) {
                asyncProcessor.start();
            }

            try {
                // synchronous
                screenEncoder.streamScreen();
            } catch (IOException e) {
                // Broken pipe is expected on close, because the socket is closed by the client
                if (!IO.isBrokenPipe(e)) {
                    Ln.e("Video encoding error", e);
                }
            }
        } finally {
            Ln.d("Screen streaming stopped");
            initThread.interrupt();
            for (AsyncProcessor asyncProcessor : asyncProcessors) {
                asyncProcessor.stop();
            }

            try {
                initThread.join();
                for (AsyncProcessor asyncProcessor : asyncProcessors) {
                    asyncProcessor.join();
                }
            } catch (InterruptedException e) {
                // ignore
            }
        }
    }

    private static Thread startInitThread(final Options options) {
        Thread thread = new Thread(() -> initAndCleanUp(options));
        thread.start();
        return thread;
    }

    @SuppressWarnings("MethodLength")
    private static Options createOptions(String... args) {
        if (args.length < 1) {
            throw new IllegalArgumentException("Missing client version");
        }

        String clientVersion = args[0];
        if (!clientVersion.equals(BuildConfig.VERSION_NAME)) {
            throw new IllegalArgumentException(
                    "The server version (" + BuildConfig.VERSION_NAME + ") does not match the client " + "(" + clientVersion + ")");
        }

        Options options = new Options();

        for (int i = 1; i < args.length; ++i) {
            String arg = args[i];
            int equalIndex = arg.indexOf('=');
            if (equalIndex == -1) {
                throw new IllegalArgumentException("Invalid key=value pair: \"" + arg + "\"");
            }
            String key = arg.substring(0, equalIndex);
            String value = arg.substring(equalIndex + 1);
            switch (key) {
                case "scid":
                    int scid = Integer.parseInt(value, 0x10);
                    if (scid < -1) {
                        throw new IllegalArgumentException("scid may not be negative (except -1 for 'none'): " + scid);
                    }
                    options.setScid(scid);
                    break;
                case "log_level":
                    Ln.Level level = Ln.Level.valueOf(value.toUpperCase(Locale.ENGLISH));
                    options.setLogLevel(level);
                    break;
                case "audio":
                    boolean audio = Boolean.parseBoolean(value);
                    options.setAudio(audio);
                    break;
                case "video_codec":
                    VideoCodec videoCodec = VideoCodec.findByName(value);
                    if (videoCodec == null) {
                        throw new IllegalArgumentException("Video codec " + value + " not supported");
                    }
                    options.setVideoCodec(videoCodec);
                    break;
                case "audio_codec":
                    AudioCodec audioCodec = AudioCodec.findByName(value);
                    if (audioCodec == null) {
                        throw new IllegalArgumentException("Audio codec " + value + " not supported");
                    }
                    options.setAudioCodec(audioCodec);
                    break;
                case "max_size":
                    int maxSize = Integer.parseInt(value) & ~7; // multiple of 8
                    options.setMaxSize(maxSize);
                    break;
                case "video_bit_rate":
                    int videoBitRate = Integer.parseInt(value);
                    options.setVideoBitRate(videoBitRate);
                    break;
                case "audio_bit_rate":
                    int audioBitRate = Integer.parseInt(value);
                    options.setAudioBitRate(audioBitRate);
                    break;
                case "max_fps":
                    int maxFps = Integer.parseInt(value);
                    options.setMaxFps(maxFps);
                    break;
                case "lock_video_orientation":
                    int lockVideoOrientation = Integer.parseInt(value);
                    options.setLockVideoOrientation(lockVideoOrientation);
                    break;
                case "tunnel_forward":
                    boolean tunnelForward = Boolean.parseBoolean(value);
                    options.setTunnelForward(tunnelForward);
                    break;
                case "crop":
                    Rect crop = parseCrop(value);
                    options.setCrop(crop);
                    break;
                case "control":
                    boolean control = Boolean.parseBoolean(value);
                    options.setControl(control);
                    break;
                case "display_id":
                    int displayId = Integer.parseInt(value);
                    options.setDisplayId(displayId);
                    break;
                case "show_touches":
                    boolean showTouches = Boolean.parseBoolean(value);
                    options.setShowTouches(showTouches);
                    break;
                case "stay_awake":
                    boolean stayAwake = Boolean.parseBoolean(value);
                    options.setStayAwake(stayAwake);
                    break;
                case "video_codec_options":
                    List<CodecOption> videoCodecOptions = CodecOption.parse(value);
                    options.setVideoCodecOptions(videoCodecOptions);
                    break;
                case "audio_codec_options":
                    List<CodecOption> audioCodecOptions = CodecOption.parse(value);
                    options.setAudioCodecOptions(audioCodecOptions);
                    break;
                case "video_encoder":
                    if (!value.isEmpty()) {
                        options.setVideoEncoder(value);
                    }
                    break;
                case "audio_encoder":
                    if (!value.isEmpty()) {
                        options.setAudioEncoder(value);
                    }
                case "power_off_on_close":
                    boolean powerOffScreenOnClose = Boolean.parseBoolean(value);
                    options.setPowerOffScreenOnClose(powerOffScreenOnClose);
                    break;
                case "clipboard_autosync":
                    boolean clipboardAutosync = Boolean.parseBoolean(value);
                    options.setClipboardAutosync(clipboardAutosync);
                    break;
                case "downsize_on_error":
                    boolean downsizeOnError = Boolean.parseBoolean(value);
                    options.setDownsizeOnError(downsizeOnError);
                    break;
                case "cleanup":
                    boolean cleanup = Boolean.parseBoolean(value);
                    options.setCleanup(cleanup);
                    break;
                case "power_on":
                    boolean powerOn = Boolean.parseBoolean(value);
                    options.setPowerOn(powerOn);
                    break;
                case "list_encoders":
                    boolean listEncoders = Boolean.parseBoolean(value);
                    options.setListEncoders(listEncoders);
                    break;
                case "list_displays":
                    boolean listDisplays = Boolean.parseBoolean(value);
                    options.setListDisplays(listDisplays);
                    break;
                case "send_device_meta":
                    boolean sendDeviceMeta = Boolean.parseBoolean(value);
                    options.setSendDeviceMeta(sendDeviceMeta);
                    break;
                case "send_frame_meta":
                    boolean sendFrameMeta = Boolean.parseBoolean(value);
                    options.setSendFrameMeta(sendFrameMeta);
                    break;
                case "send_dummy_byte":
                    boolean sendDummyByte = Boolean.parseBoolean(value);
                    options.setSendDummyByte(sendDummyByte);
                    break;
                case "send_codec_meta":
                    boolean sendCodecMeta = Boolean.parseBoolean(value);
                    options.setSendCodecMeta(sendCodecMeta);
                    break;
                case "raw_video_stream":
                    boolean rawVideoStream = Boolean.parseBoolean(value);
                    if (rawVideoStream) {
                        options.setSendDeviceMeta(false);
                        options.setSendFrameMeta(false);
                        options.setSendDummyByte(false);
                        options.setSendCodecMeta(false);
                    }
                    break;
                default:
                    Ln.w("Unknown server option: " + key);
                    break;
            }
        }

        return options;
    }

    private static Rect parseCrop(String crop) {
        if (crop.isEmpty()) {
            return null;
        }
        // input format: "width:height:x:y"
        String[] tokens = crop.split(":");
        if (tokens.length != 4) {
            throw new IllegalArgumentException("Crop must contains 4 values separated by colons: \"" + crop + "\"");
        }
        int width = Integer.parseInt(tokens[0]);
        int height = Integer.parseInt(tokens[1]);
        int x = Integer.parseInt(tokens[2]);
        int y = Integer.parseInt(tokens[3]);
        return new Rect(x, y, x + width, y + height);
    }

    public static void main(String... args) throws Exception {
        Thread.setDefaultUncaughtExceptionHandler((t, e) -> {
            Ln.e("Exception on thread " + t, e);
        });

        Options options = createOptions(args);

        Ln.initLogLevel(options.getLogLevel());

        if (options.getListEncoders() || options.getListDisplays()) {
            if (options.getCleanup()) {
                CleanUp.unlinkSelf();
            }

            if (options.getListEncoders()) {
                Ln.i(LogUtils.buildVideoEncoderListMessage());
                Ln.i(LogUtils.buildAudioEncoderListMessage());
            }
            if (options.getListDisplays()) {
                Ln.i(LogUtils.buildDisplayListMessage());
            }
            // Just print the requested data, do not mirror
            return;
        }

        try {
            scrcpy(options);
        } catch (ConfigurationException e) {
            // Do not print stack trace, a user-friendly error-message has already been logged
        }
    }
}
