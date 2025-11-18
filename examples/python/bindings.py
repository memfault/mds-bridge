"""
FFI bindings for MDS Bridge library

This module provides ctypes bindings to the mds_bridge C library.

It includes the public MDS API from mds_protocol.h, which provides:
- High-level session management (mds_session_create, mds_read_device_config, etc.)
- Stream processing for both blocking I/O (mds_process_stream) and event-driven I/O (mds_process_stream_from_bytes)
"""

import ctypes
import platform
import os
from pathlib import Path
from typing import Optional

# Get library path based on platform
def get_library_path() -> str:
    """Find the mds_bridge shared library"""
    current_dir = Path(__file__).parent

    system = platform.system()
    if system == 'Darwin':
        lib_name = 'libmds_bridge.dylib'
        lib_name_versioned = 'libmds_bridge.2.dylib'
    elif system == 'Linux':
        lib_name = 'libmds_bridge.so'
        lib_name_versioned = 'libmds_bridge.so.2'
    elif system == 'Windows':
        lib_name = 'mds_bridge.dll'
        lib_name_versioned = None
    else:
        raise RuntimeError(f"Unsupported platform: {system}")

    # Strategy 1: Check if we're in build/examples/python (running from build folder)
    # Library would be in build/ (two levels up)
    build_dir = current_dir.parent.parent
    lib_path = build_dir / lib_name
    if lib_path.exists():
        print(f"Loading MDS Bridge library from: {lib_path}")
        return str(lib_path)

    # Try versioned library
    if lib_name_versioned:
        lib_path = build_dir / lib_name_versioned
        if lib_path.exists():
            print(f"Loading MDS Bridge library from: {lib_path}")
            return str(lib_path)

    # Strategy 2: Check if we're in examples/python (source folder)
    # Library would be in ../../build
    root_dir = current_dir.parent.parent
    build_dir = root_dir / 'build'
    lib_path = build_dir / lib_name
    if lib_path.exists():
        print(f"Loading MDS Bridge library from: {lib_path}")
        return str(lib_path)

    if lib_name_versioned:
        lib_path = build_dir / lib_name_versioned
        if lib_path.exists():
            print(f"Loading MDS Bridge library from: {lib_path}")
            return str(lib_path)

    # Not found - provide helpful error
    raise FileNotFoundError(
        f"Library not found in:\n"
        f"  {current_dir.parent.parent / lib_name}\n"
        f"  {root_dir / 'build' / lib_name}\n"
        f"Please build the library first:\n"
        f"  cd {root_dir}\n"
        f"  cmake -B build -DBUILD_SHARED_LIBS=ON\n"
        f"  cmake --build build"
    )

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

# Backend callback function types
BACKEND_READ_FN = ctypes.CFUNCTYPE(
    ctypes.c_int,  # return type
    ctypes.c_void_p,  # impl_data
    ctypes.c_uint8,  # report_id
    ctypes.POINTER(ctypes.c_uint8),  # buffer
    ctypes.c_size_t,  # length
    ctypes.c_int  # timeout_ms
)

BACKEND_WRITE_FN = ctypes.CFUNCTYPE(
    ctypes.c_int,  # return type
    ctypes.c_void_p,  # impl_data
    ctypes.c_uint8,  # report_id
    ctypes.POINTER(ctypes.c_uint8),  # buffer
    ctypes.c_size_t  # length
)

BACKEND_DESTROY_FN = ctypes.CFUNCTYPE(
    None,  # return type
    ctypes.c_void_p  # impl_data
)

class mds_backend_ops_t(ctypes.Structure):
    """Backend operations vtable"""
    _fields_ = [
        ('read', BACKEND_READ_FN),
        ('write', BACKEND_WRITE_FN),
        ('destroy', BACKEND_DESTROY_FN),
    ]

class mds_backend_t(ctypes.Structure):
    """Backend structure"""
    _fields_ = [
        ('ops', ctypes.POINTER(mds_backend_ops_t)),
        ('impl_data', ctypes.c_void_p),
    ]

# Load the library
_lib_path = get_library_path()
print(f"Loading MDS Bridge library from: {_lib_path}")
lib = ctypes.CDLL(_lib_path)

# Callback typedefs
MDS_CHUNK_UPLOAD_CALLBACK = ctypes.CFUNCTYPE(
    ctypes.c_int,  # return type
    ctypes.c_char_p,  # uri
    ctypes.c_char_p,  # auth_header
    ctypes.POINTER(ctypes.c_uint8),  # chunk_data
    ctypes.c_size_t,  # chunk_len
    ctypes.c_void_p  # user_data
)

# Function signatures

# Session management - HIGH-LEVEL API
lib.mds_session_create.argtypes = [ctypes.POINTER(mds_backend_t), ctypes.POINTER(ctypes.c_void_p)]
lib.mds_session_create.restype = ctypes.c_int

lib.mds_session_create_hid.argtypes = [
    ctypes.c_uint16,  # vendor_id
    ctypes.c_uint16,  # product_id
    ctypes.c_void_p,  # serial_number (wchar_t*)
    ctypes.POINTER(ctypes.c_void_p)  # session
]
lib.mds_session_create_hid.restype = ctypes.c_int

lib.mds_session_destroy.argtypes = [ctypes.c_void_p]
lib.mds_session_destroy.restype = None

# Device configuration - HIGH-LEVEL API
lib.mds_read_device_config.argtypes = [
    ctypes.c_void_p,  # session
    ctypes.POINTER(mds_device_config_t)  # config
]
lib.mds_read_device_config.restype = ctypes.c_int

# Stream control - HIGH-LEVEL API
lib.mds_stream_enable.argtypes = [ctypes.c_void_p]  # session
lib.mds_stream_enable.restype = ctypes.c_int

lib.mds_stream_disable.argtypes = [ctypes.c_void_p]  # session
lib.mds_stream_disable.restype = ctypes.c_int

# Upload callback registration
lib.mds_set_upload_callback.argtypes = [
    ctypes.c_void_p,  # session
    MDS_CHUNK_UPLOAD_CALLBACK,  # callback
    ctypes.c_void_p  # user_data
]
lib.mds_set_upload_callback.restype = ctypes.c_int

lib.mds_stream_read_packet.argtypes = [
    ctypes.c_void_p,  # session
    ctypes.POINTER(mds_stream_packet_t),  # packet
    ctypes.c_int  # timeout_ms
]
lib.mds_stream_read_packet.restype = ctypes.c_int

# High-level stream processing - blocking I/O (reads from device)
lib.mds_process_stream.argtypes = [
    ctypes.c_void_p,  # session
    ctypes.POINTER(mds_device_config_t),  # config
    ctypes.c_int,  # timeout_ms
    ctypes.POINTER(mds_stream_packet_t)  # packet (can be NULL)
]
lib.mds_process_stream.restype = ctypes.c_int

# High-level stream processing - event-driven I/O (processes byte buffer)
lib.mds_process_stream_from_bytes.argtypes = [
    ctypes.c_void_p,  # session
    ctypes.POINTER(mds_device_config_t),  # config
    ctypes.POINTER(ctypes.c_uint8),  # buffer
    ctypes.c_size_t,  # buffer_len
    ctypes.POINTER(mds_stream_packet_t)  # packet (can be NULL)
]
lib.mds_process_stream_from_bytes.restype = ctypes.c_int

