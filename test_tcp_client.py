#!/usr/bin/env python3
"""
Simple test client for scrcpy TCP restream feature.
Connects to localhost:8080 and receives H.264/H.265 packets.
Press SPACE to capture a screenshot.
"""

import socket
import struct
import sys
import threading
import select

try:
    import av
    HAVE_PYAV = True
except ImportError:
    HAVE_PYAV = False
    print("WARNING: PyAV not installed. Install with: pip install av")
    print("         Running in packet-only mode (no screenshots).")

try:
    from PIL import Image
    import numpy as np
    HAVE_PIL = True
except ImportError:
    HAVE_PIL = False
    if HAVE_PYAV:
        print("WARNING: Pillow not installed. Install with: pip install Pillow")
        print("         Screenshots will not be saved.")

def read_scrcpy_stream(host='localhost', port=8080):
    """
    Connect to scrcpy TCP restream and decode the stream protocol.
    Press SPACE to capture screenshot, 'q' to quit.
    """
    print(f"Connecting to {host}:{port}...")
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    
    try:
        sock.connect((host, port))
        print("Connected!")
        
        # Read codec ID (4 bytes)
        codec_id_bytes = sock.recv(4)
        if len(codec_id_bytes) < 4:
            print("ERROR: Could not read codec ID")
            return
        
        codec_id = struct.unpack('>I', codec_id_bytes)[0]
        codec_name = 'h264' if codec_id == 0x68323634 else 'h265' if codec_id == 0x68323635 else 'unknown'
        print(f"Codec ID: 0x{codec_id:08x} ({codec_name})")
        
        # Read video size (8 bytes)
        size_bytes = sock.recv(8)
        if len(size_bytes) < 8:
            print("ERROR: Could not read video size")
            return
        
        width, height = struct.unpack('>II', size_bytes)
        print(f"Resolution: {width}x{height}")
        
        # Create decoder if PyAV is available
        codec = None
        latest_frame = None
        screenshot_count = 0
        
        if HAVE_PYAV:
            try:
                codec = av.CodecContext.create(codec_name, 'r')
                print("\n✓ PyAV decoder initialized")
                
            except Exception as e:
                print(f"\nWARNING: Could not create decoder: {e}")
                print("Running in packet-only mode.")
                codec = None
        
        print("\nReceiving packets... (Ctrl+C to stop)")
        
        packet_count = 0
        key_frame_count = 0
        config_packet_count = 0
        total_bytes = 0
        decoded_frame_count = 0
        
        while True:
            # Read packet header (12 bytes)
            header = sock.recv(12)
            if len(header) < 12:
                print("\nConnection closed or incomplete header")
                break
            
            pts_flags = struct.unpack('>Q', header[:8])[0]
            packet_size = struct.unpack('>I', header[8:12])[0]
            
            # Parse flags
            is_config = (pts_flags & (1 << 63)) != 0
            is_keyframe = (pts_flags & (1 << 62)) != 0
            pts = pts_flags & ((1 << 62) - 1)
            
            # Read packet data
            data = b''
            while len(data) < packet_size:
                chunk = sock.recv(min(packet_size - len(data), 8192))
                if not chunk:
                    print("\nConnection closed while reading packet data")
                    return
                data += chunk
            
            packet_count += 1
            total_bytes += packet_size
            
            if is_config:
                config_packet_count += 1
                packet_type = "CONFIG"
            elif is_keyframe:
                key_frame_count += 1
                packet_type = "KEYFRAME"
            else:
                packet_type = "FRAME"
            
            # Decode frame if codec is available
            if codec is not None:
                try:
                    packet = av.Packet(data)
                    frames = codec.decode(packet)
                    
                    for frame in frames:
                        decoded_frame_count += 1
                        # Keep the latest frame for screenshots
                        latest_frame = frame.to_ndarray(format='rgb24')
                        
                        if HAVE_PIL and latest_frame is not None:
                            screenshot_count += 1
                            # Always save as test_screenshot.png (overwrites previous)
                            filename = "test_screenshot.png"
                            img = Image.fromarray(latest_frame)
                            img.save(filename)
                            print(f"\n✓ Screenshot saved: {filename} (capture #{screenshot_count})")
                        else:
                            print("\n✗ Cannot save screenshot (Pillow not installed)")
                except KeyboardInterrupt as k:
                    raise k
                except Exception as e:
                    # Silently ignore decode errors (can happen with partial packets)
                    pass
            
            # Print stats every 30 packets
            if packet_count % 30 == 0:
                avg_size = total_bytes / packet_count if packet_count > 0 else 0
                stats = f"Packets: {packet_count} | Keyframes: {key_frame_count} | "
                stats += f"Config: {config_packet_count} | Avg size: {avg_size:.1f} bytes | "
                stats += f"Total: {total_bytes/1024:.1f} KB"
                if codec is not None:
                    stats += f" | Decoded: {decoded_frame_count}"
                print(stats)
            
    except KeyboardInterrupt:
        print("\n\nStopped by user")
    except Exception as e:
        print(f"\nERROR: {e}")
    finally:
        sock.close()
        
        print("\n" + "="*50)
        print("Session Summary:")
        print(f"  Total packets: {packet_count}")
        if codec is not None:
            print(f"  Decoded frames: {decoded_frame_count}")
        if screenshot_count > 0:
            print(f"  Screenshots captured: {screenshot_count}")
        print("="*50)
        print("Connection closed")

if __name__ == '__main__':
    port = 8080
    if len(sys.argv) > 1:
        try:
            port = int(sys.argv[1])
        except ValueError:
            print(f"Usage: {sys.argv[0]} [port]")
            sys.exit(1)
    
    read_scrcpy_stream('localhost', port)
