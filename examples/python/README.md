# Python MDS Example

This example demonstrates how to use the Memfault HID library from Python using ctypes (Python's built-in FFI). It shows how to run MDS (Memfault Diagnostic Service) streaming in parallel with custom HID application logic.

## Architecture

The example uses a hybrid approach:

- **hidapi (Python)**: Handles all actual HID I/O (reading/writing reports)
- **C Library (via ctypes)**: Provides MDS protocol logic (parsing, validation, sequence tracking)
- **Python**: Application logic and orchestration

This architecture allows you to:

1. Use Python's native HID libraries and ecosystem
2. Leverage the robust C implementation of MDS protocol
3. Run MDS streaming alongside your custom HID application
4. Avoid re-implementing the MDS protocol in Python

## Files

- `requirements.txt` - Python dependencies
- `bindings.py` - Low-level ctypes bindings to the C library
- `mds_client.py` - High-level MDS client wrapper
- `main.py` - Example application showing parallel HID usage
- `README.md` - This file

## Prerequisites

1. **Build the C library first:**

```bash
cd ../..
cmake -B build -DBUILD_SHARED_LIBS=ON
cmake --build build
```

This creates the shared library:
- macOS: `build/libmemfault_hid.dylib`
- Linux: `build/libmemfault_hid.so`
- Windows: `build/memfault_hid.dll`

2. **Install Python dependencies:**

```bash
cd examples/python
pip install -r requirements.txt
```

Or with a virtual environment (recommended):

```bash
python3 -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate
pip install -r requirements.txt
```

## Configuration

Edit `main.py` and update the `Config` class with your device's VID/PID:

```python
@dataclass
class Config:
    vendor_id: int = 0x1234    # Your device's Vendor ID
    product_id: int = 0x5678   # Your device's Product ID
    upload_chunks: bool = True # Enable/disable cloud uploads
```

## Running the Example

```bash
python main.py
```

Or make it executable:

```bash
chmod +x main.py
./main.py
```

The application will:

1. Connect to your HID device
2. Initialize the MDS client
3. Read device configuration (device ID, URI, auth)
4. Enable diagnostic data streaming
5. Process incoming data:
   - Route MDS stream data to the MDS client
   - Route custom reports to your application logic
6. Optionally upload chunks to Memfault cloud

Press `Ctrl+C` to stop gracefully.

## How It Works

### 1. HID I/O with hidapi

All HID communication is handled by the Python `hid` library:

```python
import hid

# Open device
device = hid.device()
device.open_path(device_path)

# Read feature reports
data = device.get_feature_report(report_id, length)

# Write output reports
device.write([report_id, ...data])

# Read input reports (non-blocking)
device.set_nonblocking(True)
data = device.read(64, timeout_ms=100)
```

### 2. MDS Protocol via C Library

The C library handles protocol logic through ctypes:

```python
import ctypes
from bindings import lib, mds_stream_packet_t

# Parse a stream packet
packet = mds_stream_packet_t()
lib.mds_parse_stream_packet(buffer, length, ctypes.byref(packet))

# Validate sequence numbers
is_valid = lib.mds_validate_sequence(prev_seq, new_seq)

# Build stream control reports
buffer = (ctypes.c_uint8 * 1)()
lib.mds_build_stream_control(True, buffer, 1)
```

### 3. Parallel Operation

The application routes HID reports to the appropriate handler:

```python
def handle_hid_data(self, data: bytes) -> None:
    # Custom application reports
    handled = self.custom_app.process_custom_report(data)
    if handled:
        return

    # MDS stream data
    self.mds_client.process_hid_data(data)
```

## Example Output

```
============================================================
Memfault HID + MDS Example Application
============================================================

Looking for HID device (VID: 0x1234, PID: 0x5678)...
Found device: Example Manufacturer Example Product
  Path: /dev/hidraw0

MDS Device Configuration:
  Device ID: DEVICE-12345
  Data URI: https://chunks.memfault.com/api/v0/chunks/PROJ_KEY
  Auth: Memfault-Project-Key:abc123...
  Features: 0x00000000

[MDS] Streaming enabled

Application running. Press Ctrl+C to exit.

[MDS] Chunk received: seq=0, len=63 bytes
[MDS] Chunk uploaded successfully
[CustomApp] Performing periodic task...
[MDS] Chunk received: seq=1, len=63 bytes
[MDS] Chunk uploaded successfully

[Stats] Chunks: 15 received, 15 uploaded, Custom reports: 3
```

## Integrating with Your Application

To use MDS in your existing Python HID application:

1. **Add the bindings and MDS client:**
   - Copy `bindings.py` and `mds_client.py` to your project
   - Install dependencies: `pip install hidapi requests`

2. **Create an MDS client instance:**
   ```python
   from mds_client import MDSClient

   mds_client = MDSClient(your_hid_device)
   mds_client.initialize()
   ```

3. **Enable streaming:**
   ```python
   mds_client.enable_streaming()
   ```

4. **Route MDS reports in your data handler:**
   ```python
   from bindings import MDS_REPORT_ID

   data = your_hid_device.read(64)
   if data and data[0] == MDS_REPORT_ID.STREAM_DATA:
       mds_client.process_hid_data(bytes(data))
   else:
       # Your custom application logic
       handle_custom_report(data)
   ```

5. **Handle received chunks:**
   ```python
   def on_chunk(packet):
       print(f"Received chunk: {packet.length} bytes")
       mds_client.upload_chunk(packet.data)

   mds_client.set_chunk_callback(on_chunk)
   ```

## API Reference

### MDSClient

#### Constructor
```python
MDSClient(hid_device)
```

#### Methods

- `initialize()` - Initialize MDS session and read device config
- `read_device_config() -> DeviceConfig` - Read device configuration from device
- `enable_streaming()` - Enable diagnostic data streaming
- `disable_streaming()` - Disable streaming
- `process_hid_data(data: bytes) -> Optional[StreamPacket]` - Process incoming HID input report
- `upload_chunk(chunk_data: bytes) -> bool` - Upload chunk data to Memfault cloud
- `set_chunk_callback(callback: Callable)` - Set callback for received chunks
- `get_config() -> DeviceConfig` - Get device configuration
- `is_streaming() -> bool` - Check if streaming is enabled
- `destroy()` - Clean up resources

#### Context Manager

The `MDSClient` can be used as a context manager:

```python
with MDSClient(hid_device) as mds:
    mds.initialize()
    mds.enable_streaming()
    # ... use the client ...
# Automatically cleaned up
```

### Data Classes

#### DeviceConfig
```python
@dataclass
class DeviceConfig:
    supported_features: int
    device_identifier: str
    data_uri: str
    authorization: str
```

#### StreamPacket
```python
@dataclass
class StreamPacket:
    sequence: int      # Sequence number (0-31)
    data: bytes        # Chunk data
    length: int        # Length of chunk data
```

## Troubleshooting

### Library not found

If you get an error loading the library:

1. Make sure you built the C library:
   ```bash
   cd ../..
   cmake -B build -DBUILD_SHARED_LIBS=ON
   cmake --build build
   ```

2. Check that the library exists in `../../build/`
3. Verify the library path in `bindings.py` matches your platform

### Device not found

1. Run with any VID/PID to see available devices
2. Update `Config` with your device's actual VID/PID
3. Ensure you have permissions to access HID devices:
   - **Linux**: You may need to add udev rules or run with sudo
   - **macOS**: Should work without special permissions
   - **Windows**: May require running as Administrator

### hidapi installation issues

If you have trouble installing hidapi:

**macOS:**
```bash
brew install hidapi
pip install hidapi
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get install libhidapi-dev
pip install hidapi
```

**Linux (Fedora/RHEL):**
```bash
sudo dnf install hidapi-devel
pip install hidapi
```

**Windows:**
```bash
pip install hidapi
```

If the above doesn't work, try the alternative package:
```bash
pip install hid
```

### ctypes errors

Make sure the C library was built as a shared library (`.dylib`/`.so`/`.dll`).

If you see symbol errors, verify the exports:

**macOS:**
```bash
nm -gU build/libmemfault_hid.dylib | grep mds_
```

**Linux:**
```bash
nm -D build/libmemfault_hid.so | grep mds_
```

## Platform-Specific Notes

### macOS

- Works out of the box with Homebrew-installed hidapi
- Uses `libhidapi.dylib` from the build directory

### Linux

- May require udev rules for non-root access
- Create `/etc/udev/rules.d/99-hid.rules`:
  ```
  SUBSYSTEM=="hidraw", ATTRS{idVendor}=="1234", ATTRS{idProduct}=="5678", MODE="0666"
  ```
  Replace `1234` and `5678` with your VID/PID
- Reload rules: `sudo udevadm control --reload-rules`

### Windows

- May require running as Administrator for HID access
- Uses `memfault_hid.dll` from the build directory

## License

MIT
