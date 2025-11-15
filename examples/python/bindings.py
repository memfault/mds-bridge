"""
FFI bindings for Memfault HID library

This module provides low-level bindings to the memfault_hid C library
using ctypes. It exposes the buffer-based MDS protocol API.
"""

import ctypes
import platform
import os
from pathlib import Path
from typing import Optional

# Get library path based on platform
def get_library_path() -> str:
    """Find the memfault_hid shared library"""
    root_dir = Path(__file__).parent.parent.parent
    build_dir = root_dir / 'build'

    system = platform.system()
    if system == 'Darwin':
        lib_name = 'libmemfault_hid.dylib'
    elif system == 'Linux':
        lib_name = 'libmemfault_hid.so'
    elif system == 'Windows':
        lib_name = 'memfault_hid.dll'
    else:
        raise RuntimeError(f"Unsupported platform: {system}")

    lib_path = build_dir / lib_name
    if not lib_path.exists():
        # Try versioned library
        if system == 'Darwin':
            lib_path = build_dir / 'libmemfault_hid.1.dylib'
        elif system == 'Linux':
            lib_path = build_dir / 'libmemfault_hid.so.1'

    if not lib_path.exists():
        raise FileNotFoundError(
            f"Library not found: {lib_path}\n"
            f"Please build the library first:\n"
            f"  cd {root_dir}\n"
            f"  cmake -B build -DBUILD_SHARED_LIBS=ON\n"
            f"  cmake --build build"
        )

    return str(lib_path)

# MDS Report IDs
class MDS_REPORT_ID:
    """MDS HID Report IDs"""
    SUPPORTED_FEATURES = 0x01
    DEVICE_IDENTIFIER = 0x02
    DATA_URI = 0x03
    AUTHORIZATION = 0x04
    STREAM_CONTROL = 0x05
    STREAM_DATA = 0x06

# MDS Stream modes
class MDS_STREAM_MODE:
    """Stream control modes"""
    DISABLED = 0x00
    ENABLED = 0x01

# Constants
MDS_MAX_DEVICE_ID_LEN = 64
MDS_MAX_URI_LEN = 128
MDS_MAX_AUTH_LEN = 128
MDS_MAX_CHUNK_DATA_LEN = 63
MDS_SEQUENCE_MASK = 0x1F
MDS_SEQUENCE_MAX = 31

# Struct definitions
class mds_device_config_t(ctypes.Structure):
    """MDS device configuration"""
    _fields_ = [
        ('supported_features', ctypes.c_uint32),
        ('device_identifier', ctypes.c_char * MDS_MAX_DEVICE_ID_LEN),
        ('data_uri', ctypes.c_char * MDS_MAX_URI_LEN),
        ('authorization', ctypes.c_char * MDS_MAX_AUTH_LEN),
    ]

class mds_stream_packet_t(ctypes.Structure):
    """MDS stream data packet"""
    _fields_ = [
        ('sequence', ctypes.c_uint8),
        ('data', ctypes.c_uint8 * MDS_MAX_CHUNK_DATA_LEN),
        ('data_len', ctypes.c_size_t),
    ]

# Load the library
_lib_path = get_library_path()
print(f"Loading Memfault HID library from: {_lib_path}")
lib = ctypes.CDLL(_lib_path)

# Function signatures

# Session management
lib.mds_session_create.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p)]
lib.mds_session_create.restype = ctypes.c_int

lib.mds_session_destroy.argtypes = [ctypes.c_void_p]
lib.mds_session_destroy.restype = None

# Buffer-based parsing functions
lib.mds_parse_supported_features.argtypes = [
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_size_t,
    ctypes.POINTER(ctypes.c_uint32)
]
lib.mds_parse_supported_features.restype = ctypes.c_int

lib.mds_parse_device_identifier.argtypes = [
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_size_t,
    ctypes.c_char_p,
    ctypes.c_size_t
]
lib.mds_parse_device_identifier.restype = ctypes.c_int

lib.mds_parse_data_uri.argtypes = [
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_size_t,
    ctypes.c_char_p,
    ctypes.c_size_t
]
lib.mds_parse_data_uri.restype = ctypes.c_int

lib.mds_parse_authorization.argtypes = [
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_size_t,
    ctypes.c_char_p,
    ctypes.c_size_t
]
lib.mds_parse_authorization.restype = ctypes.c_int

lib.mds_build_stream_control.argtypes = [
    ctypes.c_bool,
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_size_t
]
lib.mds_build_stream_control.restype = ctypes.c_int

lib.mds_parse_stream_packet.argtypes = [
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_size_t,
    ctypes.POINTER(mds_stream_packet_t)
]
lib.mds_parse_stream_packet.restype = ctypes.c_int

# Utility functions
lib.mds_validate_sequence.argtypes = [ctypes.c_uint8, ctypes.c_uint8]
lib.mds_validate_sequence.restype = ctypes.c_bool

lib.mds_get_last_sequence.argtypes = [ctypes.c_void_p]
lib.mds_get_last_sequence.restype = ctypes.c_uint8

lib.mds_update_last_sequence.argtypes = [ctypes.c_void_p, ctypes.c_uint8]
lib.mds_update_last_sequence.restype = None

# Helper functions

def mds_extract_sequence(byte0: int) -> int:
    """Extract sequence number from first byte of stream packet"""
    return byte0 & MDS_SEQUENCE_MASK
