# MDS Bridge Library

A cross-platform C library implementing the Memfault Diagnostic Service (MDS) protocol with pluggable backend support. The library provides a transport-agnostic protocol layer with built-in HID backend, enabling diagnostic data bridging from embedded devices to gateway applications.

## Features

- **Transport-agnostic MDS protocol**: Pluggable backend architecture supports HID, Serial, BLE, and custom transports
- **Cross-platform support**: Windows, macOS, and Linux
- **Built on HIDAPI**: Reliable cross-platform HID communication via built-in HID backend
- **HTTP chunk uploader**: Built-in libcurl-based uploader for Memfault cloud integration
- **Simple high-level API**: Automatic device management and initialization
- **Pure C implementation**: Maximum compatibility and portability
- **Comprehensive testing**: Full test suite with mocked dependencies (no hardware required)
- **Language bindings**: Python and Node.js examples with native bindings

## Architecture

The library uses a layered architecture with pluggable backends:

```
┌─────────────────────────────────────────────────┐
│           Application Layer                     │
│  (Your code, Python/Node.js bindings, etc.)    │
└─────────────────────────────────────────────────┘
                      ↓
┌─────────────────────────────────────────────────┐
│        MDS Protocol Layer (mds_protocol.h)      │
│  • Session management                           │
│  • Device configuration (features, URI, auth)   │
│  • Stream control (enable/disable)              │
│  • Packet processing (parse, validate, upload)  │
│  • Sequence tracking                            │
└─────────────────────────────────────────────────┘
                      ↓
┌─────────────────────────────────────────────────┐
│      Backend Interface (mds_backend.h)          │
│  • read(report_id, buffer, timeout)             │
│  • write(report_id, buffer)                     │
│  • destroy()                                    │
└─────────────────────────────────────────────────┘
           ↓                    ↓
  ┌────────────────┐   ┌──────────────────┐
  │  HID Backend   │   │  Custom Backends │
  │  (built-in)    │   │  (Serial, BLE,   │
  │                │   │   WebSocket...)  │
  └────────────────┘   └──────────────────┘
           ↓
  ┌────────────────┐
  │    HIDAPI      │
  │  (USB HID I/O) │
  └────────────────┘
```

### Backend Architecture

The MDS protocol is completely **transport-agnostic** through the backend interface. Any transport that can provide READ/WRITE operations can be used:

**Built-in HID Backend** (`mds_backend_hid.c`):
- Implements the backend interface using HIDAPI
- Maps MDS report IDs to HID GET_FEATURE/SET_FEATURE/READ operations
- Automatically initialized when using `mds_session_create_hid()`

**Custom Backend Support**:
- Implement the `mds_backend_ops_t` vtable with read/write/destroy functions
- Pass your backend to `mds_session_create()` for full protocol support
- Examples: Serial port, BLE GATT, WebSocket, or event-driven I/O (see Python/Node.js examples)

**Event-driven I/O**:
- For event-driven transports (node-hid, hidapi in non-blocking mode), you can:
  - Create a session with NULL backend: `mds_session_create(NULL, &session)`
  - Use `mds_process_stream_from_bytes()` to process pre-received data
  - The C library handles parsing, sequence validation, and upload callbacks

This design enables the same MDS protocol code to work across different transports without modification.

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

#### libcurl (for HTTP chunk uploading)

**macOS:**
```bash
brew install curl
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get install libcurl4-openssl-dev
```

**Windows:**
- Usually included with Windows
- Or use vcpkg: `vcpkg install curl`

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

### Quick Start - MDS Protocol over HID

The primary use case is implementing the Memfault Diagnostic Service (MDS) protocol over USB HID. The library provides a simplified API that handles device management automatically.

```c
#include "mds_bridge/mds_protocol.h"

int main(void) {
    // Create MDS session (opens HID device and initializes automatically)
    mds_session_t *session = NULL;
    int ret = mds_session_create_hid(0x1234, 0x5678, NULL, &session);
    if (ret != 0) {
        fprintf(stderr, "Failed to create MDS session\n");
        return 1;
    }

    // Read device configuration
    mds_device_config_t config;
    mds_read_device_config(session, &config);
    printf("Device ID: %s\n", config.device_identifier);
    printf("Data URI: %s\n", config.data_uri);

    // Enable streaming
    mds_stream_enable(session);

    // Process stream packets with automatic upload
    while (running) {
        mds_stream_packet_t packet;
        ret = mds_process_stream(session, &config, 5000, &packet);
        if (ret == 0) {
            printf("Received chunk: seq=%d, len=%zu\n",
                   packet.sequence, packet.data_len);
            // Upload callback (if registered) was automatically called
        }
    }

    // Cleanup (disables streaming and closes device automatically)
    mds_session_destroy(session);
    return 0;
}
```

### MDS Protocol Overview

The MDS protocol uses HID reports to communicate diagnostic data:

**Feature Reports** (Configuration & Control):
- `0x01`: Supported features bitmask (read-only)
- `0x02`: Device identifier string (read-only)
- `0x03`: Data URI for chunk upload (read-only)
- `0x04`: Authorization header (read-only, e.g., project key)
- `0x05`: Stream control (read-write, enable/disable streaming)

**Input Reports** (Device → Host):
- `0x06`: Stream data packets with diagnostic chunks

Each stream packet includes:
- **Sequence counter** (5-bit, 0-31, wraps around) for detecting dropped packets
- **Chunk data payload** (up to 63 bytes per packet)

### MDS API Functions

**Session Management:**
- `mds_session_create_hid(vid, pid, serial, &session)` - Create session with HID backend
- `mds_session_create_hid_path(path, &session)` - Create session with HID backend (device path)
- `mds_session_create(backend, &session)` - Create session with custom backend
- `mds_session_destroy(session)` - Destroy session and cleanup

**Device Configuration:**
- `mds_read_device_config(session, &config)` - Read all configuration
- `mds_get_supported_features(session, &features)` - Get feature bitmask
- `mds_get_device_identifier(session, buffer, size)` - Get device ID
- `mds_get_data_uri(session, buffer, size)` - Get upload URI
- `mds_get_authorization(session, buffer, size)` - Get auth header

**Stream Control:**
- `mds_stream_enable(session)` - Enable diagnostic data streaming
- `mds_stream_disable(session)` - Disable streaming

**Data Reception:**
- `mds_stream_read_packet(session, &packet, timeout_ms)` - Read packet (blocking I/O)
- `mds_process_stream(session, &config, timeout_ms, &packet)` - Read + validate + upload
- `mds_process_stream_from_bytes(session, &config, buffer, len, &packet)` - Parse pre-received data

**Chunk Upload:**
- `mds_set_upload_callback(session, callback, user_data)` - Register upload callback

### Uploading Chunks to Memfault Cloud

The library supports both custom upload callbacks and a built-in HTTP uploader.

**Option 1: Custom Upload Callback**

```c
int my_upload_callback(const char *uri, const char *auth_header,
                       const uint8_t *chunk_data, size_t chunk_len,
                       void *user_data) {
    // Parse authorization header (format: "HeaderName:HeaderValue")
    // POST chunk_data to uri with appropriate headers
    // Content-Type: application/octet-stream
    return 0; /* Return 0 on success, negative on error */
}

// Register callback
mds_set_upload_callback(session, my_upload_callback, my_context);

// Process streams - callback is automatically invoked
while (running) {
    mds_process_stream(session, &config, 5000, NULL);
}
```

**Option 2: Built-in HTTP Uploader**

```c
#include "mds_bridge/chunks_uploader.h"

// Create uploader
chunks_uploader_t *uploader = chunks_uploader_create();
chunks_uploader_set_verbose(uploader, true);

// Register built-in uploader
mds_set_upload_callback(session, chunks_uploader_callback, uploader);

// Process streams - chunks are automatically uploaded
while (running) {
    mds_process_stream(session, &config, 5000, NULL);
}

// Get stats
chunks_upload_stats_t stats;
chunks_uploader_get_stats(uploader, &stats);
printf("Uploaded: %zu chunks, %zu bytes\n",
       stats.chunks_uploaded, stats.bytes_uploaded);

// Cleanup
chunks_uploader_destroy(uploader);
```

### Device Enumeration

For applications that need to list/select HID devices:

```c
#include "mds_bridge/memfault_hid.h"

// Initialize HID library
memfault_hid_init();

// Enumerate devices
memfault_hid_device_info_t *devices = NULL;
size_t num_devices = 0;

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

// Cleanup
memfault_hid_exit();
```

**Note**: When using `mds_session_create_hid()`, the HID library is initialized automatically.

### Custom Backend Example

Implement your own transport by providing the backend vtable:

```c
#include "mds_bridge/mds_backend.h"

// Your backend state
typedef struct {
    int serial_fd;  // Or BLE handle, WebSocket, etc.
} my_backend_t;

// Backend read operation
int my_backend_read(void *impl_data, uint8_t report_id,
                    uint8_t *buffer, size_t length, int timeout_ms) {
    my_backend_t *backend = impl_data;
    // Read from your transport
    return bytes_read;
}

// Backend write operation
int my_backend_write(void *impl_data, uint8_t report_id,
                     const uint8_t *buffer, size_t length) {
    my_backend_t *backend = impl_data;
    // Write to your transport
    return bytes_written;
}

// Backend cleanup
void my_backend_destroy(void *impl_data) {
    my_backend_t *backend = impl_data;
    // Close/cleanup your transport
    free(backend);
}

// Create backend
mds_backend_ops_t ops = {
    .read = my_backend_read,
    .write = my_backend_write,
    .destroy = my_backend_destroy
};

my_backend_t *backend_impl = /* initialize your transport */;

mds_backend_t backend = {
    .ops = &ops,
    .impl_data = backend_impl
};

// Create MDS session with your backend
mds_session_t *session;
mds_session_create(&backend, &session);
```

## Examples

The library includes example programs in C, Python, and Node.js:

### C Examples

- **`mds_gateway`**: Full MDS gateway that uploads diagnostic chunks to Memfault cloud
- **`mds_monitor`**: Real-time monitor for inspecting MDS stream data

```bash
# MDS gateway - upload chunks to Memfault cloud
./build/examples/mds_gateway 2fe3 0007

# MDS gateway - dry-run mode (print chunks without uploading)
./build/examples/mds_gateway 2fe3 0007 --dry-run

# MDS monitor - display stream data in real-time
./build/examples/mds_monitor 2fe3 0007

# MDS monitor - interactive device selection
./build/examples/mds_monitor
```

### Python Example

Uses ctypes bindings with a custom Python backend:

```bash
cd build/examples/python
python3 main.py 2fe3 0007
```

The Python example demonstrates:
- Custom backend implementation bridging hidapi-python with the C library
- Event-driven I/O using `mds_process_stream_from_bytes()`
- Upload callback registration from Python

### Node.js Example

Uses N-API native addon:

```bash
cd build/examples/nodejs
npm start -- 2fe3 0007
```

The Node.js example demonstrates:
- N-API addon for native C bindings
- Integration with node-hid for HID I/O
- Event-driven processing with the MDS protocol

See [examples/README.md](examples/README.md) for detailed documentation.

## API Headers

The library provides three public headers:

- **`mds_bridge/memfault_hid.h`** - Device enumeration and library initialization
- **`mds_bridge/mds_protocol.h`** - High-level MDS protocol API
- **`mds_bridge/mds_backend.h`** - Backend interface for custom transports
- **`mds_bridge/chunks_uploader.h`** - Built-in HTTP uploader

Most applications only need `mds_protocol.h`.

## Testing

The library includes comprehensive test suites with mocked dependencies (no hardware or network required):

```bash
cd build
make test
```

Output:
```
Running tests...
Test project /path/to/build
    Start 1: HID_Tests
1/3 Test #1: HID_Tests ........................   Passed    0.11 sec
    Start 2: Upload_Tests
2/3 Test #2: Upload_Tests .....................   Passed    0.01 sec
    Start 3: MDS_E2E_Test
3/3 Test #3: MDS_E2E_Test .....................   Passed    0.01 sec

100% tests passed, 0 tests failed out of 3
```

### Test Suites

- **HID Tests** (`test_hid`): 20 tests covering HID communication and MDS protocol with mock hidapi
- **Upload Tests** (`test_upload`): 12 tests covering HTTP upload functionality with mock libcurl
- **E2E Integration Test** (`test_mds_e2e`): Complete gateway workflow test with mocked device and cloud

See [test/README.md](test/README.md) for detailed testing documentation.

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

## Error Handling

All API functions that can fail return an error code. Use `memfault_hid_error_string()` to get a human-readable description:

```c
int ret = mds_session_create_hid(vid, pid, NULL, &session);
if (ret != 0) {
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
https://github.com/memfault/mds-bridge
