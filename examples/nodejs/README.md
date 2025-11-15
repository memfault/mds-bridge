# Node.js MDS Example

This example demonstrates how to use the Memfault HID library from Node.js using FFI (Foreign Function Interface). It shows how to run MDS (Memfault Diagnostic Service) streaming in parallel with custom HID application logic.

## Architecture

The example uses a hybrid approach:

- **Node-HID**: Handles all actual HID I/O (reading/writing reports)
- **C Library (via FFI)**: Provides MDS protocol logic (parsing, validation, sequence tracking)
- **JavaScript**: Application logic and orchestration

This architecture allows you to:

1. Use existing Node.js HID libraries and ecosystem
2. Leverage the robust C implementation of MDS protocol
3. Run MDS streaming alongside your custom HID application
4. Avoid re-implementing the MDS protocol in JavaScript

## Files

- `package.json` - Node.js dependencies and build scripts
- `bindings.js` - Low-level FFI bindings to the C library
- `mds-client.js` - High-level MDS client wrapper
- `index.js` - Example application showing parallel HID usage
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

2. **Install Node.js dependencies:**

```bash
cd examples/nodejs
npm install
```

## Configuration

Edit `index.js` and update the CONFIG section with your device's VID/PID:

```javascript
const CONFIG = {
  vendorId: 0x1234,    // Your device's Vendor ID
  productId: 0x5678,   // Your device's Product ID
  uploadChunks: true,  // Enable/disable cloud uploads
};
```

## Running the Example

```bash
npm start
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

### 1. HID I/O with Node-HID

All HID communication is handled by Node-HID:

```javascript
const device = new HID.HID(devicePath);

// Read feature reports
const data = device.getFeatureReport(reportId, length);

// Write output reports
device.write([reportId, ...data]);

// Listen for input reports
device.on('data', (data) => {
  // Handle incoming data
});
```

### 2. MDS Protocol via C Library

The C library handles protocol logic through buffer-based functions:

```javascript
// Parse a stream packet
const packet = new mds_stream_packet_t();
lib.mds_parse_stream_packet(buffer, length, packet.ref());

// Validate sequence numbers
const isValid = lib.mds_validate_sequence(prevSeq, newSeq);

// Build stream control reports
lib.mds_build_stream_control(enable, buffer, length);
```

### 3. Parallel Operation

The application routes HID reports to the appropriate handler:

```javascript
handleHIDData(data) {
  const reportId = data[0];

  // Custom application reports
  if (reportId !== MDS_REPORT_ID.STREAM_DATA) {
    this.customApp.processCustomReport(data);
    return;
  }

  // MDS stream data
  this.mdsClient.processHIDData(data);
}
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
  Features: 0x0

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

To use MDS in your existing Node.js HID application:

1. **Add the FFI bindings and MDS client:**
   - Copy `bindings.js` and `mds-client.js` to your project
   - Install dependencies: `npm install ffi-napi ref-napi ref-struct-napi ref-array-napi`

2. **Create an MDS client instance:**
   ```javascript
   import { MDSClient } from './mds-client.js';

   const mdsClient = new MDSClient(yourHIDDevice);
   await mdsClient.initialize();
   ```

3. **Enable streaming:**
   ```javascript
   await mdsClient.enableStreaming();
   ```

4. **Route MDS reports in your data handler:**
   ```javascript
   yourHIDDevice.on('data', (data) => {
     if (data[0] === MDS_REPORT_ID.STREAM_DATA) {
       mdsClient.processHIDData(data);
     } else {
       // Your custom application logic
       handleCustomReport(data);
     }
   });
   ```

5. **Handle received chunks:**
   ```javascript
   mdsClient.setChunkCallback(async (packet) => {
     console.log(`Received chunk: ${packet.length} bytes`);
     await mdsClient.uploadChunk(packet.data);
   });
   ```

## API Reference

### MDSClient

#### Constructor
```javascript
new MDSClient(hidDevice)
```

#### Methods

- `async initialize()` - Initialize MDS session and read device config
- `async readDeviceConfig()` - Read device configuration from device
- `async enableStreaming()` - Enable diagnostic data streaming
- `async disableStreaming()` - Disable streaming
- `processHIDData(data)` - Process incoming HID input report
- `async uploadChunk(chunkData)` - Upload chunk data to Memfault cloud
- `setChunkCallback(callback)` - Set callback for received chunks
- `getConfig()` - Get device configuration
- `isStreaming()` - Check if streaming is enabled
- `destroy()` - Clean up resources

## Troubleshooting

### Library not found

If you get an error loading the library:

1. Make sure you built the C library: `npm run build:lib`
2. Check that the library exists in `../../build/`
3. Verify the library path in `bindings.js` matches your platform

### Device not found

1. Run with any VID/PID to see available devices
2. Update CONFIG with your device's actual VID/PID
3. Ensure you have permissions to access HID devices (may need sudo on Linux)

### FFI errors

Make sure all dependencies are installed:

```bash
npm install
```

If you're on macOS with Apple Silicon, you may need to rebuild native modules:

```bash
npm rebuild
```

## License

MIT
