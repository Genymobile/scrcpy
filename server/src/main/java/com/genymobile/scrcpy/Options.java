package com.genymobile.scrcpy;

import com.genymobile.scrcpy.audio.AudioCodec;
import com.genymobile.scrcpy.audio.AudioSource;
import com.genymobile.scrcpy.device.Size;
import com.genymobile.scrcpy.util.CodecOption;
import com.genymobile.scrcpy.util.Ln;
import com.genymobile.scrcpy.video.CameraAspectRatio;
import com.genymobile.scrcpy.video.CameraFacing;
import com.genymobile.scrcpy.video.VideoCodec;
import com.genymobile.scrcpy.video.VideoSource;

import android.graphics.Rect;

import java.util.List;
import java.util.Locale;

public class Options {

    private Ln.Level logLevel = Ln.Level.DEBUG;
    private int scid = -1; // 31-bit non-negative value, or -1
    private boolean video = true;
    private boolean audio = true;
    private int maxSize;
    private VideoCodec videoCodec = VideoCodec.H264;
    private AudioCodec audioCodec = AudioCodec.OPUS;
    private VideoSource videoSource = VideoSource.DISPLAY;
    private AudioSource audioSource = AudioSource.OUTPUT;
    private boolean audioDup;
    private int videoBitRate = 8000000;
    private int audioBitRate = 128000;
    private float maxFps;
    private int lockVideoOrientation = -1;
    private boolean tunnelForward;
    private Rect crop;
    private boolean control = true;
    private int displayId;
    private String cameraId;
    private Size cameraSize;
    private CameraFacing cameraFacing;
    private CameraAspectRatio cameraAspectRatio;
    private int cameraFps;
    private boolean cameraHighSpeed;
    private boolean showTouches;
    private boolean stayAwake;
    private List<CodecOption> videoCodecOptions;
    private List<CodecOption> audioCodecOptions;

    private String videoEncoder;
    private String audioEncoder;
    private boolean powerOffScreenOnClose;
    private boolean clipboardAutosync = true;
    private boolean downsizeOnError = true;
    private boolean cleanup = true;
    private boolean powerOn = true;

    private boolean listEncoders;
    private boolean listDisplays;
    private boolean listCameras;
    private boolean listCameraSizes;

    // Options not used by the scrcpy client, but useful to use scrcpy-server directly
    private boolean sendDeviceMeta = true; // send device name and size
    private boolean sendFrameMeta = true; // send PTS so that the client may record properly
    private boolean sendDummyByte = true; // write a byte on start to detect connection issues
    private boolean sendCodecMeta = true; // write the codec metadata before the stream

    public Ln.Level getLogLevel() {
        return logLevel;
    }

    public int getScid() {
        return scid;
    }

    public boolean getVideo() {
        return video;
    }

    public boolean getAudio() {
        return audio;
    }

    public int getMaxSize() {
        return maxSize;
    }

    public VideoCodec getVideoCodec() {
        return videoCodec;
    }

    public AudioCodec getAudioCodec() {
        return audioCodec;
    }

    public VideoSource getVideoSource() {
        return videoSource;
    }

    public AudioSource getAudioSource() {
        return audioSource;
    }

    public boolean getAudioDup() {
        return audioDup;
    }

    public int getVideoBitRate() {
        return videoBitRate;
    }

    public int getAudioBitRate() {
        return audioBitRate;
    }

    public float getMaxFps() {
        return maxFps;
    }

    public int getLockVideoOrientation() {
        return lockVideoOrientation;
    }

    public boolean isTunnelForward() {
        return tunnelForward;
    }

    public Rect getCrop() {
        return crop;
    }

    public boolean getControl() {
        return control;
    }

    public int getDisplayId() {
        return displayId;
    }

    public String getCameraId() {
        return cameraId;
    }

    public Size getCameraSize() {
        return cameraSize;
    }

    public CameraFacing getCameraFacing() {
        return cameraFacing;
    }

    public CameraAspectRatio getCameraAspectRatio() {
        return cameraAspectRatio;
    }

    public int getCameraFps() {
        return cameraFps;
    }

    public boolean getCameraHighSpeed() {
        return cameraHighSpeed;
    }

    public boolean getShowTouches() {
        return showTouches;
    }

    public boolean getStayAwake() {
        return stayAwake;
    }

    public List<CodecOption> getVideoCodecOptions() {
        return videoCodecOptions;
    }

    public List<CodecOption> getAudioCodecOptions() {
        return audioCodecOptions;
    }

    public String getVideoEncoder() {
        return videoEncoder;
    }

    public String getAudioEncoder() {
        return audioEncoder;
    }

    public boolean getPowerOffScreenOnClose() {
        return this.powerOffScreenOnClose;
    }

    public boolean getClipboardAutosync() {
        return clipboardAutosync;
    }

    public boolean getDownsizeOnError() {
        return downsizeOnError;
    }

    public boolean getCleanup() {
        return cleanup;
    }

    public boolean getPowerOn() {
        return powerOn;
    }

    public boolean getList() {
        return listEncoders || listDisplays || listCameras || listCameraSizes;
    }

    public boolean getListEncoders() {
        return listEncoders;
    }

    public boolean getListDisplays() {
        return listDisplays;
    }

    public boolean getListCameras() {
        return listCameras;
    }

    public boolean getListCameraSizes() {
        return listCameraSizes;
    }

    public boolean getSendDeviceMeta() {
        return sendDeviceMeta;
    }

    public boolean getSendFrameMeta() {
        return sendFrameMeta;
    }

    public boolean getSendDummyByte() {
        return sendDummyByte;
    }

    public boolean getSendCodecMeta() {
        return sendCodecMeta;
    }

    @SuppressWarnings("MethodLength")
    public static Options parse(String... args) {
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
                    options.scid = scid;
                    break;
                case "log_level":
                    options.logLevel = Ln.Level.valueOf(value.toUpperCase(Locale.ENGLISH));
                    break;
                case "video":
                    options.video = Boolean.parseBoolean(value);
                    break;
                case "audio":
                    options.audio = Boolean.parseBoolean(value);
                    break;
                case "video_codec":
                    VideoCodec videoCodec = VideoCodec.findByName(value);
                    if (videoCodec == null) {
                        throw new IllegalArgumentException("Video codec " + value + " not supported");
                    }
                    options.videoCodec = videoCodec;
                    break;
                case "audio_codec":
                    AudioCodec audioCodec = AudioCodec.findByName(value);
                    if (audioCodec == null) {
                        throw new IllegalArgumentException("Audio codec " + value + " not supported");
                    }
                    options.audioCodec = audioCodec;
                    break;
                case "video_source":
                    VideoSource videoSource = VideoSource.findByName(value);
                    if (videoSource == null) {
                        throw new IllegalArgumentException("Video source " + value + " not supported");
                    }
                    options.videoSource = videoSource;
                    break;
                case "audio_source":
                    AudioSource audioSource = AudioSource.findByName(value);
                    if (audioSource == null) {
                        throw new IllegalArgumentException("Audio source " + value + " not supported");
                    }
                    options.audioSource = audioSource;
                    break;
                case "audio_dup":
                    options.audioDup = Boolean.parseBoolean(value);
                    break;
                case "max_size":
                    options.maxSize = Integer.parseInt(value) & ~7; // multiple of 8
                    break;
                case "video_bit_rate":
                    options.videoBitRate = Integer.parseInt(value);
                    break;
                case "audio_bit_rate":
                    options.audioBitRate = Integer.parseInt(value);
                    break;
                case "max_fps":
                    options.maxFps = parseFloat("max_fps", value);
                    break;
                case "lock_video_orientation":
                    options.lockVideoOrientation = Integer.parseInt(value);
                    break;
                case "tunnel_forward":
                    options.tunnelForward = Boolean.parseBoolean(value);
                    break;
                case "crop":
                    if (!value.isEmpty()) {
                        options.crop = parseCrop(value);
                    }
                    break;
                case "control":
                    options.control = Boolean.parseBoolean(value);
                    break;
                case "display_id":
                    options.displayId = Integer.parseInt(value);
                    break;
                case "show_touches":
                    options.showTouches = Boolean.parseBoolean(value);
                    break;
                case "stay_awake":
                    options.stayAwake = Boolean.parseBoolean(value);
                    break;
                case "video_codec_options":
                    options.videoCodecOptions = CodecOption.parse(value);
                    break;
                case "audio_codec_options":
                    options.audioCodecOptions = CodecOption.parse(value);
                    break;
                case "video_encoder":
                    if (!value.isEmpty()) {
                        options.videoEncoder = value;
                    }
                    break;
                case "audio_encoder":
                    if (!value.isEmpty()) {
                        options.audioEncoder = value;
                    }
                case "power_off_on_close":
                    options.powerOffScreenOnClose = Boolean.parseBoolean(value);
                    break;
                case "clipboard_autosync":
                    options.clipboardAutosync = Boolean.parseBoolean(value);
                    break;
                case "downsize_on_error":
                    options.downsizeOnError = Boolean.parseBoolean(value);
                    break;
                case "cleanup":
                    options.cleanup = Boolean.parseBoolean(value);
                    break;
                case "power_on":
                    options.powerOn = Boolean.parseBoolean(value);
                    break;
                case "list_encoders":
                    options.listEncoders = Boolean.parseBoolean(value);
                    break;
                case "list_displays":
                    options.listDisplays = Boolean.parseBoolean(value);
                    break;
                case "list_cameras":
                    options.listCameras = Boolean.parseBoolean(value);
                    break;
                case "list_camera_sizes":
                    options.listCameraSizes = Boolean.parseBoolean(value);
                    break;
                case "camera_id":
                    if (!value.isEmpty()) {
                        options.cameraId = value;
                    }
                    break;
                case "camera_size":
                    if (!value.isEmpty()) {
                        options.cameraSize = parseSize(value);
                    }
                    break;
                case "camera_facing":
                    if (!value.isEmpty()) {
                        CameraFacing facing = CameraFacing.findByName(value);
                        if (facing == null) {
                            throw new IllegalArgumentException("Camera facing " + value + " not supported");
                        }
                        options.cameraFacing = facing;
                    }
                    break;
                case "camera_ar":
                    if (!value.isEmpty()) {
                        options.cameraAspectRatio = parseCameraAspectRatio(value);
                    }
                    break;
                case "camera_fps":
                    options.cameraFps = Integer.parseInt(value);
                    break;
                case "camera_high_speed":
                    options.cameraHighSpeed = Boolean.parseBoolean(value);
                    break;
                case "send_device_meta":
                    options.sendDeviceMeta = Boolean.parseBoolean(value);
                    break;
                case "send_frame_meta":
                    options.sendFrameMeta = Boolean.parseBoolean(value);
                    break;
                case "send_dummy_byte":
                    options.sendDummyByte = Boolean.parseBoolean(value);
                    break;
                case "send_codec_meta":
                    options.sendCodecMeta = Boolean.parseBoolean(value);
                    break;
                case "raw_stream":
                    boolean rawStream = Boolean.parseBoolean(value);
                    if (rawStream) {
                        options.sendDeviceMeta = false;
                        options.sendFrameMeta = false;
                        options.sendDummyByte = false;
                        options.sendCodecMeta = false;
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
        // input format: "width:height:x:y"
        String[] tokens = crop.split(":");
        if (tokens.length != 4) {
            throw new IllegalArgumentException("Crop must contains 4 values separated by colons: \"" + crop + "\"");
        }
        int width = Integer.parseInt(tokens[0]);
        int height = Integer.parseInt(tokens[1]);
        if (width <= 0 || height <= 0) {
            throw new IllegalArgumentException("Invalid crop size: " + width + "x" + height);
        }
        int x = Integer.parseInt(tokens[2]);
        int y = Integer.parseInt(tokens[3]);
        if (x < 0 || y < 0) {
            throw new IllegalArgumentException("Invalid crop offset: " + x + ":" + y);
        }
        return new Rect(x, y, x + width, y + height);
    }

    private static Size parseSize(String size) {
        // input format: "<width>x<height>"
        String[] tokens = size.split("x");
        if (tokens.length != 2) {
            throw new IllegalArgumentException("Invalid size format (expected <width>x<height>): \"" + size + "\"");
        }
        int width = Integer.parseInt(tokens[0]);
        int height = Integer.parseInt(tokens[1]);
        return new Size(width, height);
    }

    private static CameraAspectRatio parseCameraAspectRatio(String ar) {
        if ("sensor".equals(ar)) {
            return CameraAspectRatio.sensorAspectRatio();
        }

        String[] tokens = ar.split(":");
        if (tokens.length == 2) {
            int w = Integer.parseInt(tokens[0]);
            int h = Integer.parseInt(tokens[1]);
            return CameraAspectRatio.fromFraction(w, h);
        }

        float floatAr = Float.parseFloat(tokens[0]);
        return CameraAspectRatio.fromFloat(floatAr);
    }

    private static float parseFloat(String key, String value) {
        try {
            return Float.parseFloat(value);
        } catch (NumberFormatException e) {
            throw new IllegalArgumentException("Invalid float value for " + key + ": \"" + value + "\"");
        }
    }
}
