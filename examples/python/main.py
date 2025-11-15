#!/usr/bin/env python3
"""
Memfault HID + MDS Example Application

This example demonstrates using hidapi for both:
1. Custom application-specific HID communication
2. Memfault Diagnostic Service (MDS) streaming in parallel

The MDS protocol logic is handled by the C library via ctypes,
while hidapi manages all actual HID I/O.
"""

import hid
import time
import signal
import sys
from typing import Optional
from dataclasses import dataclass

from mds_client import MDSClient, StreamPacket
from bindings import MDS_REPORT_ID


# Configuration - Update these values for your device
@dataclass
class Config:
    vendor_id: int = 0x1234    # Replace with your device's VID
    product_id: int = 0x5678   # Replace with your device's PID
    upload_chunks: bool = True # Set to True to upload chunks to Memfault cloud


CONFIG = Config()


class CustomHIDApp:
    """
    Custom application logic
    This represents your existing HID application code
    """

    def __init__(self, device):
        self.device = device

    def send_custom_command(self, report_id: int, data: list) -> None:
        """
        Example: Send a custom command to the device
        This could be any application-specific HID communication
        """
        print(f"[CustomApp] Sending custom command (Report ID: 0x{report_id:02x})")
        report = [report_id] + data
        self.device.write(report)

    def process_custom_report(self, data: bytes) -> bool:
        """
        Example: Process custom HID reports
        Returns True if the report was handled, False otherwise
        """
        if not data or len(data) < 1:
            return False

        report_id = data[0]

        # Ignore MDS reports - let MDSClient handle those
        if report_id == MDS_REPORT_ID.STREAM_DATA:
            return False

        # Handle your custom reports here
        # For this example, we'll just log them
        data_preview = ' '.join(f'{b:02x}' for b in data[1:min(8, len(data))])
        print(f"[CustomApp] Received report 0x{report_id:02x}: {data_preview}...")
        return True

    def do_periodic_task(self) -> None:
        """Example: Periodic application task"""
        # Your application logic here
        # For example: reading sensors, updating UI, etc.
        print('[CustomApp] Performing periodic task...')


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
        """Initialize MDS and custom app"""
        # Create custom app instance
        self.custom_app = CustomHIDApp(self.device)

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
        Routes data to either custom app or MDS client
        """
        # Try custom app first
        handled = self.custom_app.process_custom_report(data)
        if handled:
            self.stats['custom_reports'] += 1
            return

        # Let MDS client handle it
        self.mds_client.process_hid_data(data)

    def start(self) -> None:
        """Start the application"""
        print("Starting application...\n")

        # Enable MDS streaming
        self.mds_client.enable_streaming()
        print("[MDS] Streaming enabled\n")

        self.running = True

        print("Application running. Press Ctrl+C to exit.\n")

        # Main loop
        last_periodic_task = time.time()
        last_stats_report = time.time()
        periodic_task_interval = 10.0  # seconds
        stats_report_interval = 30.0   # seconds

        try:
            while self.running:
                # Read from HID device (non-blocking)
                data = self.device.read(64, timeout_ms=100)
                if data:
                    self.handle_hid_data(bytes(data))

                current_time = time.time()

                # Periodic custom app task
                if current_time - last_periodic_task >= periodic_task_interval:
                    self.custom_app.do_periodic_task()
                    last_periodic_task = current_time

                    # Example: Send a custom command periodically
                    # Uncomment this if you want to test custom HID commands
                    # self.custom_app.send_custom_command(0x10, [0x01, 0x02, 0x03])

                # Stats reporting
                if current_time - last_stats_report >= stats_report_interval:
                    print(f"\n[Stats] Chunks: {self.stats['chunks_received']} received, "
                          f"{self.stats['chunks_uploaded']} uploaded, "
                          f"Custom reports: {self.stats['custom_reports']}\n")
                    last_stats_report = current_time

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
        print(f"  Custom reports: {self.stats['custom_reports']}")


def main():
    """Main entry point"""
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
