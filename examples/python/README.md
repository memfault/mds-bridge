# Python MDS Example

This example demonstrates how to use the Memfault Diagnostic Service (MDS) protocol with a custom backend using Python's ctypes FFI.

## Quick Start

```bash
# 1. Build the C library
cd ../..
cmake -B build -DBUILD_SHARED_LIBS=ON
cmake --build build

# 2. Install Python dependencies
cd examples/python
pip3 install -r requirements.txt

# 3. Update main.py with your device VID/PID
# Edit the Config class at the top of main.py

# 4. Run the example
python3 main.py
```

The application will connect to your HID device, read its configuration, enable diagnostic streaming, and display received chunks. Press Ctrl+C to stop.

## Architecture

This example implements the pluggable backend architecture:

- **hidapi**: Wrapped in a custom backend class
- **C Library**: Calls back to Python for HID I/O operations
- **Python**: Implements backend interface, uses high-level MDS API

**Benefits:**
- Cleaner, simpler code
- Transport-agnostic design (works with HID, Serial, BLE, etc.)
- No manual buffer management or protocol details exposed
- Uses high-level MDS API
- Demonstrates pluggable architecture

## Files

- `requirements.txt` - Python dependencies
- `bindings.py` - ctypes bindings to the C library
- `hid_backend.py` - Custom backend implementing MDS backend interface
- `mds_client.py` - MDS client using custom backend
- `main.py` - Example application
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
5. Process incoming transport data:
   - MDS client automatically handles MDS protocol data
   - Custom reports are passed to your application logic
6. Optionally upload chunks to Memfault cloud

Press `Ctrl+C` to stop gracefully.

## How It Works

### 1. Custom Backend Implementation

The `HIDBackend` class implements the MDS backend interface using hidapi:

```python
class HIDBackend:
    def __init__(self, hid_device):
        self.device = hid_device

        # Create callbacks that the C library will invoke for HID I/O
        @BACKEND_READ_FN
        def read_callback(impl_data, report_id, buffer, length, timeout_ms):
            return self._handle_read(report_id, buffer, length, timeout_ms)

        @BACKEND_WRITE_FN
        def write_callback(impl_data, report_id, buffer, length):
            return self._handle_write(report_id, buffer, length)

        # Create backend structure with operations
        self.backend = mds_backend_t()
        self.backend.ops = ctypes.pointer(mds_backend_ops_t(
            read=read_callback,
            write=write_callback,
            destroy=destroy_callback
        ))

    def _handle_read(self, report_id, buffer, length, timeout_ms):
        # Feature reports (0x01-0x05)
        data = self.device.get_feature_report(report_id, length + 1)
        for i in range(len(data) - 1):
            buffer[i] = data[i + 1]  # Copy to C buffer
        return len(data) - 1

    def _handle_write(self, report_id, buffer, length):
        # Write feature reports
        data = bytes([buffer[i] for i in range(length)])
        self.device.write([report_id] + list(data))
        return length
```

### 2. MDS Session with Custom Backend

The MDS client creates a session using the custom backend:

```python
backend = HIDBackend(device)
session_ptr = ctypes.c_void_p()
lib.mds_session_create(backend.get_backend_ref(), ctypes.byref(session_ptr))
session = session_ptr

# Use high-level API - C library calls our backend for HID I/O
config = mds_device_config_t()
lib.mds_read_device_config(session, ctypes.byref(config))

# Enable streaming
lib.mds_stream_enable(session)
```

### 3. Processing Transport Data (Transport-Agnostic API)

The MDS client provides a clean, transport-agnostic API for processing data:

```python
# Set up chunk callback
def on_chunk(packet):
    print(f"Received chunk: seq={packet.sequence}, len={packet.length} bytes")
    mds_client.upload_chunk(packet.data)

mds_client.set_chunk_callback(on_chunk)

# Process incoming data
device.set_nonblocking(True)
while True:
    data = device.read(64)
    if data:
        # Let MDS process first - returns True if it handled the data
        if mds_client.process(bytes(data)):
            continue  # MDS handled it internally

        # Not MDS data - handle as custom report
        handle_custom_report(data)
```

**For multiplexed transports (HID, Serial):**
- Use `process(data)` where `data[0]` is the channel/report ID
- Returns `True` if MDS handled it, `False` if it's for your application

**For pre-demultiplexed transports (BLE GATT characteristics):**
- Use `process_stream_data(payload)` when receiving from the MDS characteristic
- No channel ID prefix needed (characteristic UUID already identifies it as MDS data)

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

1. **Add the required files to your project:**
   - Copy `bindings.py`, `hid_backend.py`, and `mds_client.py` to your project
   - Install dependencies: `pip install hidapi requests`

2. **Create an MDS client instance:**
   ```python
   import hid
   from mds_client import MDSClient

   device = hid.device()
   device.open_path(device_path)

   mds_client = MDSClient(device)
   mds_client.initialize()
   ```

3. **Set up chunk callback:**
   ```python
   def on_chunk(packet):
       print(f"Received chunk: {packet.length} bytes")
       mds_client.upload_chunk(packet.data)

   mds_client.set_chunk_callback(on_chunk)
   ```

4. **Enable streaming:**
   ```python
   mds_client.enable_streaming()
   ```

5. **Process incoming transport data:**
   ```python
   device.set_nonblocking(True)
   while True:
       data = device.read(64)
       if data:
           # Let MDS process first - returns True if it handled the data
           if mds_client.process(bytes(data)):
               continue  # MDS handled it internally

           # Not MDS data - handle as custom report
           handle_custom_report(data)
   ```

## Creating Custom Backends

You can implement backends for other transports (Serial, BLE, etc.) by following the same pattern:

1. **Implement the backend interface:**
   ```python
   class CustomBackend:
       def __init__(self, transport):
           self.transport = transport

           # Create read/write callbacks
           @BACKEND_READ_FN
           def read_callback(impl_data, report_id, buffer, length, timeout_ms):
               # Read from your transport
               data = self.transport.read(report_id, length)
               for i in range(len(data)):
                   buffer[i] = data[i]
               return len(data)

           @BACKEND_WRITE_FN
           def write_callback(impl_data, report_id, buffer, length):
               # Write to your transport
               data = bytes([buffer[i] for i in range(length)])
               self.transport.write(report_id, data)
               return length

           # Create backend structure
           self.backend = mds_backend_t()
           self.backend.ops = ctypes.pointer(mds_backend_ops_t(
               read=read_callback,
               write=write_callback,
               destroy=destroy_callback
           ))

       def get_backend_ref(self):
           return ctypes.byref(self.backend)
   ```

2. **Use it with MDS:**
   ```python
   backend = CustomBackend(my_transport)
   session_ptr = ctypes.c_void_p()
   lib.mds_session_create(backend.get_backend_ref(), ctypes.byref(session_ptr))
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
- `process(data: bytes) -> bool` - Process transport data (multiplexed transports like HID/Serial)
- `process_stream_data(payload: bytes)` - Process MDS stream payload (pre-demultiplexed transports like BLE)
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
