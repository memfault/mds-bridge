"""
HID Backend - Implements MDS backend interface using hidapi

This class demonstrates how to create a custom backend for the MDS protocol
using Python and hidapi for HID operations. The backend is registered with
the C library via ctypes callbacks, allowing the C code to call back into
Python for HID I/O operations.
"""

import ctypes
from typing import Optional

from bindings import (
    mds_backend_t,
    mds_backend_ops_t,
    BACKEND_READ_FN,
    BACKEND_WRITE_FN,
    BACKEND_DESTROY_FN,
    MDS_REPORT_ID,
)


class HIDBackend:
    """
    HID Backend - Bridges hidapi with MDS backend interface
    """

    def __init__(self, hid_device):
        """
        Initialize HID backend

        Args:
            hid_device: hidapi device instance
        """
        self.device = hid_device
        self.backend = None
        self.ops = None

        # Create backend callbacks
        self._create_backend()

    def _create_backend(self):
        """Create the backend structure with C callbacks"""

        # Create read callback - the C library will call this to read HID reports
        @BACKEND_READ_FN
        def read_callback(impl_data, report_id, buffer, length, timeout_ms):
            try:
                return self._handle_read(report_id, buffer, length, timeout_ms)
            except Exception as e:
                print(f"Backend read error: {e}")
                return -5  # -EIO

        # Create write callback - the C library will call this to write HID reports
        @BACKEND_WRITE_FN
        def write_callback(impl_data, report_id, buffer, length):
            try:
                return self._handle_write(report_id, buffer, length)
            except Exception as e:
                print(f"Backend write error: {e}")
                return -5  # -EIO

        # Create destroy callback (no-op for Python backend)
        @BACKEND_DESTROY_FN
        def destroy_callback(impl_data):
            pass  # No-op for Python backend

        # Create ops structure
        self.ops = mds_backend_ops_t()
        self.ops.read = read_callback
        self.ops.write = write_callback
        self.ops.destroy = destroy_callback

        # Create backend structure
        self.backend = mds_backend_t()
        self.backend.ops = ctypes.pointer(self.ops)
        self.backend.impl_data = None  # We don't need impl_data since we use 'self'

        # Keep callbacks alive (prevent garbage collection)
        self._keep_alive = [read_callback, write_callback, destroy_callback]

    def _handle_read(self, report_id: int, buffer, length: int, timeout_ms: int) -> int:
        """
        Handle read operation from C library

        Args:
            report_id: HID report ID to read
            buffer: C buffer to write data into
            length: Expected length
            timeout_ms: Timeout in milliseconds

        Returns:
            Number of bytes read or negative error code
        """
        # Determine if this is a feature report (0x01-0x05) or input report (0x06)
        if (report_id >= MDS_REPORT_ID.SUPPORTED_FEATURES and
            report_id <= MDS_REPORT_ID.STREAM_CONTROL):
            # Feature report - use get_feature_report()
            data = self.device.get_feature_report(report_id, length + 1)  # +1 for report ID

            if not data or len(data) < 2:
                return -5  # -EIO

            # Copy data to C buffer (skip report ID byte)
            data_without_report_id = bytes(data[1:])
            bytes_to_copy = min(len(data_without_report_id), length)

            for i in range(bytes_to_copy):
                buffer[i] = data_without_report_id[i]

            return bytes_to_copy

        elif report_id == MDS_REPORT_ID.STREAM_DATA:
            # Input report - this should be handled via the read() in the application
            # For blocking reads, we would need to implement a queue
            # For now, return EAGAIN (would block)
            return -11  # -EAGAIN

        else:
            return -22  # -EINVAL

    def _handle_write(self, report_id: int, buffer, length: int) -> int:
        """
        Handle write operation from C library

        Args:
            report_id: HID report ID to write
            buffer: C buffer containing data to write
            length: Length of data

        Returns:
            Number of bytes written or negative error code
        """
        # All write operations in MDS use SET_FEATURE (Report ID 0x05 - stream control)
        if report_id == MDS_REPORT_ID.STREAM_CONTROL:
            try:
                # Copy data from C buffer to Python bytes
                data = bytes([buffer[i] for i in range(length)])

                # send_feature_report() expects [report_id, ...data]
                report = bytes([report_id]) + data
                bytes_written = self.device.send_feature_report(list(report))

                return length
            except Exception as e:
                return -5  # -EIO
        else:
            return -22  # -EINVAL

    def get_backend_ref(self):
        """Get the backend reference (pointer)"""
        return ctypes.byref(self.backend)

    def destroy(self):
        """Destroy the backend (cleanup)"""
        # Nothing to do for Python backend
        # The HID device is managed by the application
        pass
