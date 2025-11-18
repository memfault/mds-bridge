# Node.js MDS Example

This directory contains a Node.js example demonstrating how to use the Memfault Diagnostic Service (MDS) protocol with native bindings.

## Architecture

This example uses **N-API** (Node.js Native API) for clean integration with the MDS protocol:

- **N-API Addon** (`src/addon.c`): Native bindings exposing MDS protocol parsing functions
- **JavaScript Client** (`mds-native.js`): Handles HID I/O and device management
- **Node-HID**: Provides cross-platform HID device access
- **Build System**: Uses node-gyp to compile the native addon

**Benefits:**
- Clean separation: JavaScript handles I/O, native code handles protocol parsing
- Fast native parsing of diagnostic stream packets
- No complex FFI type marshalling
- Standard Node.js addon approach
- Easy to debug and maintain

## Files

### Source Files
- `package.json` - Node.js dependencies and build scripts
- `binding.gyp` - Native addon build configuration
- `src/addon.c` - N-API bindings to MDS protocol functions
- `mds-native.js` - JavaScript client using native addon
- `index.js` - Example application
- `README.md` - This file

### Build Output
- `build/Release/memfault_hid_native.node` - Compiled native addon
- `build/index.js` - Copied application entry point
- `build/mds-native.js` - Copied client library

## Prerequisites

1. **Node.js 16+** with npm
2. **Build tools**:
   - macOS: Xcode Command Line Tools (`xcode-select --install`)
   - Linux: `build-essential`, `python3`
   - Windows: Visual Studio Build Tools

3. **Memfault HID library** must be built first:
   ```bash
   # From repository root
   cmake -B build -DBUILD_SHARED_LIBS=ON
   cmake --build build
   ```

## Installation

```bash
# Install dependencies and build native addon
npm install

# This runs:
# 1. node-gyp rebuild (compiles the N-API addon)
# 2. npm run copy-js (copies JS files to build/)
```

## Usage

### Basic Usage

```bash
# Run with device VID and PID (hex)
npm start -- <VID> <PID>

# Example with Zephyr device
npm start -- 2fe3 7
```

### Command Line Options

```bash
# Run with cloud upload enabled (default)
npm start -- 2fe3 7

# Disable cloud upload
npm start -- 2fe3 7 --no-upload

# Get help
npm start -- --help
```

### Development Mode

```bash
# Run from source directory (without copying to build/)
npm run dev -- 2fe3 7
```

## Build Scripts

```bash
# Build everything (addon + copy JS files)
npm run build

# Just copy JavaScript files to build/
npm run copy-js

# Rebuild the C library (memfault-hid)
npm run build:lib

# Clean all build artifacts
npm run clean
```

## How It Works

1. **Initialization**:
   - Opens HID device by VID/PID
   - Reads device configuration from HID feature reports
   - Parses config (device ID, data URI, auth key)

2. **Streaming**:
   - Sends stream enable command via HID feature report
   - Listens for HID input reports (diagnostic data)
   - Each report is parsed by the native addon
   - Parsed chunks are displayed or uploaded to Memfault cloud

3. **Native Addon Functions**:
   - `parseStreamPacket(buffer)` - Parse MDS stream packet
   - `validateSequence(lastSeq, currentSeq)` - Validate packet sequence
   - `MDS_REPORT_ID` - Constants for HID report IDs

## Example Output

```
╔════════════════════════════════════════════════════════════╗
║  Memfault MDS Gateway                                     ║
║  Demonstrates pluggable backend architecture              ║
╚════════════════════════════════════════════════════════════╝

Looking for device 2fe3:7...
Found: Zephyr Project USBD sample
Path: DevSrvsID:4295818576

Creating MDS client with Node HID backend...
[MDSClient] Initializing with native protocol...
[MDSClient] Reading device configuration...
[MDSClient] Device configuration:
  Device ID: mds-hid-dev-001
  Data URI: https://chunks-nrf.memfault.com/api/v0/chunks/mds-hid-dev-001
  Auth: Memfault-Project-Key: WUtwoBUkhhd90WN9ykj01dXqYsKiP67y
  Features: 0x1f

Enabling diagnostic streaming...
[MDSClient] Streaming enabled

Listening for diagnostic chunks... (Press Ctrl+C to stop)

[MDSClient] Received chunk: seq=0, len=63
[Chunk #1] Seq=0, Length=63 bytes
```

## Troubleshooting

### Build Errors

**Error: `Cannot find module 'node-gyp'`**
```bash
npm install -g node-gyp
npm install
```

**Error: `Library not loaded: @rpath/libmemfault_hid.dylib`**

The C library wasn't built or isn't in the correct location:
```bash
npm run build:lib  # Rebuild C library
npm run build      # Rebuild addon
```

### Runtime Errors

**Error: `could not get feature report from device`**

- Check device permissions (may need sudo on Linux)
- Verify VID/PID are correct
- Try unplugging and replugging the device

**Error: `Failed to parse packet`**

- Device may not be sending MDS-compatible data
- Check that streaming is enabled
- Verify device firmware supports MDS protocol

## API Reference

### MDSClient

```javascript
import { MDSClient } from './mds-native.js';

const client = new MDSClient(hidDevice);

// Initialize and read config
await client.initialize();

// Enable streaming
await client.enableStreaming();

// Set chunk callback
client.setChunkCallback((packet) => {
  console.log(`Chunk: seq=${packet.sequence}, len=${packet.length}`);
  // packet.data contains the chunk Buffer
});

// Disable streaming
await client.disableStreaming();

// Cleanup
client.destroy();
```

### Native Addon

```javascript
import { createRequire } from 'module';
const require = createRequire(import.meta.url);
const native = require('./build/Release/memfault_hid_native.node');

// Parse stream packet
const packet = native.parseStreamPacket(buffer);
// Returns: { sequence, data, length }

// Validate sequence
const valid = native.validateSequence(lastSeq, currentSeq);

// Constants
console.log(native.MDS_REPORT_ID.STREAM_DATA); // 0x06
```

## License

MIT
