#!/usr/bin/env python3
"""
Memfault HID + MDS Example Application

This example demonstrates using hidapi for both:
1. Custom application-specific HID communication
2. Memfault Diagnostic Service (MDS) streaming in parallel

The MDS protocol logic is handled by the C library via ctypes,
while hidapi manages all actual HID I/O.

Usage:
  ./main.py                    # Use default VID/PID from config
  ./main.py <vid> <pid>        # Specify VID/PID in hex
  ./main.py 0x1234 0x5678
  ./main.py --no-upload        # Disable cloud uploads
"""

import hid
import time
import signal
import sys
import argparse
from typing import Optional
from dataclasses import dataclass

from mds_client import MDSClient, StreamPacket


# Configuration - Default values, can be overridden via CLI
@dataclass
class Config:
    vendor_id: int = 0x1234    # Replace with your device's VID
    product_id: int = 0x5678   # Replace with your device's PID
    upload_chunks: bool = True # Set to True to upload chunks to Memfault cloud


CONFIG = Config()


def parse_hex(value: str) -> int:
    """Parse hex value with or without 0x prefix"""
    if value.lower().startswith('0x'):
        return int(value, 16)
    return int(value, 16)


def parse_args():
    """Parse command line arguments"""
    parser = argparse.ArgumentParser(
        description='Memfault HID + MDS Example Application',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                    # Use default VID/PID
  %(prog)s 1234 5678          # VID=0x1234, PID=0x5678
  %(prog)s 0x1234 0x5678      # Same with 0x prefix
  %(prog)s 1234 5678 --no-upload  # Disable cloud uploads
"""
    )

    parser.add_argument(
        'vid',
        nargs='?',
        type=parse_hex,
        help='USB Vendor ID (hex, with or without 0x prefix)'
    )
    parser.add_argument(
        'pid',
        nargs='?',
        type=parse_hex,
        help='USB Product ID (hex, with or without 0x prefix)'
    )
    parser.add_argument(
        '--no-upload',
        action='store_true',
        help='Disable chunk uploads to Memfault cloud'
    )

    args = parser.parse_args()

    # Update config from CLI args
    if args.vid is not None:
        CONFIG.vendor_id = args.vid
    if args.pid is not None:
        CONFIG.product_id = args.pid
    if args.no_upload:
        CONFIG.upload_chunks = False

    # Validate that both VID and PID are provided if either is
    if (args.vid is None) != (args.pid is None):
        parser.error("Both VID and PID must be specified together")

    return args


class Application:
    """Main application - coordinates custom app and MDS"""

    def __init__(self):
        self.device: Optional[hid.device] = None
        self.mds_client: Optional[MDSClient] = None
        self.custom_app: Optional[CustomHIDApp] = None
        self.running = False
        self.stats = {
            'chunks_received': 0,
            'chunks_uploaded': 0,
            'custom_reports': 0,
        }

    def open_device(self) -> None:
        """Find and open the HID device"""
        print(f"Looking for HID device (VID: 0x{CONFIG.vendor_id:04x}, "
              f"PID: 0x{CONFIG.product_id:04x})...")

        # List all devices
        devices = hid.enumerate()

        # Find our device
        device_info = None
        for dev in devices:
            if dev['vendor_id'] == CONFIG.vendor_id and dev['product_id'] == CONFIG.product_id:
                device_info = dev
                break

        if not device_info:
            print("\nAvailable devices:")
            for dev in devices:
                print(f"  - VID: 0x{dev['vendor_id']:04x}, "
                      f"PID: 0x{dev['product_id']:04x}, "
                      f"{dev.get('manufacturer_string', 'Unknown')} "
                      f"{dev.get('product_string', 'Unknown')}")
            raise RuntimeError("Device not found. Update CONFIG with your device VID/PID.")

        print(f"Found device: {device_info.get('manufacturer_string', 'Unknown')} "
              f"{device_info.get('product_string', 'Unknown')}")
        print(f"  Path: {device_info['path']}")

        self.device = hid.device()
        self.device.open_path(device_info['path'])
        self.device.set_nonblocking(True)  # Non-blocking reads
        print("Device opened successfully\n")

    def initialize(self) -> None:
        # Create and initialize MDS client
        self.mds_client = MDSClient(self.device)
        self.mds_client.initialize()

        config = self.mds_client.get_config()
        print("MDS Device Configuration:")
        print(f"  Device ID: {config.device_identifier}")
        print(f"  Data URI: {config.data_uri}")
        print(f"  Auth: {config.authorization[:30]}...")
        print(f"  Features: 0x{config.supported_features:08x}\n")

        # Set up chunk callback
        def on_chunk(packet: StreamPacket):
            self.stats['chunks_received'] += 1
            print(f"[MDS] Chunk received: seq={packet.sequence}, len={packet.length} bytes")

            # Upload chunk if enabled
            if CONFIG.upload_chunks:
                success = self.mds_client.upload_chunk(packet.data)
                if success:
                    self.stats['chunks_uploaded'] += 1
                    print("[MDS] Chunk uploaded successfully")

        self.mds_client.set_chunk_callback(on_chunk)

    def handle_hid_data(self, data: bytes) -> None:
        """
        Handle incoming HID data
        Routes data to either MDS client or elsewhere
        """
        # Let MDS process first - returns True if it handled the data
        if self.mds_client.process(data):
            return  # MDS handled it

        # Non MDS HID report, handle with your own code here.
        pass

    def start(self) -> None:
        """Start the application"""
        print("Starting application...\n")

        # Enable MDS streaming
        self.mds_client.enable_streaming()
        print("[MDS] Streaming enabled\n")

        self.running = True

        print("Application running. Press Ctrl+C to exit.\n")

        # Main loop
        try:
            while self.running:
                # Read from HID device (non-blocking)
                data = self.device.read(64, timeout_ms=100)
                if data:
                    self.handle_hid_data(bytes(data))

                # Small sleep to prevent busy-waiting
                time.sleep(0.01)

        except KeyboardInterrupt:
            print("\nReceived interrupt signal...")

    def stop(self) -> None:
        """Stop the application"""
        print("\nStopping application...")
        self.running = False

        if self.mds_client:
            try:
                self.mds_client.disable_streaming()
            except Exception as e:
                print(f"Error disabling streaming: {e}")
            self.mds_client.destroy()

        if self.device:
            self.device.close()

        print("Application stopped.")
        print(f"\nFinal stats:")
        print(f"  Chunks received: {self.stats['chunks_received']}")
        print(f"  Chunks uploaded: {self.stats['chunks_uploaded']}")


def main():
    """Main entry point"""
    # Parse command line arguments
    parse_args()

    print("=" * 60)
    print("Memfault HID + MDS Example Application")
    print("=" * 60)
    print()

    app = Application()

    # Handle graceful shutdown
    def signal_handler(sig, frame):
        app.stop()
        sys.exit(0)

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    try:
        app.open_device()
        app.initialize()
        app.start()
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
        app.stop()
        sys.exit(1)
    finally:
        app.stop()


if __name__ == '__main__':
    main()
