/**
 * FFI bindings for Memfault HID library
 *
 * This module provides low-level bindings to the memfault_hid C library
 * using ffi-napi. It exposes the buffer-based MDS protocol API.
 */

import ffi from 'ffi-napi';
import ref from 'ref-napi';
import StructType from 'ref-struct-napi';
import ArrayType from 'ref-array-napi';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';
import os from 'os';

// Get library path based on platform
function getLibraryPath() {
  const rootDir = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
  const buildDir = join(rootDir, 'build');

  const platform = os.platform();
  if (platform === 'darwin') {
    return join(buildDir, 'libmemfault_hid.dylib');
  } else if (platform === 'linux') {
    return join(buildDir, 'libmemfault_hid.so');
  } else if (platform === 'win32') {
    return join(buildDir, 'memfault_hid.dll');
  } else {
    throw new Error(`Unsupported platform: ${platform}`);
  }
}

// Type definitions
const uint8_t = ref.types.uint8;
const uint32_t = ref.types.uint32;
const size_t = ref.types.size_t;
const int = ref.types.int;
const bool = ref.types.bool;
const voidPtr = ref.refType(ref.types.void);

// Array types
const uint8Array = ArrayType(uint8_t);

// MDS constants
export const MDS_REPORT_ID = {
  SUPPORTED_FEATURES: 0x01,
  DEVICE_IDENTIFIER: 0x02,
  DATA_URI: 0x03,
  AUTHORIZATION: 0x04,
  STREAM_CONTROL: 0x05,
  STREAM_DATA: 0x06,
};

export const MDS_STREAM_MODE = {
  DISABLED: 0x00,
  ENABLED: 0x01,
};

export const MDS_MAX_DEVICE_ID_LEN = 64;
export const MDS_MAX_URI_LEN = 128;
export const MDS_MAX_AUTH_LEN = 128;
export const MDS_MAX_CHUNK_DATA_LEN = 63;
export const MDS_SEQUENCE_MASK = 0x1F;
export const MDS_SEQUENCE_MAX = 31;

// Struct definitions
export const mds_device_config_t = StructType({
  supported_features: uint32_t,
  device_identifier: ArrayType(ref.types.char, MDS_MAX_DEVICE_ID_LEN),
  data_uri: ArrayType(ref.types.char, MDS_MAX_URI_LEN),
  authorization: ArrayType(ref.types.char, MDS_MAX_AUTH_LEN),
});

export const mds_stream_packet_t = StructType({
  sequence: uint8_t,
  data: ArrayType(uint8_t, MDS_MAX_CHUNK_DATA_LEN),
  data_len: size_t,
});

// Opaque pointer types
const mds_session_ptr = ref.refType(ref.types.void);
const mds_session_ptr_ptr = ref.refType(mds_session_ptr);

// Load the library
const libraryPath = getLibraryPath();
console.log(`Loading Memfault HID library from: ${libraryPath}`);

export const lib = ffi.Library(libraryPath, {
  // Session management
  'mds_session_create': [int, [voidPtr, mds_session_ptr_ptr]],
  'mds_session_destroy': ['void', [mds_session_ptr]],

  // Buffer-based parsing functions
  'mds_parse_supported_features': [int, [ref.refType(uint8_t), size_t, ref.refType(uint32_t)]],
  'mds_parse_device_identifier': [int, [ref.refType(uint8_t), size_t, 'string', size_t]],
  'mds_parse_data_uri': [int, [ref.refType(uint8_t), size_t, 'string', size_t]],
  'mds_parse_authorization': [int, [ref.refType(uint8_t), size_t, 'string', size_t]],
  'mds_build_stream_control': [int, [bool, ref.refType(uint8_t), size_t]],
  'mds_parse_stream_packet': [int, [ref.refType(uint8_t), size_t, ref.refType(mds_stream_packet_t)]],

  // Utility functions
  'mds_validate_sequence': [bool, [uint8_t, uint8_t]],
  'mds_get_last_sequence': [uint8_t, [mds_session_ptr]],
  'mds_update_last_sequence': ['void', [mds_session_ptr, uint8_t]],
});

// Export ref types for use in other modules
export { ref, voidPtr, mds_session_ptr };
