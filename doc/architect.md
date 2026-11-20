# scrcpy Architect Documentation

## Table of Contents

1. [Overview](#overview)
2. [High-Level Architecture](#high-level-architecture)
3. [Component Architecture](#component-architecture)
4. [Communication Protocols](#communication-protocols)
5. [Threading Model](#threading-model)
6. [Data Flow](#data-flow)
7. [Dependencies](#dependencies)
8. [Platform-Specific Features](#platform-specific-features)
9. [Configuration System](#configuration-system)
10. [Error Handling](#error-handling)
11. [Performance Considerations](#performance-considerations)
12. [Extension Points](#extension-points)

## Overview

scrcpy (screen copy) is a cross-platform application that provides display and control of Android devices connected via USB or TCP/IP. The application consists of two main components:

- **Client**: A native application (C) running on the host computer (Linux, Windows, macOS)
- **Server**: A Java application running on the Android device

The architecture is designed for minimal latency, high performance, and cross-platform compatibility while maintaining a clean separation between the client and server components.

### Key Design Principles

- **Minimal Latency**: Direct streaming without unnecessary buffering
- **Cross-Platform**: Consistent behavior across Linux, Windows, and macOS
- **No Root Required**: Uses standard Android APIs and ADB
- **Modular Design**: Clear separation of concerns between components
- **Resource Efficient**: Optimized memory and CPU usage

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        Host Computer                            │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │                    scrcpy Client (C)                        ││
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  ││
│  │  │   Screen    │  │    Audio    │  │      Controller     │  ││
│  │  │   Display   │  │   Player    │  │   (Input Events)    │  ││
│  │  └─────────────┘  └─────────────┘  └─────────────────────┘  ││
│  │           ▲              ▲                      │           ││
│  │  ┌─────────────┐  ┌─────────────┐               │           ││
│  │  │   Video     │  │    Audio    │               │           ││
│  │  │   Decoder   │  │   Decoder   │               │           ││
│  │  └─────────────┘  └─────────────┘               │           ││
│  │           ▲              ▲                      │           ││
│  │  ┌─────────────┐  ┌─────────────┐               │           ││
│  │  │   Video     │  │    Audio    │               │           ││
│  │  │  Demuxer    │  │  Demuxer    │               │           ││
│  │  └─────────────┘  └─────────────┘               │           ││
│  │           ▲              ▲                      ▼           ││
│  └───────────┼──────────────┼──────────────────────┼───────────┘│
│              │              │                      │            │
│         ┌────┼──────────────┼──────────────────────┼────┐       │
│         │    │              │                      │    │       │
│         │ Video Socket   Audio Socket        Control Socket │   │
│         │    │              │                      │    │       │
│         └────┼──────────────┼──────────────────────┼────┘       │
└──────────────┼──────────────┼──────────────────────┼────────────┘
               │              │                      │
               │              │                      │
┌──────────────┼──────────────┼──────────────────────┼────────────┐
│              │              │                      │            │
│              ▼              ▼                      ▲            │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐      │
│  │   Video     │  │    Audio    │  │     Controller      │      │
│  │  Encoder    │  │   Encoder   │  │  (Event Injection)  │      │
│  └─────────────┘  └─────────────┘  └─────────────────────┘      │
│           ▲              ▲                      ▲               │
│  ┌─────────────┐  ┌─────────────┐               │               │
│  │   Screen    │  │    Audio    │               │               │
│  │  Capture    │  │   Capture   │               │               │
│  └─────────────┘  └─────────────┘               │               │
│                                                 │               │
│              scrcpy Server (Java)               │               │
│                                                 │               │
│                    Android Device               │               │
│                                           ┌─────────────┐       │
│                                           │   Android   │       │
│                                           │   System    │       │
│                                           │  Services   │       │
│                                           └─────────────┘       │
└─────────────────────────────────────────────────────────────────┘
```

## Component Architecture

### Client Components

The client application is organized around a central `scrcpy` structure that coordinates all components:

```c
struct scrcpy {
    struct sc_server server;              // Server management
    struct sc_screen screen;              // Display rendering
    struct sc_audio_player audio_player;  // Audio playback
    struct sc_demuxer video_demuxer;      // Video stream parsing
    struct sc_demuxer audio_demuxer;      // Audio stream parsing
    struct sc_decoder video_decoder;      // Video frame decoding
    struct sc_decoder audio_decoder;      // Audio frame decoding
    struct sc_recorder recorder;          // Recording functionality
    struct sc_delay_buffer video_buffer;  // Video buffering
    struct sc_controller controller;      // Input event handling
    struct sc_file_pusher file_pusher;    // File transfer
    // Platform-specific components...
};
```

#### Core Components

1. **Server Manager** (`sc_server`)
   - Manages ADB connection and server deployment
   - Handles server process lifecycle
   - Establishes network tunnels

2. **Screen Display** (`sc_screen`)
   - SDL-based window management
   - OpenGL rendering pipeline
   - Frame presentation and scaling

3. **Audio Player** (`sc_audio_player`)
   - SDL audio output management
   - Audio buffering and synchronization
   - Sample rate conversion

4. **Demuxers** (`sc_demuxer`)
   - Parse incoming video/audio streams
   - Extract packets from network data
   - Handle stream metadata

5. **Decoders** (`sc_decoder`)
   - FFmpeg-based video/audio decoding
   - Hardware acceleration support
   - Frame format conversion

6. **Controller** (`sc_controller`)
   - Input event capture and processing
   - Control message serialization
   - Bidirectional communication with server

#### Optional Components

1. **Recorder** (`sc_recorder`)
   - Video/audio recording to file
   - Multiple format support (MP4, MKV)
   - Asynchronous muxing

2. **V4L2 Sink** (`sc_v4l2_sink`) - Linux only
   - Virtual camera device creation
   - Video frame forwarding to V4L2

3. **USB/HID Support** (`sc_usb`, `sc_aoa`) - Platform dependent
   - Direct USB communication
   - HID device emulation
   - OTG mode support

### Server Components

The server is a Java application that runs on the Android device:

```java
public class Server {
    // Core streaming components
    private List<AsyncProcessor> asyncProcessors;
    private DesktopConnection connection;
    private Controller controller;

    // Capture components
    private SurfaceCapture surfaceCapture;
    private AudioCapture audioCapture;

    // Encoding components
    private SurfaceEncoder surfaceEncoder;
    private AudioEncoder audioEncoder;
}
```

#### Key Server Components

1. **Surface Capture**
   - `ScreenCapture`: Display content capture
   - `CameraCapture`: Camera stream capture
   - `NewDisplayCapture`: Virtual display creation

2. **Audio Capture**
   - `AudioDirectCapture`: Direct audio source capture
   - `AudioPlaybackCapture`: System audio capture

3. **Encoders**
   - `SurfaceEncoder`: Video encoding using MediaCodec
   - `AudioEncoder`: Audio encoding (OPUS, AAC, RAW)

4. **Controller**
   - Input event injection
   - System service interaction
   - Device state management

5. **Connection Management**
   - Socket management
   - Protocol handling
   - Error recovery

## Communication Protocols

scrcpy uses a custom protocol over TCP sockets for communication between client and server. The protocol is designed for efficiency and minimal overhead.

### Socket Architecture

The communication uses up to 3 separate TCP sockets:

1. **Video Socket**: Streams encoded video data (H.264, H.265, AV1)
2. **Audio Socket**: Streams encoded audio data (OPUS, AAC, RAW)
3. **Control Socket**: Bidirectional control messages and device events

```
Client                                    Server
┌─────────────────┐                       ┌─────────────────┐
│                 │ ──── Video Socket ──→ │   Video         │
│                 │                       │   Encoder       │
│                 │ ──── Audio Socket ──→ │   Audio         │
│   Demuxers/     │                       │   Encoder       │
│   Decoders      │                       │                 │
│                 │ ←─── Control Socket ─→│   Controller    │
│   Controller    │                       │                 │
└─────────────────┘                       └─────────────────┘
```

### Protocol Details

#### Connection Establishment

1. Client establishes ADB connection to device
2. Client pushes server JAR to device
3. Client starts server process via ADB
4. Server creates listening sockets
5. Client connects to server sockets in order:
   - First socket (video or audio, depending on configuration)
   - Additional sockets as needed
6. Server sends device metadata on first socket

#### Video/Audio Stream Protocol

Each video/audio packet follows this format:

```
┌─────────────────────────────────────────────────────────────┐
│                    Codec Metadata (once)                    │
├─────────────────────────────────────────────────────────────┤
│ Video: codec_id(4) + width(4) + height(4) = 12 bytes        │
│ Audio: codec_id(4) = 4 bytes                                │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                    Frame Header (per packet)                │
├─────────────────────────────────────────────────────────────┤
│ config_flag(1) + key_frame_flag(1) + pts(8) + size(4)       │
│ = 12 bytes total                                            │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                    Packet Data                              │
├─────────────────────────────────────────────────────────────┤
│ Raw encoded data (size specified in header)                 │
└─────────────────────────────────────────────────────────────┘
```

#### Control Message Protocol

Control messages are variable-length binary messages:

```c
// Client to Server Control Messages
enum sc_control_msg_type {
    SC_CONTROL_MSG_TYPE_INJECT_KEYCODE,
    SC_CONTROL_MSG_TYPE_INJECT_TEXT,
    SC_CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT,
    SC_CONTROL_MSG_TYPE_INJECT_SCROLL_EVENT,
    SC_CONTROL_MSG_TYPE_BACK_OR_SCREEN_ON,
    SC_CONTROL_MSG_TYPE_EXPAND_NOTIFICATION_PANEL,
    SC_CONTROL_MSG_TYPE_EXPAND_SETTINGS_PANEL,
    SC_CONTROL_MSG_TYPE_COLLAPSE_PANELS,
    SC_CONTROL_MSG_TYPE_GET_CLIPBOARD,
    SC_CONTROL_MSG_TYPE_SET_CLIPBOARD,
    SC_CONTROL_MSG_TYPE_SET_DISPLAY_POWER,
    SC_CONTROL_MSG_TYPE_ROTATE_DEVICE,
    SC_CONTROL_MSG_TYPE_UHID_CREATE,
    SC_CONTROL_MSG_TYPE_UHID_INPUT,
    SC_CONTROL_MSG_TYPE_UHID_DESTROY,
    SC_CONTROL_MSG_TYPE_OPEN_HARD_KEYBOARD_SETTINGS,
    SC_CONTROL_MSG_TYPE_START_APP,
    SC_CONTROL_MSG_TYPE_RESET_VIDEO,
};

// Server to Client Device Messages
enum sc_device_msg_type {
    DEVICE_MSG_TYPE_CLIPBOARD,
    DEVICE_MSG_TYPE_ACK_CLIPBOARD,
    DEVICE_MSG_TYPE_UHID_OUTPUT,
};
```

Each message starts with a type byte followed by type-specific data.

### Network Tunneling

scrcpy supports multiple connection methods:

1. **ADB Reverse Tunnel** (default): Server listens, client connects
2. **ADB Forward Tunnel**: Client listens, server connects
3. **TCP/IP Direct**: Direct network connection without ADB

## Threading Model

scrcpy uses a multi-threaded architecture to achieve low latency and responsive performance.

### Client Threading Architecture

```
Main Thread
├── SDL Event Loop
├── UI Rendering
├── Input Processing
└── Component Coordination

Server Thread
├── ADB Process Management
├── Server Deployment
└── Connection Establishment

Video Demuxer Thread
├── Socket Reading
├── Packet Parsing
└── Frame Distribution

Audio Demuxer Thread
├── Socket Reading
├── Packet Parsing
└── Frame Distribution

Video Decoder Thread
├── FFmpeg Decoding
├── Frame Processing
└── Display Queue

Audio Decoder Thread
├── FFmpeg Decoding
├── Sample Processing
└── Audio Queue

Controller Thread
├── Control Socket I/O
├── Message Serialization
└── Event Processing

Recorder Thread (optional)
├── Muxing Operations
├── File I/O
└── Format Conversion

File Pusher Thread (optional)
├── ADB File Transfer
└── Progress Monitoring
```

### Server Threading Architecture

```java
Main Thread (Looper)
├── Component Coordination
├── Lifecycle Management
└── Error Handling

Video Encoder Thread
├── Surface Capture
├── MediaCodec Encoding
└── Socket Writing

Audio Encoder Thread
├── Audio Capture
├── MediaCodec Encoding
└── Socket Writing

Controller Thread
├── Control Socket Reading
├── Event Injection
└── System Interaction

Cleanup Thread
├── Resource Management
├── Process Monitoring
└── Graceful Shutdown
```

### Synchronization Mechanisms

1. **Frame Sources/Sinks**: Producer-consumer pattern for frame data
2. **SDL Events**: Cross-thread communication via event queue
3. **Mutexes/Conditions**: Thread synchronization primitives
4. **Atomic Operations**: Lock-free data sharing where possible
5. **Interruption System**: Graceful thread termination

## Data Flow

### Video Data Flow

```
Android Device                           Host Computer
┌─────────────────┐                     ┌─────────────────┐
│ Display/Camera  │                     │                 │
│       │         │                     │                 │
│       ▼         │                     │                 │
│ Surface Capture │                     │                 │
│       │         │                     │                 │
│       ▼         │                     │                 │
│ MediaCodec      │ ──── Network ────→  │ Video Demuxer   │
│ (H264/H265/AV1) │     (TCP Socket)    │       │         │
│                 │                     │       ▼         │
│                 │                     │ Video Decoder   │
│                 │                     │ (FFmpeg)        │
│                 │                     │       │         │
│                 │                     │       ▼         │
│                 │                     │ Delay Buffer    │
│                 │                     │ (optional)      │
│                 │                     │       │         │
│                 │                     │       ▼         │
│                 │                     │ Screen Display  │
│                 │                     │ (SDL + OpenGL)  │
│                 │                     │       │         │
│                 │                     │       ▼         │
│                 │                     │ V4L2 Sink       │
│                 │                     │ (Linux only)    │
└─────────────────┘                     └─────────────────┘
```

### Audio Data Flow

```
Android Device                           Host Computer
┌─────────────────┐                     ┌─────────────────┐
│ Audio Source    │                     │                 │
│ (Output/Mic)    │                     │                 │
│       │         │                     │                 │
│       ▼         │                     │                 │
│ Audio Capture   │                     │                 │
│       │         │                     │                 │
│       ▼         │                     │                 │
│ MediaCodec      │ ──── Network ────→  │ Audio Demuxer   │
│ (OPUS/AAC/RAW)  │     (TCP Socket)    │       │         │
│                 │                     │       ▼         │
│                 │                     │ Audio Decoder   │
│                 │                     │ (FFmpeg)        │
│                 │                     │       │         │
│                 │                     │       ▼         │
│                 │                     │ Audio Regulator │
│                 │                     │ (Buffering)     │
│                 │                     │       │         │
│                 │                     │       ▼         │
│                 │                     │ Audio Player    │
│                 │                     │ (SDL Audio)     │
└─────────────────┘                     └─────────────────┘
```

### Control Data Flow

```
Host Computer                            Android Device
┌─────────────────┐                     ┌──────────────────┐
│ Input Events    │                     │                  │
│ (Mouse/Keyboard)│                     │                  │
│       │         │                     │                  │
│       ▼         │                     │                  │
│ Input Manager   │                     │                  │
│       │         │                     │                  │
│       ▼         │                     │                  │
│ Controller      │ ──── Network ────→  │ Controller       │
│ (Serialization) │     (TCP Socket)    │ (Deserialization)│
│       │         │                     │       │          │
│       ▼         │                     │       ▼          │
│ Control Socket  │                     │ Event Injection  │
│                 │                     │ (Android APIs)   │
│                 │                     │       │          │
│                 │ ←──── Network ────  │       ▼          │
│                 │     (TCP Socket)    │ Android System   │
│ Clipboard/      │                     │                  │
│ Device Events   │                     │                  │
└─────────────────┘                     └──────────────────┘
```

### Frame Source/Sink Pattern

scrcpy uses a producer-consumer pattern for data flow:

```c
// Frame source produces frames
struct sc_frame_source {
    struct sc_frame_source_ops *ops;
    // List of sinks to send frames to
};

// Frame sink consumes frames
struct sc_frame_sink {
    struct sc_frame_sink_ops *ops;
    // Processing logic for received frames
};

// Example: Video decoder produces frames for screen display
sc_frame_source_add_sink(&video_decoder.frame_source, &screen.frame_sink);
```

## Dependencies

### Client Dependencies

#### Core Dependencies

1. **SDL2** (>= 2.0.5)
   - Cross-platform window management
   - Input event handling
   - Audio output
   - Threading primitives
   - OpenGL context management

2. **FFmpeg Libraries**
   - `libavformat` (>= 57.33): Container format handling
   - `libavcodec` (>= 57.37): Video/audio codec support
   - `libavutil`: Utility functions and data structures
   - `libswresample`: Audio resampling
   - `libavdevice`: V4L2 support (Linux only)

#### Platform-Specific Dependencies

**Linux:**
- `libusb-1.0`: USB/HID support
- V4L2 kernel support: Virtual camera functionality

**Windows:**
- `ws2_32`: Windows socket library
- `mingw32`: MinGW runtime (when using MinGW)

**macOS:**
- System frameworks via SDL2

#### Build System Dependencies

1. **Meson** (>= 0.49): Primary build system
2. **pkg-config**: Dependency discovery
3. **C11 Compiler**: GCC, Clang, or MSVC

### Server Dependencies

#### Android SDK Dependencies

1. **Android SDK** (API Level 21+)
   - `MediaCodec`: Hardware-accelerated encoding
   - `AudioRecord`/`AudioPlayback`: Audio capture
   - `SurfaceView`: Display capture
   - `InputManager`: Event injection

2. **Build Tools**
   - **Gradle**: Build automation
   - **Android Build Tools**: APK compilation
   - **ProGuard**: Code obfuscation (optional)

#### Runtime Dependencies

1. **Android System Services**
   - `SurfaceFlinger`: Display management
   - `AudioFlinger`: Audio system
   - `InputMethodManager`: Input handling
   - `WindowManager`: Display configuration

2. **Hardware Abstraction Layer (HAL)**
   - Camera HAL: Camera capture support
   - Audio HAL: Audio capture/playback
   - Display HAL: Screen capture

### Dependency Management

#### Client Build Configuration

```meson
dependencies = [
    dependency('libavformat', version: '>= 57.33', static: static),
    dependency('libavcodec', version: '>= 57.37', static: static),
    dependency('libavutil', static: static),
    dependency('libswresample', static: static),
    dependency('sdl2', version: '>= 2.0.5', static: static),
]

# Optional dependencies
if v4l2_support
    dependencies += dependency('libavdevice', static: static)
endif

if usb_support
    dependencies += dependency('libusb-1.0', static: static)
endif
```

#### Server Build Configuration

```gradle
android {
    compileSdk 35
    defaultConfig {
        minSdkVersion 21
        targetSdkVersion 35
    }
}

dependencies {
    testImplementation 'junit:junit:4.13.2'
}
```

## Platform-Specific Features

### Linux-Specific Features

#### V4L2 Virtual Camera Support

Linux systems can expose the scrcpy video stream as a virtual camera device:

```c
#ifdef HAVE_V4L2
struct sc_v4l2_sink {
    char *device_path;
    AVFormatContext *format_ctx;
    AVStream *stream;
    // V4L2 device management
};
#endif
```

**Features:**
- Creates `/dev/videoN` device
- Compatible with applications expecting camera input
- Supports multiple pixel formats
- Hardware-accelerated when available

#### USB/HID Support

Direct USB communication for OTG mode:

```c
#ifdef HAVE_USB
struct sc_usb {
    libusb_context *context;
    libusb_device_handle *handle;
    // USB device management
};

struct sc_aoa {
    struct sc_usb *usb;
    // Android Open Accessory protocol
};
#endif
```

**Capabilities:**
- Direct USB communication without ADB
- HID keyboard/mouse emulation
- Gamepad support via AOA protocol
- Lower latency input handling

### Windows-Specific Features

#### Process Management

Windows uses different APIs for process handling:

```c
#ifdef _WIN32
// Windows-specific process implementation
struct sc_process_windows {
    HANDLE handle;
    DWORD pid;
    PROCESS_INFORMATION pi;
};
#endif
```

**Features:**
- Unicode command line handling
- Windows service integration
- Console window management
- Resource cleanup via Windows APIs

#### Network Stack

Windows requires specific socket initialization:

```c
#ifdef _WIN32
bool net_init(void) {
    WSADATA wsa_data;
    int res = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    return res == 0;
}
#endif
```

### macOS-Specific Features

#### Framework Integration

macOS builds integrate with system frameworks through SDL2:

```c
#ifdef __APPLE__
// macOS-specific configurations
#define _DARWIN_C_SOURCE
// Framework integration handled by SDL2
#endif
```

**Features:**
- Native window management
- Retina display support
- macOS-style keyboard shortcuts
- System notification integration

### Cross-Platform Abstractions

#### File System Operations

```c
// Platform-agnostic file operations
bool sc_file_copy(const char *from, const char *to);
bool sc_file_move(const char *from, const char *to);
char *sc_file_get_local_path(const char *name);

// Platform-specific implementations
#ifdef _WIN32
    // Windows file operations
#else
    // Unix file operations
#endif
```

#### Threading Primitives

All threading is abstracted through SDL2:

```c
// Cross-platform thread management
typedef struct {
    SDL_Thread *thread;
} sc_thread;

typedef struct {
    SDL_mutex *mutex;
#ifndef NDEBUG
    atomic_uint_least32_t locker;
#endif
} sc_mutex;
```

## Configuration System

### Command Line Interface

scrcpy uses a comprehensive CLI system for configuration:

```c
struct scrcpy_options {
    // Connection options
    const char *serial;
    const char *tcpip;
    uint16_t port_range_first;
    uint16_t port_range_last;

    // Video options
    enum sc_codec video_codec;
    const char *video_encoder;
    uint32_t video_bit_rate;
    uint16_t max_size;
    uint32_t max_fps;

    // Audio options
    enum sc_codec audio_codec;
    const char *audio_encoder;
    uint32_t audio_bit_rate;
    enum sc_audio_source audio_source;

    // Control options
    enum sc_keyboard_input_mode keyboard_input_mode;
    enum sc_mouse_input_mode mouse_input_mode;
    enum sc_gamepad_input_mode gamepad_input_mode;

    // Display options
    bool fullscreen;
    bool always_on_top;
    bool window_borderless;
    const char *window_title;

    // Recording options
    const char *record_filename;
    enum sc_record_format record_format;

    // Platform-specific options
#ifdef HAVE_V4L2
    const char *v4l2_device;
#endif
#ifdef HAVE_USB
    bool otg;
#endif
};
```

### Configuration Validation

Options are validated during parsing:

```c
bool scrcpy_parse_args(struct scrcpy_args *args, int argc, char *argv[]) {
    // Parse command line arguments
    // Validate option combinations
    // Set defaults for unspecified options
    // Handle platform-specific options
}
```

**Validation Rules:**
- Codec compatibility checks
- Port range validation
- File path accessibility
- Hardware capability detection
- Platform feature availability

### Default Configuration

```c
const struct scrcpy_options scrcpy_options_default = {
    .video_codec = SC_CODEC_H264,
    .audio_codec = SC_CODEC_OPUS,
    .video_bit_rate = 8000000,  // 8 Mbps
    .audio_bit_rate = 128000,   // 128 Kbps
    .max_size = 0,              // No limit
    .max_fps = 0,               // No limit
    .port_range_first = 27183,
    .port_range_last = 27199,
    .keyboard_input_mode = SC_KEYBOARD_INPUT_MODE_AUTO,
    .mouse_input_mode = SC_MOUSE_INPUT_MODE_AUTO,
    .gamepad_input_mode = SC_GAMEPAD_INPUT_MODE_DISABLED,
    // ... other defaults
};
```

### Runtime Configuration

Some options can be changed at runtime:

1. **Display Settings**: Window size, fullscreen mode
2. **Input Modes**: Keyboard/mouse input methods
3. **Recording**: Start/stop recording
4. **Audio**: Volume control, mute/unmute

### Environment Variables

scrcpy respects several environment variables:

- `SCRCPY_SERVER_PATH`: Custom server JAR location
- `SCRCPY_ICON_PATH`: Custom application icon
- `ADB`: Custom ADB executable path
- Platform-specific variables for debugging

## Error Handling

### Error Propagation Strategy

scrcpy uses a multi-layered error handling approach:

1. **Component Level**: Each component handles its own errors
2. **Thread Level**: Threads communicate errors via events
3. **Application Level**: Main event loop handles critical errors
4. **User Level**: Meaningful error messages and recovery suggestions

### Error Categories

#### Connection Errors

```c
enum scrcpy_exit_code {
    SCRCPY_EXIT_SUCCESS,      // Normal termination
    SCRCPY_EXIT_FAILURE,      // Connection/setup failure
    SCRCPY_EXIT_DISCONNECTED, // Device disconnected during operation
};
```

**Handling:**
- ADB connection failures: Retry with exponential backoff
- Network timeouts: Graceful degradation
- Server startup failures: Detailed error reporting

#### Stream Errors

**Video Stream:**
- Decoder errors: Attempt recovery or fallback codec
- Network interruption: Buffer management and reconnection
- Hardware acceleration failures: Software fallback

**Audio Stream:**
- Capture failures: Disable audio or switch source
- Playback issues: Buffer adjustment and format conversion
- Synchronization problems: Clock adjustment

#### Input Errors

- Invalid control messages: Log and ignore
- Injection failures: Retry with different method
- Permission issues: User notification and guidance

### Recovery Mechanisms

#### Automatic Recovery

1. **Network Reconnection**: Automatic retry with backoff
2. **Stream Recovery**: Codec reset and re-initialization
3. **Buffer Management**: Adaptive buffering based on conditions

#### Graceful Degradation

1. **Feature Fallbacks**: Disable non-essential features on error
2. **Quality Reduction**: Lower bitrate/resolution on network issues
3. **Input Method Switching**: Fallback input methods

### Error Reporting

#### Logging System

```c
enum sc_log_level {
    SC_LOG_LEVEL_VERBOSE,
    SC_LOG_LEVEL_DEBUG,
    SC_LOG_LEVEL_INFO,
    SC_LOG_LEVEL_WARN,
    SC_LOG_LEVEL_ERROR,
};

// Structured logging with context
LOGE("Video decoder error: %s (codec=%s)", error_msg, codec_name);
```

#### User Feedback

- Clear error messages with actionable advice
- Progress indicators for long operations
- Status information in window title
- System notifications for important events

## Performance Considerations

### Latency Optimization

#### Video Pipeline

1. **Minimal Buffering**: Direct frame presentation without buffering
2. **Hardware Acceleration**: GPU-accelerated decoding when available
3. **Zero-Copy Operations**: Minimize memory copies in pipeline
4. **Adaptive Quality**: Dynamic bitrate adjustment

#### Audio Pipeline

1. **Low-Latency Buffering**: Minimal audio buffer size
2. **Sample Rate Matching**: Avoid unnecessary resampling
3. **Clock Synchronization**: Precise timing alignment
4. **Adaptive Buffering**: Dynamic buffer adjustment

#### Input Handling

1. **Direct Event Processing**: Minimal input event latency
2. **Batch Processing**: Efficient event batching
3. **Priority Handling**: High-priority input events
4. **Hardware Acceleration**: USB HID for lowest latency

### Memory Management

#### Buffer Strategies

```c
// Circular buffer for audio samples
struct sc_audiobuf {
    uint8_t *data;
    size_t capacity;
    size_t head;
    size_t tail;
    // Lock-free operations where possible
};

// Frame buffer with reference counting
struct sc_frame_buffer {
    AVFrame *frame;
    atomic_int refs;
    // Automatic cleanup when refs reach zero
};
```

#### Resource Pooling

1. **Frame Pools**: Reuse decoded frames
2. **Buffer Pools**: Minimize allocation overhead
3. **Thread Pools**: Efficient thread management
4. **Connection Pools**: Reuse network connections

### CPU Optimization

#### Multi-threading Benefits

1. **Parallel Processing**: Separate threads for I/O and computation
2. **Pipeline Parallelism**: Overlapped encoding/decoding operations
3. **Load Balancing**: Distribute work across available cores
4. **Priority Scheduling**: Critical threads get higher priority

#### Algorithm Optimization

1. **SIMD Instructions**: Vectorized operations where applicable
2. **Cache-Friendly Access**: Optimize memory access patterns
3. **Branch Prediction**: Minimize conditional branches in hot paths
4. **Compiler Optimizations**: Profile-guided optimization

### Network Optimization

#### Protocol Efficiency

1. **Binary Protocol**: Compact message format
2. **Compression**: Optional stream compression
3. **Multiplexing**: Multiple streams over single connection
4. **Flow Control**: Adaptive transmission rate

#### Connection Management

1. **Keep-Alive**: Maintain persistent connections
2. **Reconnection Logic**: Intelligent retry strategies
3. **Bandwidth Adaptation**: Adjust quality based on available bandwidth
4. **Error Recovery**: Fast recovery from network issues

## Extension Points

### Plugin Architecture

While scrcpy doesn't have a formal plugin system, it provides several extension points:

#### Custom Codecs

```c
// Codec registration system
struct sc_codec_ops {
    bool (*init)(struct sc_codec *codec, const struct sc_codec_params *params);
    bool (*decode)(struct sc_codec *codec, const AVPacket *packet);
    void (*destroy)(struct sc_codec *codec);
};
```

#### Input Methods

```c
// Input method interface
struct sc_input_method {
    bool (*init)(struct sc_input_method *method);
    bool (*inject_keycode)(struct sc_input_method *method, /* ... */);
    bool (*inject_text)(struct sc_input_method *method, const char *text);
    bool (*inject_touch)(struct sc_input_method *method, /* ... */);
    void (*destroy)(struct sc_input_method *method);
};
```

### Customization Points

#### Build-Time Configuration

```meson
# Custom feature flags
option('custom_feature', type: 'boolean', value: false,
       description: 'Enable custom feature')

# Custom dependency paths
option('custom_lib_path', type: 'string',
       description: 'Path to custom library')
```

#### Runtime Configuration

1. **Environment Variables**: Custom behavior via environment
2. **Configuration Files**: Future support for config files
3. **Command Line Extensions**: Additional option parsing
4. **Dynamic Loading**: Runtime library loading

### Integration Points

#### External Applications

1. **V4L2 Integration**: Virtual camera for other applications
2. **Recording Integration**: External video processing tools
3. **Automation Scripts**: Command-line interface for scripting
4. **IDE Integration**: Development tool integration

#### System Integration

1. **Desktop Environment**: Native window management
2. **System Services**: Background service operation
3. **Hardware Integration**: Custom hardware support
4. **Security Systems**: Enterprise security compliance

### Future Extension Possibilities

1. **WebRTC Support**: Browser-based client
2. **Cloud Integration**: Remote device access
3. **AI Integration**: Intelligent automation features
4. **Multi-Device Support**: Simultaneous device control
5. **Protocol Extensions**: Enhanced communication protocols

---

## Conclusion

The scrcpy architecture demonstrates a well-designed, modular system that achieves its goals of low-latency device mirroring and control. Key architectural strengths include:

- **Clean Separation**: Clear boundaries between client and server
- **Modular Design**: Independent, reusable components
- **Cross-Platform Support**: Consistent behavior across platforms
- **Performance Focus**: Optimized for minimal latency
- **Extensibility**: Multiple points for customization and extension

This architecture enables scrcpy to provide a responsive, efficient, and reliable Android device mirroring solution while maintaining code quality and maintainability.
