# Memfault HID Library

⚠️⚠️⚠️⚠️ VERY WIP ⚠️⚠️⚠️⚠️

A cross-platform C library for communicating with HID devices using custom report types. This library is designed to be integrated into desktop applications that may use other HID reports for additional device functionality.

## Features

- **Cross-platform support**: Windows, macOS, and Linux
- **Built on HIDAPI**: Reliable cross-platform HID communication
- **Memfault Diagnostic Service (MDS)**: Built-in protocol for bridging diagnostic data over HID
- **Report filtering**: Configure which Report IDs the library handles
- **Simple API**: Easy-to-use interface for HID communication
- **Integration-friendly**: Designed to coexist with other HID functionality in applications
- **Pure C implementation**: Maximum compatibility and portability

## Architecture

The library provides:

- Device enumeration and discovery
- Opening/closing HID devices
- Reading and writing Input/Output reports
- Getting and setting Feature reports
- Report ID filtering for selective report handling
- Non-blocking I/O support
- Comprehensive error handling

## Building

### Prerequisites

#### HIDAPI

**macOS:**
```bash
brew install hidapi
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get install libhidapi-dev
```

**Windows:**
- Download HIDAPI from https://github.com/libusb/hidapi
- Or use vcpkg: `vcpkg install hidapi`

#### CMake

```bash
# macOS
brew install cmake

# Linux
sudo apt-get install cmake

# Windows
# Download from https://cmake.org/download/
```

### Build Instructions

```bash
mkdir build
cd build
cmake ..
make

# Optional: Run tests
make test

# Optional: Install
sudo make install
```

### Build Options

- `BUILD_SHARED_LIBS`: Build shared libraries (default: ON)
- `BUILD_EXAMPLES`: Build example programs (default: ON)
- `BUILD_TESTS`: Build test programs (default: ON)

Example:
```bash
cmake -DBUILD_SHARED_LIBS=OFF -DBUILD_EXAMPLES=ON ..
```

## Usage

### Basic Example

```c
#include "memfault_hid/memfault_hid.h"
#include <stdio.h>

int main(void) {
    // Initialize the library
    if (memfault_hid_init() != MEMFAULT_HID_SUCCESS) {
        fprintf(stderr, "Failed to initialize HID library\n");
        return 1;
    }

    // Open a device by VID/PID
    memfault_hid_device_t *device = NULL;
    int ret = memfault_hid_open(0x1234, 0x5678, NULL, &device);
    if (ret != MEMFAULT_HID_SUCCESS) {
        fprintf(stderr, "Failed to open device: %s\n",
                memfault_hid_error_string(ret));
        memfault_hid_exit();
        return 1;
    }

    // Send a report
    uint8_t data[32] = {0};
    data[0] = 0x42; // Example data
    ret = memfault_hid_write_report(device, 0x01, data, sizeof(data), 1000);
    if (ret < 0) {
        fprintf(stderr, "Write failed: %s\n", memfault_hid_error_string(ret));
    }

    // Read a report
    uint8_t recv_data[64];
    uint8_t report_id;
    ret = memfault_hid_read_report(device, &report_id, recv_data,
                                    sizeof(recv_data), 1000);
    if (ret > 0) {
        printf("Received %d bytes from report 0x%02X\n", ret, report_id);
    }

    // Cleanup
    memfault_hid_close(device);
    memfault_hid_exit();

    return 0;
}
```

### Memfault Diagnostic Service (MDS)

The library includes built-in support for the Memfault Diagnostic Service protocol, which enables bridging diagnostic data from embedded devices over HID. This protocol is adapted from the Memfault BLE GATT service specification.

```c
#include "memfault_hid/memfault_hid.h"
#include "memfault_hid/mds_protocol.h"

int main(void) {
    memfault_hid_init();

    // Open device
    memfault_hid_device_t *device = NULL;
    memfault_hid_open(0x1234, 0x5678, NULL, &device);

    // Create MDS session
    mds_session_t *session = NULL;
    mds_session_create(device, &session);

    // Read device configuration
    mds_device_config_t config;
    mds_read_device_config(session, &config);
    printf("Device ID: %s\n", config.device_identifier);
    printf("Data URI: %s\n", config.data_uri);
    printf("Authorization: %s\n", config.authorization);

    // Enable streaming
    mds_stream_enable(session);

    // Read diagnostic chunk packets
    mds_stream_packet_t packet;
    while (mds_stream_read_packet(session, &packet, 5000) == 0) {
        printf("Received packet (seq=%d, len=%zu)\n",
               packet.sequence, packet.data_len);

        // Forward packet.data to Memfault cloud
        // upload_chunk(config.data_uri, config.authorization,
        //              packet.data, packet.data_len);

        // Validate sequence to detect dropped packets
        if (prev_seq_valid) {
            if (!mds_validate_sequence(prev_seq, packet.sequence)) {
                printf("Warning: Dropped or duplicate packet detected\n");
            }
        }
        prev_seq = packet.sequence;
        prev_seq_valid = true;
    }

    // Disable streaming
    mds_stream_disable(session);

    // Cleanup
    mds_session_destroy(session);
    memfault_hid_close(device);
    memfault_hid_exit();

    return 0;
}
```

#### MDS Protocol Overview

The MDS protocol uses HID reports to communicate diagnostic data:

- **Feature Reports** (Read-only):
  - `0x01`: Supported features bitmask
  - `0x02`: Device identifier string
  - `0x03`: Data URI for chunk upload
  - `0x04`: Authorization header (e.g., project key)

- **Output Reports** (Host → Device):
  - `0x05`: Stream control (enable/disable streaming)

- **Input Reports** (Device → Host):
  - `0x06`: Stream data packets with diagnostic chunks

Each stream packet includes:
- **Sequence counter** (5-bit, 0-31, wraps around) for detecting dropped packets
- **Chunk data payload** (up to 63 bytes per packet)

#### MDS API Functions

**Session Management:**
- `mds_session_create()` - Create MDS session over HID device
- `mds_session_destroy()` - Destroy MDS session

**Device Configuration:**
- `mds_read_device_config()` - Read all configuration
- `mds_get_supported_features()` - Get feature bitmask
- `mds_get_device_identifier()` - Get device ID string
- `mds_get_data_uri()` - Get upload URI
- `mds_get_authorization()` - Get auth header

**Stream Control:**
- `mds_stream_enable()` - Enable diagnostic data streaming
- `mds_stream_disable()` - Disable streaming

**Data Reception:**
- `mds_stream_read_packet()` - Read stream data packet (blocking)
- `mds_stream_process()` - Read and automatically upload packets
- `mds_validate_sequence()` - Validate sequence number

**Chunk Upload:**
- `mds_set_upload_callback()` - Register callback for chunk uploads
- `mds_stream_process()` - Convenience function combining read + upload

#### Uploading Chunks to Memfault Cloud

The library includes a built-in HTTP uploader using libcurl, and also supports custom upload callbacks for maximum flexibility.

**Option 1: Custom Upload Callback**

Implement your own upload function using any HTTP library:

```c
int my_upload_callback(const char *uri, const char *auth_header,
                       const uint8_t *chunk_data, size_t chunk_len,
                       void *user_data) {
    // Parse authorization header (format: "HeaderName:HeaderValue")
    // POST chunk_data to uri with appropriate headers
    // Content-Type: application/octet-stream
    return 0; /* Return 0 on success, negative on error */
}

// Register your callback
mds_set_upload_callback(session, my_upload_callback, my_context);

// Process streams with automatic upload
while (running) {
    int ret = mds_stream_process(session, &config, 5000);
    if (ret == 0) {
        printf("Chunk uploaded!\n");
    }
}
```

**Option 2: Built-in HTTP Uploader**

Use the built-in libcurl-based uploader:

```c
#include "memfault_hid/mds_upload.h"

// Create uploader
mds_uploader_t *uploader = mds_uploader_create();
mds_uploader_set_verbose(uploader, true);  /* Optional: enable debug output */

// Register built-in uploader
mds_set_upload_callback(session, mds_uploader_callback, uploader);

// Process streams - chunks are automatically uploaded via HTTP
while (running) {
    int ret = mds_stream_process(session, &config, 5000);
    if (ret == 0) {
        // Get stats
        mds_upload_stats_t stats;
        mds_uploader_get_stats(uploader, &stats);
        printf("Uploaded: %zu chunks, %zu bytes\n",
               stats.chunks_uploaded, stats.bytes_uploaded);
    }
}

// Cleanup
mds_uploader_destroy(uploader);
```

### Report Filtering

The library supports report filtering, allowing it to handle only specific Report IDs while other parts of your application handle different Report IDs:

```c
// Configure the library to only handle Report IDs 0x01-0x0F
uint8_t my_report_ids[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                            0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};

memfault_hid_report_filter_t filter = {
    .report_ids = my_report_ids,
    .num_report_ids = sizeof(my_report_ids),
    .filter_enabled = true
};

memfault_hid_set_report_filter(device, &filter);

// Now the library will only process reports with IDs 0x01-0x0F
// Other Report IDs will return MEMFAULT_HID_ERROR_INVALID_REPORT_TYPE
```

### Device Enumeration

```c
memfault_hid_device_info_t *devices = NULL;
size_t num_devices = 0;

// Enumerate all devices with VID=0x1234, PID=0x5678
int ret = memfault_hid_enumerate(0x1234, 0x5678, &devices, &num_devices);
if (ret == MEMFAULT_HID_SUCCESS) {
    for (size_t i = 0; i < num_devices; i++) {
        printf("Device: %ls (%04X:%04X)\n",
               devices[i].product,
               devices[i].vendor_id,
               devices[i].product_id);
        printf("  Path: %s\n", devices[i].path);
    }

    memfault_hid_free_device_list(devices);
}
```

## Examples

The library includes several example programs:

- `enumerate_devices`: List all HID devices
- `send_receive`: Send and receive reports
- `continuous_comm`: Continuous communication loop
- `mds_gateway`: Full MDS gateway with chunk forwarding to Memfault cloud

To run the examples after building:

```bash
# List all HID devices
./build/examples/enumerate_devices

# List devices with specific VID/PID
./build/examples/enumerate_devices 1234 5678

# Send and receive with a device
./build/examples/send_receive 1234 5678

# Continuous communication
./build/examples/continuous_comm 1234 5678

# MDS gateway (with built-in HTTP uploader)
./build/examples/mds_gateway 1234 5678

# MDS gateway (with custom upload callback)
./build/examples/mds_gateway 1234 5678 --custom-upload
```

## API Reference

### Initialization

- `memfault_hid_init()` - Initialize the library
- `memfault_hid_exit()` - Clean up and shutdown

### Device Management

- `memfault_hid_enumerate()` - Enumerate devices by VID/PID
- `memfault_hid_free_device_list()` - Free enumeration results
- `memfault_hid_open()` - Open device by VID/PID
- `memfault_hid_open_path()` - Open device by path
- `memfault_hid_close()` - Close device
- `memfault_hid_get_device_info()` - Get device information

### Report Filtering

- `memfault_hid_set_report_filter()` - Configure report filtering
- `memfault_hid_get_report_filter()` - Get filter configuration

### Communication

- `memfault_hid_write_report()` - Write an output report
- `memfault_hid_read_report()` - Read an input report
- `memfault_hid_get_feature_report()` - Get a feature report
- `memfault_hid_set_feature_report()` - Set a feature report

### Utilities

- `memfault_hid_error_string()` - Get error description
- `memfault_hid_version_string()` - Get library version
- `memfault_hid_set_nonblocking()` - Set non-blocking mode

## Integration into Applications

This library is designed to be integrated into larger applications that may have their own HID communication needs. Key integration points:

1. **Report ID Filtering**: Use `memfault_hid_set_report_filter()` to specify which Report IDs this library should handle
2. **Multiple Devices**: The library supports opening multiple devices simultaneously
3. **Non-blocking I/O**: Use `memfault_hid_set_nonblocking()` for non-blocking operation
4. **Error Handling**: All functions return error codes that can be converted to strings with `memfault_hid_error_string()`

## Platform Notes

### Windows

- Requires Windows 7 or later
- No driver installation required for most HID devices
- May require administrator privileges for some devices

### macOS

- Requires macOS 10.9 or later
- Uses IOKit framework (linked automatically by CMake)
- No special permissions required for most HID devices

### Linux

- Requires kernel 2.6.39 or later
- May require udev rules for non-root access
- Example udev rule (create `/etc/udev/rules.d/99-hidraw-permissions.rules`):
  ```
  KERNEL=="hidraw*", ATTRS{idVendor}=="1234", ATTRS{idProduct}=="5678", MODE="0666"
  ```

## Testing

The library includes comprehensive test suites with mocked dependencies (no hardware or network required):

### Run All Tests

```bash
cd build
make test
```

Output:
```
Running tests...
    Start 1: HID_Tests
1/2 Test #1: HID_Tests ........................   Passed    0.11 sec
    Start 2: Upload_Tests
2/2 Test #2: Upload_Tests .....................   Passed    0.01 sec

100% tests passed, 0 tests failed out of 2
```

### Test Suites

- **HID Tests** (`test_hid`): 20 tests covering HID communication and MDS protocol with mock hidapi
- **Upload Tests** (`test_upload`): 12 tests covering HTTP upload functionality with mock libcurl

### Run Individual Suites

```bash
# Run HID tests only
./test/test_hid

# Run upload tests only
./test/test_upload

# Run with CTest for detailed output
ctest --output-on-failure
ctest -R HID_Tests --verbose
```

See [test/README.md](test/README.md) for detailed testing documentation.

## Error Handling

All API functions that can fail return an error code. Use `memfault_hid_error_string()` to get a human-readable description:

```c
int ret = memfault_hid_open(vid, pid, NULL, &device);
if (ret != MEMFAULT_HID_SUCCESS) {
    fprintf(stderr, "Error: %s\n", memfault_hid_error_string(ret));
}
```

## Contributing

Contributions are welcome! Please:

1. Follow the existing code style
2. Add tests for new features
3. Update documentation
4. Ensure cross-platform compatibility

## License

MIT License - See LICENSE file for details

## Support

For issues, questions, or contributions, please visit:
https://github.com/memfault/memfault-cloud-hid
