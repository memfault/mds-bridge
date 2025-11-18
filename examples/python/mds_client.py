"""
MDS Client - Using Custom HID Backend

This demonstrates the pluggable backend architecture by creating
a custom backend that bridges hidapi with the MDS C library. This approach:

1. Creates a HIDBackend that implements the backend interface
2. Registers it with the C library via ctypes callbacks
3. Uses high-level MDS API functions instead of buffer-based parsing
4. Demonstrates transport-agnostic protocol design
"""

import ctypes
from typing import Optional, Callable
import requests
from dataclasses import dataclass

from bindings import (
    lib,
    MDS_REPORT_ID,
    MDS_SEQUENCE_MAX,
    MDS_CHUNK_UPLOAD_CALLBACK,
    mds_device_config_t,
    mds_stream_packet_t,
)
from hid_backend import HIDBackend


@dataclass
class DeviceConfig:
    """MDS device configuration"""
    supported_features: int
    device_identifier: str
    data_uri: str
    authorization: str


class MDSClient:
    """
    MDS Client - Using pluggable backend architecture

    This client demonstrates the benefits of the backend architecture:
    - No manual buffer parsing for configuration
    - No manual HID feature report reads
    - The C library handles all the protocol details via callbacks
    """

    def __init__(self, hid_device):
        """
        Initialize MDS client

        Args:
            hid_device: hidapi device instance
        """
        self.device = hid_device
        self.backend: Optional[HIDBackend] = None
        self.session: Optional[ctypes.c_void_p] = None
        self.config: Optional[DeviceConfig] = None
        self.config_struct: Optional[mds_device_config_t] = None  # C struct for callbacks
        self.streaming = False
        self.upload_callback = None  # Store C callback to prevent garbage collection
        self.stats = {
            'chunks_received': 0,
            'chunks_uploaded': 0,
            'upload_errors': 0,
        }

    def initialize(self) -> None:
        """Initialize the MDS session with custom backend"""
        print("[MDSClient] Creating HID backend...")

        # Create custom backend using hidapi
        self.backend = HIDBackend(self.device)

        # Create MDS session with our custom backend
        session_ptr = ctypes.c_void_p()
        result = lib.mds_session_create(
            self.backend.get_backend_ref(),
            ctypes.byref(session_ptr)
        )

        if result < 0:
            raise RuntimeError(f"Failed to create MDS session: {result}")

        self.session = session_ptr
        print("[MDSClient] MDS session created successfully")

        # Read device configuration using high-level API
        self.read_device_config()

    def read_device_config(self) -> DeviceConfig:
        """
        Read device configuration using high-level C API

        This demonstrates the benefit of the backend architecture:
        - No manual buffer parsing needed
        - No manual HID feature report reads
        - The C library handles all the protocol details

        Returns:
            DeviceConfig instance
        """
        print("[MDSClient] Reading device configuration...")

        # Use high-level API - the C library will call our backend for HID operations
        config = mds_device_config_t()
        result = lib.mds_read_device_config(self.session, ctypes.byref(config))

        if result < 0:
            raise RuntimeError(f"Failed to read device config: {result}")

        # Store both the C struct (for callbacks) and Python dataclass (for convenience)
        self.config_struct = config
        self.config = DeviceConfig(
            supported_features=config.supported_features,
            device_identifier=config.device_identifier.decode('utf-8').rstrip('\x00'),
            data_uri=config.data_uri.decode('utf-8').rstrip('\x00'),
            authorization=config.authorization.decode('utf-8').rstrip('\x00'),
        )

        print("[MDSClient] Device configuration:")
        print(f"  Device ID: {self.config.device_identifier}")
        print(f"  Data URI: {self.config.data_uri}")
        print(f"  Auth: {self.config.authorization}")
        print(f"  Features: 0x{self.config.supported_features:08x}")

        return self.config

    def enable_streaming(self) -> None:
        """Enable diagnostic data streaming using high-level API"""
        print("[MDSClient] Enabling streaming...")

        # Use high-level API - no need to manually build control packets
        result = lib.mds_stream_enable(self.session)

        if result < 0:
            raise RuntimeError(f"Failed to enable streaming: {result}")

        self.streaming = True
        print("[MDSClient] Streaming enabled")

    def disable_streaming(self) -> None:
        """Disable diagnostic data streaming using high-level API"""
        print("[MDSClient] Disabling streaming...")

        result = lib.mds_stream_disable(self.session)

        if result < 0:
            raise RuntimeError(f"Failed to disable streaming: {result}")

        self.streaming = False
        print("[MDSClient] Streaming disabled")

    def enable_upload(self, upload_enabled: bool = True) -> None:
        """
        Enable or disable chunk upload to Memfault cloud.

        When enabled, chunks are automatically uploaded via the C library's
        upload callback mechanism.

        Args:
            upload_enabled: True to enable upload, False to disable
        """
        if upload_enabled:
            print("[MDSClient] Registering upload callback...")

            # Create C callback function
            @MDS_CHUNK_UPLOAD_CALLBACK
            def upload_callback_impl(uri, auth_header, chunk_data, chunk_len, user_data):
                try:
                    self.stats['chunks_received'] += 1

                    # Convert C strings and data to Python
                    uri_str = uri.decode('utf-8')
                    auth_str = auth_header.decode('utf-8')
                    data_bytes = bytes(chunk_data[:chunk_len])

                    # Parse authorization header
                    auth_parts = auth_str.split(':', 1)
                    headers = {
                        'Content-Type': 'application/octet-stream',
                    }
                    if len(auth_parts) == 2:
                        headers[auth_parts[0].strip()] = auth_parts[1].strip()

                    # Upload to Memfault cloud
                    response = requests.post(
                        uri_str,
                        headers=headers,
                        data=data_bytes,
                        timeout=10
                    )
                    response.raise_for_status()

                    self.stats['chunks_uploaded'] += 1
                    print(f"[MDSClient] Chunk uploaded: {chunk_len} bytes "
                          f"({self.stats['chunks_uploaded']}/{self.stats['chunks_received']})")
                    return 0

                except requests.RequestException as e:
                    self.stats['upload_errors'] += 1
                    print(f"[MDSClient] Failed to upload chunk: {e}")
                    return -5  # -EIO
                except Exception as e:
                    self.stats['upload_errors'] += 1
                    print(f"[MDSClient] Unexpected error in upload callback: {e}")
                    return -5  # -EIO

            # Store callback to prevent garbage collection
            self.upload_callback = upload_callback_impl

            # Register with C library
            result = lib.mds_set_upload_callback(self.session, self.upload_callback, None)
            if result < 0:
                raise RuntimeError(f"Failed to register upload callback: {result}")

            print("[MDSClient] Upload callback registered")
        else:
            # Unregister callback
            result = lib.mds_set_upload_callback(self.session, None, None)
            if result < 0:
                raise RuntimeError(f"Failed to unregister upload callback: {result}")

            self.upload_callback = None
            print("[MDSClient] Upload callback unregistered")

    def process(self, data: bytes) -> bool:
        """
        Process transport data packet.

        Call this for every packet received from your transport layer
        (HID report, Serial frame, BLE notification, etc.).

        For multiplexed transports (HID, Serial):
            - Expects data[0] to be channel ID (report ID / framing byte)
            - data[1:] is payload

        For pre-demultiplexed transports (BLE):
            - Use process_stream_data() instead

        Args:
            data: Raw packet data from transport (including channel ID)

        Returns:
            True if this was MDS stream data (handled internally)
            False if this should be handled by application
        """
        if not data or len(data) < 1:
            return False

        channel_id = data[0]

        # Only process MDS stream data channel (0x06)
        if channel_id != 0x06:  # MDS_REPORT_ID_STREAM_DATA
            return False

        # Process the stream data payload
        payload = data[1:]
        self._process_stream_payload(payload)
        return True

    def _process_stream_payload(self, payload: bytes) -> None:
        """
        Internal method to process MDS stream packet payload.

        Uses the C library's mds_process_stream_packet_and_upload() which:
        - Parses the packet
        - Validates sequence number
        - Updates sequence tracking
        - Triggers upload callback if registered

        Args:
            payload: Stream packet payload (sequence + data)
        """
        if not payload:
            return

        # Convert to C buffer
        packet_buffer = (ctypes.c_uint8 * len(payload))(*payload)

        # Process packet and trigger upload callback (if registered)
        # The C library handles everything!
        result = lib.mds_process_stream_from_bytes(
            self.session,
            ctypes.byref(self.config_struct),
            packet_buffer,
            len(payload),
            None  # Don't need packet output
        )

        if result < 0:
            print(f"[MDSClient] Failed to process stream packet: {result}")
            return

    def get_config(self) -> Optional[DeviceConfig]:
        """Get device configuration"""
        return self.config

    def destroy(self) -> None:
        """
        Clean up resources

        Note: mds_session_destroy() will automatically disable streaming
        if it's enabled, so we don't need to call disable_streaming() manually.
        """
        if self.session:
            # This will automatically disable streaming if enabled
            lib.mds_session_destroy(self.session)
            self.session = None

        if self.backend:
            self.backend.destroy()
            self.backend = None

        self.streaming = False
