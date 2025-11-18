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


@dataclass
class StreamPacket:
    """Parsed stream packet"""
    sequence: int
    data: bytes
    length: int


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
        self.streaming = False
        self.on_chunk: Optional[Callable[[StreamPacket], None]] = None

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

        # Extract configuration from struct
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

    def process_stream_data(self, payload: bytes) -> None:
        """
        Process MDS stream data payload directly.

        Use this for transports that pre-demultiplex channels (e.g., BLE GATT
        characteristics). When you receive data from the MDS stream characteristic,
        call this method directly.

        Args:
            payload: MDS stream packet payload (without channel ID prefix)
        """
        self._process_stream_payload(payload)

    def _process_stream_payload(self, payload: bytes) -> None:
        """
        Internal method to process MDS stream packet payload.

        Args:
            payload: Stream packet payload (sequence + data)
        """
        if not payload:
            return

        # Parse the stream packet using buffer-based API
        # (We can't use mds_stream_read_packet here because it would block)
        packet_buffer = (ctypes.c_uint8 * len(payload))(*payload)
        packet = mds_stream_packet_t()

        result = lib.mds_parse_stream_packet(
            packet_buffer, len(payload), ctypes.byref(packet)
        )

        if result < 0:
            print(f"[MDSClient] Failed to parse stream packet: {result}")
            return

        # Validate sequence using C library
        last_seq = lib.mds_get_last_sequence(self.session)
        if last_seq != MDS_SEQUENCE_MAX:
            is_valid = lib.mds_validate_sequence(last_seq, packet.sequence)
            if not is_valid:
                expected = (last_seq + 1) & 0x1F
                print(f"[MDSClient] Sequence error: expected {expected}, got {packet.sequence}")

        # Update last sequence
        lib.mds_update_last_sequence(self.session, packet.sequence)

        # Extract chunk data
        chunk_data = bytes(packet.data[:packet.data_len])

        print(f"[MDSClient] Received chunk: seq={packet.sequence}, len={packet.data_len}")

        stream_packet = StreamPacket(
            sequence=packet.sequence,
            data=chunk_data,
            length=packet.data_len
        )

        # Trigger callback if registered
        if self.on_chunk:
            self.on_chunk(stream_packet)

    def upload_chunk(self, chunk_data: bytes) -> bool:
        """
        Upload chunk data to Memfault cloud

        Args:
            chunk_data: Chunk data to upload

        Returns:
            True if upload succeeded, False otherwise
        """
        if not self.config:
            raise RuntimeError("Device config not loaded")

        # Parse authorization header (format: "HeaderName:HeaderValue" or "HeaderName: HeaderValue")
        auth_parts = self.config.authorization.split(':', 1)
        headers = {
            'Content-Type': 'application/octet-stream',
        }
        if len(auth_parts) == 2:
            headers[auth_parts[0].strip()] = auth_parts[1].strip()

        try:
            response = requests.post(
                self.config.data_uri,
                headers=headers,
                data=chunk_data,
                timeout=10
            )
            response.raise_for_status()
            print(f"[MDSClient] Chunk uploaded: {len(chunk_data)} bytes")
            return True
        except requests.RequestException as e:
            print(f"[MDSClient] Failed to upload chunk: {e}")
            return False

    def set_chunk_callback(self, callback: Callable[[StreamPacket], None]) -> None:
        """
        Set callback for received chunks

        Args:
            callback: Function to call when chunk is received.
                     Takes StreamPacket as argument.
        """
        self.on_chunk = callback

    def get_config(self) -> Optional[DeviceConfig]:
        """Get device configuration"""
        return self.config

    def is_streaming(self) -> bool:
        """Check if streaming is enabled"""
        return self.streaming

    def destroy(self) -> None:
        """Clean up resources"""
        if self.streaming:
            try:
                self.disable_streaming()
            except Exception as e:
                print(f"Error disabling streaming: {e}")

        if self.session:
            lib.mds_session_destroy(self.session)
            self.session = None

        if self.backend:
            self.backend.destroy()
            self.backend = None

    def __enter__(self):
        """Context manager entry"""
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit"""
        self.destroy()
