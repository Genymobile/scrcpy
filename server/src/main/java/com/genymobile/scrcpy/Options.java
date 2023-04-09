package com.genymobile.scrcpy;

import android.graphics.Rect;

import java.util.List;
import java.util.Locale;

public class Options {

    private Ln.Level logLevel = Ln.Level.DEBUG;
    private int scid = -1; // 31-bit non-negative value, or -1
    private boolean audio = true;
    private int maxSize;
    private VideoCodec videoCodec = VideoCodec.H264;
    private AudioCodec audioCodec = AudioCodec.OPUS;
    private int videoBitRate = 8000000;
    private int audioBitRate = 128000;
    private int maxFps;
    private int lockVideoOrientation = -1;
    private boolean tunnelForward;
    private Rect crop;
    private boolean control = true;
    private int displayId;
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

    // Options not used by the scrcpy client, but useful to use scrcpy-server directly
    private boolean sendDeviceMeta = true; // send device name and size
    private boolean sendFrameMeta = true; // send PTS so that the client may record properly
    private boolean sendDummyByte = true; // write a byte on start to detect connection issues
    private boolean sendCodecMeta = true; // write the codec metadata before the stream

    public Ln.Level getLogLevel() {
        return logLevel;
    }

    public void setLogLevel(Ln.Level logLevel) {
        this.logLevel = logLevel;
    }

    public int getScid() {
        return scid;
    }

    public void setScid(int scid) {
        this.scid = scid;
    }

    public boolean getAudio() {
        return audio;
    }

    public void setAudio(boolean audio) {
        this.audio = audio;
    }

    public int getMaxSize() {
        return maxSize;
    }

    public void setMaxSize(int maxSize) {
        this.maxSize = maxSize;
    }

    public VideoCodec getVideoCodec() {
        return videoCodec;
    }

    public void setVideoCodec(VideoCodec videoCodec) {
        this.videoCodec = videoCodec;
    }

    public AudioCodec getAudioCodec() {
        return audioCodec;
    }

    public void setAudioCodec(AudioCodec audioCodec) {
        this.audioCodec = audioCodec;
    }

    public int getVideoBitRate() {
        return videoBitRate;
    }

    public void setVideoBitRate(int videoBitRate) {
        this.videoBitRate = videoBitRate;
    }

    public int getAudioBitRate() {
        return audioBitRate;
    }

    public void setAudioBitRate(int audioBitRate) {
        this.audioBitRate = audioBitRate;
    }

    public int getMaxFps() {
        return maxFps;
    }

    public void setMaxFps(int maxFps) {
        this.maxFps = maxFps;
    }

    public int getLockVideoOrientation() {
        return lockVideoOrientation;
    }

    public void setLockVideoOrientation(int lockVideoOrientation) {
        this.lockVideoOrientation = lockVideoOrientation;
    }

    public boolean isTunnelForward() {
        return tunnelForward;
    }

    public void setTunnelForward(boolean tunnelForward) {
        this.tunnelForward = tunnelForward;
    }

    public Rect getCrop() {
        return crop;
    }

    public void setCrop(Rect crop) {
        this.crop = crop;
    }

    public boolean getControl() {
        return control;
    }

    public void setControl(boolean control) {
        this.control = control;
    }

    public int getDisplayId() {
        return displayId;
    }

    public void setDisplayId(int displayId) {
        this.displayId = displayId;
    }

    public boolean getShowTouches() {
        return showTouches;
    }

    public void setShowTouches(boolean showTouches) {
        this.showTouches = showTouches;
    }

    public boolean getStayAwake() {
        return stayAwake;
    }

    public void setStayAwake(boolean stayAwake) {
        this.stayAwake = stayAwake;
    }

    public List<CodecOption> getVideoCodecOptions() {
        return videoCodecOptions;
    }

    public void setVideoCodecOptions(List<CodecOption> videoCodecOptions) {
        this.videoCodecOptions = videoCodecOptions;
    }

    public List<CodecOption> getAudioCodecOptions() {
        return audioCodecOptions;
    }

    public void setAudioCodecOptions(List<CodecOption> audioCodecOptions) {
        this.audioCodecOptions = audioCodecOptions;
    }

    public String getVideoEncoder() {
        return videoEncoder;
    }

    public void setVideoEncoder(String videoEncoder) {
        this.videoEncoder = videoEncoder;
    }

    public String getAudioEncoder() {
        return audioEncoder;
    }

    public void setAudioEncoder(String audioEncoder) {
        this.audioEncoder = audioEncoder;
    }

    public void setPowerOffScreenOnClose(boolean powerOffScreenOnClose) {
        this.powerOffScreenOnClose = powerOffScreenOnClose;
    }

    public boolean getPowerOffScreenOnClose() {
        return this.powerOffScreenOnClose;
    }

    public boolean getClipboardAutosync() {
        return clipboardAutosync;
    }

    public void setClipboardAutosync(boolean clipboardAutosync) {
        this.clipboardAutosync = clipboardAutosync;
    }

    public boolean getDownsizeOnError() {
        return downsizeOnError;
    }

    public void setDownsizeOnError(boolean downsizeOnError) {
        this.downsizeOnError = downsizeOnError;
    }

    public boolean getCleanup() {
        return cleanup;
    }

    public void setCleanup(boolean cleanup) {
        this.cleanup = cleanup;
    }

    public boolean getPowerOn() {
        return powerOn;
    }

    public void setPowerOn(boolean powerOn) {
        this.powerOn = powerOn;
    }

    public boolean getListEncoders() {
        return listEncoders;
    }

    public void setListEncoders(boolean listEncoders) {
        this.listEncoders = listEncoders;
    }

    public boolean getListDisplays() {
        return listDisplays;
    }

    public void setListDisplays(boolean listDisplays) {
        this.listDisplays = listDisplays;
    }

    public boolean getSendDeviceMeta() {
        return sendDeviceMeta;
    }

    public void setSendDeviceMeta(boolean sendDeviceMeta) {
        this.sendDeviceMeta = sendDeviceMeta;
    }

    public boolean getSendFrameMeta() {
        return sendFrameMeta;
    }

    public void setSendFrameMeta(boolean sendFrameMeta) {
        this.sendFrameMeta = sendFrameMeta;
    }

    public boolean getSendDummyByte() {
        return sendDummyByte;
    }

    public void setSendDummyByte(boolean sendDummyByte) {
        this.sendDummyByte = sendDummyByte;
    }

    public boolean getSendCodecMeta() {
        return sendCodecMeta;
    }

    public void setSendCodecMeta(boolean sendCodecMeta) {
        this.sendCodecMeta = sendCodecMeta;
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
}
