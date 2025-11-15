#!/usr/bin/env python3
"""
Simple test to verify the Python bindings work correctly
"""

import ctypes
from bindings import (
    lib,
    MDS_REPORT_ID,
    MDS_STREAM_MODE,
    MDS_MAX_DEVICE_ID_LEN,
    mds_stream_packet_t,
    mds_extract_sequence,
)


def test_parse_supported_features():
    """Test parsing supported features"""
    print("Testing mds_parse_supported_features...")

    # Create test data (little-endian 0x12345678)
    test_data = (ctypes.c_uint8 * 4)(0x78, 0x56, 0x34, 0x12)
    features = ctypes.c_uint32()

    result = lib.mds_parse_supported_features(test_data, 4, ctypes.byref(features))

    assert result == 0, f"Expected 0, got {result}"
    assert features.value == 0x12345678, f"Expected 0x12345678, got 0x{features.value:08x}"
    print(f"  ✓ Parsed features: 0x{features.value:08x}")


def test_parse_device_identifier():
    """Test parsing device identifier"""
    print("Testing mds_parse_device_identifier...")

    # Create test data
    test_string = b"TEST-DEVICE-12345"
    test_data = (ctypes.c_uint8 * len(test_string))(*test_string)
    device_id = ctypes.create_string_buffer(MDS_MAX_DEVICE_ID_LEN)

    result = lib.mds_parse_device_identifier(
        test_data, len(test_string), device_id, MDS_MAX_DEVICE_ID_LEN
    )

    assert result == 0, f"Expected 0, got {result}"
    assert device_id.value == test_string, f"Expected {test_string}, got {device_id.value}"
    print(f"  ✓ Parsed device ID: {device_id.value.decode('utf-8')}")


def test_build_stream_control():
    """Test building stream control report"""
    print("Testing mds_build_stream_control...")

    # Test enable
    buffer = (ctypes.c_uint8 * 1)()
    result = lib.mds_build_stream_control(True, buffer, 1)

    assert result == 1, f"Expected 1 byte, got {result}"
    assert buffer[0] == MDS_STREAM_MODE.ENABLED, f"Expected ENABLED, got {buffer[0]}"
    print(f"  ✓ Built enable command: 0x{buffer[0]:02x}")

    # Test disable
    result = lib.mds_build_stream_control(False, buffer, 1)

    assert result == 1, f"Expected 1 byte, got {result}"
    assert buffer[0] == MDS_STREAM_MODE.DISABLED, f"Expected DISABLED, got {buffer[0]}"
    print(f"  ✓ Built disable command: 0x{buffer[0]:02x}")


def test_parse_stream_packet():
    """Test parsing stream packet"""
    print("Testing mds_parse_stream_packet...")

    # Create test packet: sequence 5 + some data
    test_data = bytes([0x05, 0x01, 0x02, 0x03, 0x04, 0x05])
    buffer = (ctypes.c_uint8 * len(test_data))(*test_data)
    packet = mds_stream_packet_t()

    result = lib.mds_parse_stream_packet(buffer, len(test_data), ctypes.byref(packet))

    assert result == 0, f"Expected 0, got {result}"
    assert packet.sequence == 5, f"Expected sequence 5, got {packet.sequence}"
    assert packet.data_len == 5, f"Expected 5 bytes, got {packet.data_len}"
    assert list(packet.data[:5]) == [0x01, 0x02, 0x03, 0x04, 0x05], "Data mismatch"
    print(f"  ✓ Parsed packet: seq={packet.sequence}, len={packet.data_len}")


def test_validate_sequence():
    """Test sequence validation"""
    print("Testing mds_validate_sequence...")

    # Valid sequence
    assert lib.mds_validate_sequence(0, 1) == True, "Expected 0->1 to be valid"
    assert lib.mds_validate_sequence(5, 6) == True, "Expected 5->6 to be valid"
    assert lib.mds_validate_sequence(31, 0) == True, "Expected 31->0 to be valid (wrap)"

    # Invalid sequence
    assert lib.mds_validate_sequence(0, 2) == False, "Expected 0->2 to be invalid"
    assert lib.mds_validate_sequence(5, 5) == False, "Expected 5->5 to be invalid"

    print("  ✓ Sequence validation working correctly")


def test_session_management():
    """Test session creation and destruction"""
    print("Testing session management...")

    session_ptr = ctypes.c_void_p()
    result = lib.mds_session_create(None, ctypes.byref(session_ptr))

    assert result == 0, f"Expected 0, got {result}"
    assert session_ptr.value is not None, "Session pointer should not be NULL"
    print(f"  ✓ Session created: {session_ptr.value}")

    # Test sequence tracking
    last_seq = lib.mds_get_last_sequence(session_ptr)
    print(f"  ✓ Initial sequence: {last_seq}")

    lib.mds_update_last_sequence(session_ptr, 15)
    last_seq = lib.mds_get_last_sequence(session_ptr)
    assert last_seq == 15, f"Expected 15, got {last_seq}"
    print(f"  ✓ Updated sequence: {last_seq}")

    lib.mds_session_destroy(session_ptr)
    print("  ✓ Session destroyed")


def test_extract_sequence():
    """Test sequence extraction helper"""
    print("Testing mds_extract_sequence...")

    assert mds_extract_sequence(0x00) == 0, "Expected 0"
    assert mds_extract_sequence(0x05) == 5, "Expected 5"
    assert mds_extract_sequence(0x1F) == 31, "Expected 31"
    assert mds_extract_sequence(0xE5) == 5, "Expected 5 (masked)"

    print("  ✓ Sequence extraction working correctly")


def main():
    print("=" * 60)
    print("Memfault HID Python Bindings Test")
    print("=" * 60)
    print()

    try:
        test_parse_supported_features()
        test_parse_device_identifier()
        test_build_stream_control()
        test_parse_stream_packet()
        test_validate_sequence()
        test_session_management()
        test_extract_sequence()

        print()
        print("=" * 60)
        print("All tests passed! ✓")
        print("=" * 60)

    except AssertionError as e:
        print(f"\n✗ Test failed: {e}")
        return 1
    except Exception as e:
        print(f"\n✗ Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        return 1

    return 0


if __name__ == '__main__':
    import sys
    sys.exit(main())
