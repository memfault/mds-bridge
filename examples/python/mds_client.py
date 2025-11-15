"""
MDS Client - High-level wrapper for Memfault Diagnostic Service

This class provides a Python API for the MDS protocol, using hid library
for HID I/O and the C library (via ctypes) for protocol logic.
"""

import ctypes
from typing import Optional, Callable, Dict, Any
import requests
from dataclasses import dataclass

from bindings import (
    lib,
    MDS_REPORT_ID,
    MDS_STREAM_MODE,
    MDS_MAX_DEVICE_ID_LEN,
    MDS_MAX_URI_LEN,
    MDS_MAX_AUTH_LEN,
    MDS_MAX_CHUNK_DATA_LEN,
    MDS_SEQUENCE_MAX,
    mds_stream_packet_t,
    mds_extract_sequence,
)


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
    MDS Client for managing diagnostic data streaming

    This client uses the HID library for I/O and the C library for
    protocol logic via FFI.
    """

    def __init__(self, hid_device):
        """
        Initialize MDS client

        Args:
            hid_device: hidapi device instance
        """
        self.device = hid_device
        self.session: Optional[ctypes.c_void_p] = None
        self.config: Optional[DeviceConfig] = None
        self.streaming = False
        self.on_chunk: Optional[Callable[[StreamPacket], None]] = None

    def initialize(self) -> None:
        """Initialize the MDS session and read device configuration"""
        # Create MDS session (without HID device handle - we manage that in Python)
        session_ptr = ctypes.c_void_p()
        result = lib.mds_session_create(None, ctypes.byref(session_ptr))

        if result < 0:
            raise RuntimeError(f"Failed to create MDS session: {result}")

        self.session = session_ptr

        # Read device configuration
        self.read_device_config()

    def read_device_config(self) -> DeviceConfig:
        """
        Read device configuration from the device

        Returns:
            DeviceConfig instance
        """
        config = {}

        # Read supported features (Feature Report 0x01)
        features_data = self.device.get_feature_report(
            MDS_REPORT_ID.SUPPORTED_FEATURES, 5  # 1 byte report ID + 4 bytes data
        )
        if features_data and len(features_data) > 1:
            features_buffer = (ctypes.c_uint8 * 4)(*features_data[1:5])
            features = ctypes.c_uint32()
            lib.mds_parse_supported_features(
                features_buffer, 4, ctypes.byref(features)
            )
            config['supported_features'] = features.value
        else:
            config['supported_features'] = 0

        # Read device identifier (Feature Report 0x02)
        device_id_data = self.device.get_feature_report(
            MDS_REPORT_ID.DEVICE_IDENTIFIER, MDS_MAX_DEVICE_ID_LEN + 1
        )
        if device_id_data and len(device_id_data) > 1:
            device_id_buffer = (ctypes.c_uint8 * len(device_id_data[1:]))(*device_id_data[1:])
            device_id_str = ctypes.create_string_buffer(MDS_MAX_DEVICE_ID_LEN)
            lib.mds_parse_device_identifier(
                device_id_buffer, len(device_id_data[1:]),
                device_id_str, MDS_MAX_DEVICE_ID_LEN
            )
            config['device_identifier'] = device_id_str.value.decode('utf-8')
        else:
            config['device_identifier'] = ''

        # Read data URI (Feature Report 0x03)
        uri_data = self.device.get_feature_report(
            MDS_REPORT_ID.DATA_URI, MDS_MAX_URI_LEN + 1
        )
        if uri_data and len(uri_data) > 1:
            uri_buffer = (ctypes.c_uint8 * len(uri_data[1:]))(*uri_data[1:])
            uri_str = ctypes.create_string_buffer(MDS_MAX_URI_LEN)
            lib.mds_parse_data_uri(
                uri_buffer, len(uri_data[1:]),
                uri_str, MDS_MAX_URI_LEN
            )
            config['data_uri'] = uri_str.value.decode('utf-8')
        else:
            config['data_uri'] = ''

        # Read authorization (Feature Report 0x04)
        auth_data = self.device.get_feature_report(
            MDS_REPORT_ID.AUTHORIZATION, MDS_MAX_AUTH_LEN + 1
        )
        if auth_data and len(auth_data) > 1:
            auth_buffer = (ctypes.c_uint8 * len(auth_data[1:]))(*auth_data[1:])
            auth_str = ctypes.create_string_buffer(MDS_MAX_AUTH_LEN)
            lib.mds_parse_authorization(
                auth_buffer, len(auth_data[1:]),
                auth_str, MDS_MAX_AUTH_LEN
            )
            config['authorization'] = auth_str.value.decode('utf-8')
        else:
            config['authorization'] = ''

        self.config = DeviceConfig(**config)
        return self.config

    def enable_streaming(self) -> None:
        """Enable diagnostic data streaming"""
        buffer = (ctypes.c_uint8 * 1)()
        bytes_written = lib.mds_build_stream_control(True, buffer, 1)

        if bytes_written < 0:
            raise RuntimeError(f"Failed to build stream control: {bytes_written}")

        # Send output report (Report ID 0x05)
        report = [MDS_REPORT_ID.STREAM_CONTROL] + list(buffer)
        self.device.write(report)

        self.streaming = True

    def disable_streaming(self) -> None:
        """Disable diagnostic data streaming"""
        buffer = (ctypes.c_uint8 * 1)()
        bytes_written = lib.mds_build_stream_control(False, buffer, 1)

        if bytes_written < 0:
            raise RuntimeError(f"Failed to build stream control: {bytes_written}")

        # Send output report (Report ID 0x05)
        report = [MDS_REPORT_ID.STREAM_CONTROL] + list(buffer)
        self.device.write(report)

        self.streaming = False

    def process_hid_data(self, data: bytes) -> Optional[StreamPacket]:
        """
        Process incoming HID data

        Call this when you receive data from the HID device

        Args:
            data: Raw HID input report data (including report ID)

        Returns:
            StreamPacket if this was MDS stream data, None otherwise
        """
        if not data or len(data) < 1:
            return None

        report_id = data[0]

        # Only process MDS stream data reports
        if report_id != MDS_REPORT_ID.STREAM_DATA:
            return None

        # Parse the stream packet
        packet_data = data[1:]  # Remove report ID
        packet_buffer = (ctypes.c_uint8 * len(packet_data))(*packet_data)
        packet = mds_stream_packet_t()

        result = lib.mds_parse_stream_packet(
            packet_buffer, len(packet_data), ctypes.byref(packet)
        )

        if result < 0:
            print(f"Failed to parse stream packet: {result}")
            return None

        # Validate sequence number
        last_seq = lib.mds_get_last_sequence(self.session)
        if last_seq != MDS_SEQUENCE_MAX:
            is_valid = lib.mds_validate_sequence(last_seq, packet.sequence)
            if not is_valid:
                expected = (last_seq + 1) & 0x1F
                print(f"Warning: Sequence validation failed: expected {expected}, got {packet.sequence}")

        # Update last sequence
        lib.mds_update_last_sequence(self.session, packet.sequence)

        # Extract chunk data
        chunk_data = bytes(packet.data[:packet.data_len])

        stream_packet = StreamPacket(
            sequence=packet.sequence,
            data=chunk_data,
            length=packet.data_len
        )

        # Trigger callback if registered
        if self.on_chunk:
            self.on_chunk(stream_packet)

        return stream_packet

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

        # Parse authorization header (format: "HeaderName:HeaderValue")
        auth_parts = self.config.authorization.split(':', 1)
        headers = {
            'Content-Type': 'application/octet-stream',
        }
        if len(auth_parts) == 2:
            headers[auth_parts[0]] = auth_parts[1]

        try:
            response = requests.post(
                self.config.data_uri,
                headers=headers,
                data=chunk_data,
                timeout=10
            )
            response.raise_for_status()
            return True
        except requests.RequestException as e:
            print(f"Failed to upload chunk: {e}")
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
        if self.session:
            lib.mds_session_destroy(self.session)
            self.session = None

    def __enter__(self):
        """Context manager entry"""
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit"""
        self.destroy()
