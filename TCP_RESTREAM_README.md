# TCP Restream Feature

This feature allows scrcpy to stream raw H.264/H.265 video packets to a TCP socket, enabling external applications (like Python scripts using PyAV) to efficiently sample and process the video stream.

## Usage

```bash
scrcpy --tcp-restream PORT [--record FILE] [--no-control]
```

### Options

- `--tcp-restream PORT`: Enable TCP restreaming on the specified port (e.g., `8080`)
- Implicitly disables video and audio playback (no window, no audio output)
- Can be combined with `--record` to simultaneously record and stream
- Can be combined with `--no-control` to disable device control

### Examples

**Basic restreaming:**
```bash
scrcpy --tcp-restream 8080
```

**Restream + Record:**
```bash
scrcpy --tcp-restream 8080 --record video.mp4 --no-control
```

**Custom video settings:**
```bash
scrcpy --tcp-restream 8080 --video-codec=h265 --max-size=1920 --max-fps=30
```

## Protocol

The TCP sink uses scrcpy's standard wire protocol:

### Initial Handshake (sent once on connection)
- **4 bytes**: Codec ID (big-endian)
  - `0x68323634` = H.264
  - `0x68323635` = H.265
- **8 bytes**: Width (4 bytes) + Height (4 bytes), both big-endian

### Each Packet
- **8 bytes**: PTS with flags (big-endian)
  - Bit 63: Config packet flag
  - Bit 62: Key frame flag
  - Bits 0-61: PTS value in microseconds
- **4 bytes**: Packet size (big-endian)
- **N bytes**: Raw H.264/H.265 packet data

## Python Client Example

### Simple Test Client

A basic test client is provided in `test_tcp_client.py`:

```bash
# Start scrcpy with TCP restream
scrcpy --tcp-restream 8080

# In another terminal, run the test client
python3 test_tcp_client.py 8080
```

The test client will display:
- Codec information
- Resolution
- Packet statistics (packet count, keyframes, data received)
- Real-time frame decoding (requires PyAV)

**Interactive Features:**
- Press `SPACE` to capture a screenshot of the current frame
- Press `q` to quit
- Press `Ctrl+C` to force quit

Screenshots are saved as `test_screenshot_1.png`, `test_screenshot_2.png`, etc.

**Requirements:**
```bash
pip install av Pillow
```

**Quick Demo:**
```bash
./demo_screenshots.sh
```

### PyAV Decoding Example

For actual video decoding with PyAV:

```python
import socket
import struct
import av

def decode_scrcpy_stream(port=8080):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('localhost', port))
    
    # Read codec ID
    codec_id = struct.unpack('>I', sock.recv(4))[0]
    codec_name = 'h264' if codec_id == 0x68323634 else 'h265'
    
    # Read video size
    width, height = struct.unpack('>II', sock.recv(8))
    print(f'Stream: {codec_name} {width}x{height}')
    
    # Create PyAV decoder
    codec = av.CodecContext.create(codec_name, 'r')
    
    while True:
        # Read packet header
        header = sock.recv(12)
        if len(header) < 12:
            break
        
        pts_flags = struct.unpack('>Q', header[:8])[0]
        size = struct.unpack('>I', header[8:12])[0]
        
        # Read packet data
        data = b''
        while len(data) < size:
            chunk = sock.recv(size - len(data))
            if not chunk:
                break
            data += chunk
        
        # Decode with PyAV
        packet = av.Packet(data)
        frames = codec.decode(packet)
        
        for frame in frames:
            # frame is a VideoFrame object
            img = frame.to_ndarray(format='rgb24')  # Convert to numpy array
            # Process image (e.g., display, analyze, save)...
```

## Architecture

The TCP restream feature is implemented as a **packet sink** that runs in a background thread:

1. **Initialization**: Opens a TCP server socket on the specified port
2. **Connection**: Accepts one client connection at a time
3. **Handshake**: Sends codec information to the client
4. **Streaming**: Forwards H.264/H.265 packets from the demuxer to the client
5. **Reconnection**: If client disconnects, waits for a new connection

### Data Flow

```
Android Device → Demuxer (packet source)
                    ├─→ Recorder (if --record) → MP4 file
                    ├─→ TCP Sink → TCP Socket → Python Client
                    └─→ (Video/Audio decoders skipped with --tcp-restream)
```

## Performance Notes

- **No re-encoding**: Packets are copied directly from the stream (zero quality loss)
- **Low overhead**: Minimal CPU usage as packets are just forwarded
- **Thread-safe**: Packet queue ensures no blocking of the demuxer
- **Drop policy**: If no client is connected, packets are dropped (not queued)

## Limitations

- **One client at a time**: Only one TCP client can connect simultaneously
- **Video only**: Currently only video stream is restreamed (audio support could be added)
- **No buffering**: Packets are dropped when no client is connected
- **Config packet caching**: Late-connecting clients receive the cached config packet automatically

## Building

The feature is included in the standard build:

```bash
meson setup builddir --buildtype=release
ninja -C builddir
```

## Testing

1. **Start scrcpy with TCP restream:**
   ```bash
   ./builddir/app/scrcpy --tcp-restream 8080
   ```

2. **Run the test client:**
   ```bash
   python3 test_tcp_client.py 8080
   ```

3. **Verify output shows:**
   - Successful connection
   - Correct codec and resolution
   - Packet statistics updating

## Troubleshooting

**"Could not listen on port XXX"**
- Port is already in use
- Try a different port number
- Check with: `lsof -i :PORT`

**"Client disconnected" immediately**
- Client might not be reading data fast enough
- Check client implementation
- Try increasing network buffer sizes

**No packets received**
- Ensure scrcpy is actually streaming video
- Check that `--video` is not set to false
- Verify device is connected and streaming

**"Decoded: 0" (no frames decoded)**
- This was fixed in v2
- If still occurring, run `python3 test_config_packet.py 8080` to verify config packets
- Ensure you're using the latest build
- Config packets should be received automatically on connection

**Audio still playing from device**
- This was fixed in v2
- Ensure you're using the latest build with both audio and video playback disabled
- Verify with: no audio output from laptop and no scrcpy window

## Use Cases

- **Automated testing**: Analyze video frames in CI/CD pipelines
- **Computer vision**: Process frames with OpenCV/PyTorch in real-time
- **Quality analysis**: Measure video quality metrics
- **Screenshot automation**: Capture specific frames programmatically
- **Remote monitoring**: Forward stream to another machine
- **Video processing**: Apply filters, overlays, or transformations

## Changelog

### v2 - Config Packet Caching & Audio Playback Fix (2025-11-26)

**Fixed: Frame Decoding Issue**
- Problem: PyAV decoder was receiving packets but not decoding frames (`Decoded: 0`)
- Root cause: Config packets (SPS/PPS) were not being sent to late-connecting clients
- Solution: TCP sink now caches the latest config packet and sends it to new clients
- Result: Decoder receives config packets immediately and can decode frames

**Fixed: Audio Playback Still Active**
- Problem: Audio from device was playing despite `--tcp-restream` flag
- Root cause: Only `video_playback` was being disabled, not `audio_playback`
- Solution: Both video and audio playback are now disabled
- Result: Truly headless operation (no window, no audio)

**Testing Tools:**
- Added `test_config_packet.py` - Diagnostic tool to verify config packet reception
- Enhanced `test_tcp_client.py` - Now shows decoded frame count

### v1 - Initial Implementation (2025-11-26)

**Features:**
- TCP server streaming H.264/H.265 packets
- Background thread for non-blocking operation
- Scrcpy wire protocol implementation
- Python client with PyAV decoding
- Interactive screenshot capture (spacebar)
- Simultaneous recording support

## Future Enhancements

Potential improvements:
- Audio stream support
- Multiple simultaneous client connections
- Configurable packet buffering
- WebSocket protocol support
- Frame filtering options (e.g., keyframes only)
