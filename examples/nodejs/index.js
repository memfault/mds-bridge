/**
 * MDS Gateway Example - Using Custom Backend
 *
 * This example demonstrates the pluggable backend architecture by creating
 * a custom Node.js backend that implements the MDS backend interface.
 *
 * Key features:
 * - Creates a NodeHIDBackend that bridges node-hid with the C library
 * - Uses high-level MDS API (mds_read_device_config, mds_stream_enable, etc.)
 * - No manual buffer parsing for configuration data
 * - Demonstrates transport-agnostic protocol design
 *
 * Benefits:
 * - Cleaner, simpler code
 * - Less duplication between C and JS
 * - Demonstrates how to create custom backends for other transports (Serial, BLE, etc.)
 *
 * Usage:
 *   node index.js                    # Use default VID/PID
 *   node index.js <vid> <pid>        # Specify VID/PID in hex
 *   node index.js 0x1234 0x5678
 *   node index.js 1234 5678 --no-upload  # Disable cloud uploads
 */

import HID from 'node-hid';
import { MDSClient } from './mds-client.js';

// Configuration - Default values, can be overridden via CLI
let VENDOR_ID = 0x1234;   // Replace with your device's VID
let PRODUCT_ID = 0x5678;   // Replace with your device's PID
let UPLOAD_TO_CLOUD = false; // Set to true to upload chunks to Memfault

/**
 * Parse command line arguments
 */
function parseArgs() {
  const args = process.argv.slice(2);

  if (args.includes('--help') || args.includes('-h')) {
    console.log(`
Memfault MDS Gateway Example

Usage:
  node index.js [vid] [pid] [--no-upload]

Arguments:
  vid         USB Vendor ID (hex, with or without 0x prefix)
  pid         USB Product ID (hex, with or without 0x prefix)
  --no-upload Disable chunk uploads to Memfault cloud

Examples:
  node index.js                    # Use default VID/PID
  node index.js 1234 5678          # VID=0x1234, PID=0x5678
  node index.js 0x1234 0x5678      # Same with 0x prefix
  node index.js 1234 5678 --no-upload  # Disable uploads
`);
    process.exit(0);
  }

  // Filter out flags
  const positional = args.filter(arg => !arg.startsWith('--'));
  const flags = args.filter(arg => arg.startsWith('--'));

  // Parse VID/PID
  if (positional.length >= 2) {
    VENDOR_ID = parseInt(positional[0], 16);
    PRODUCT_ID = parseInt(positional[1], 16);

    if (isNaN(VENDOR_ID) || isNaN(PRODUCT_ID)) {
      console.error('Error: VID and PID must be valid hex numbers');
      process.exit(1);
    }
  } else if (positional.length === 1) {
    console.error('Error: Both VID and PID must be specified together');
    process.exit(1);
  }

  // Parse flags
  if (flags.includes('--no-upload')) {
    UPLOAD_TO_CLOUD = false;
  }
}

// Parse CLI arguments
parseArgs();

// Statistics
let stats = {
  chunksReceived: 0,
  bytesReceived: 0,
  chunksUploaded: 0,
  uploadErrors: 0,
  lastChunkTime: null,
};

/**
 * Main application
 */
async function main() {
  console.log('╔════════════════════════════════════════════════════════════╗');
  console.log('║  Memfault MDS Gateway                                     ║');
  console.log('║  Demonstrates pluggable backend architecture              ║');
  console.log('╚════════════════════════════════════════════════════════════╝\n');

  // Find and open HID device
  console.log(`Looking for device ${VENDOR_ID.toString(16)}:${PRODUCT_ID.toString(16)}...`);
  const devices = HID.devices();
  const deviceInfo = devices.find(d => d.vendorId === VENDOR_ID && d.productId === PRODUCT_ID);

  if (!deviceInfo) {
    console.error(`Device not found: ${VENDOR_ID.toString(16)}:${PRODUCT_ID.toString(16)}`);
    console.error('\nAvailable devices:');
    devices.forEach(d => {
      console.error(`  ${d.vendorId.toString(16)}:${d.productId.toString(16)} - ${d.manufacturer} ${d.product}`);
    });
    process.exit(1);
  }

  console.log(`Found: ${deviceInfo.manufacturer} ${deviceInfo.product}`);
  console.log(`Path: ${deviceInfo.path}\n`);

  const device = new HID.HID(deviceInfo.path);

  try {
    // Create MDS client with custom backend
    console.log('Creating MDS client with Node HID backend...');
    const mdsClient = new MDSClient(device);

    // Initialize session and read configuration
    await mdsClient.initialize();

    const config = mdsClient.getConfig();
    console.log('\n╔════════════════════════════════════════════════════════════╗');
    console.log('║  Device Configuration                                      ║');
    console.log('╠════════════════════════════════════════════════════════════╣');
    console.log(`║  Device ID:     ${config.deviceIdentifier.padEnd(43)}║`);
    console.log(`║  Data URI:      ${config.dataUri.substring(0, 43).padEnd(43)}║`);
    if (config.dataUri.length > 43) {
      console.log(`║                 ${config.dataUri.substring(43).padEnd(43)}║`);
    }
    console.log(`║  Authorization: ${config.authorization.substring(0, 43).padEnd(43)}║`);
    if (config.authorization.length > 43) {
      console.log(`║                 ${config.authorization.substring(43).padEnd(43)}║`);
    }
    console.log(`║  Features:      0x${config.supportedFeatures.toString(16).padStart(8, '0').padEnd(41)}║`);
    console.log('╚════════════════════════════════════════════════════════════╝\n');

    // Set up chunk callback
    mdsClient.setChunkCallback(async (packet) => {
      stats.chunksReceived++;
      stats.bytesReceived += packet.length;
      stats.lastChunkTime = new Date();

      console.log(`\n[Chunk #${stats.chunksReceived}] Seq=${packet.sequence}, Length=${packet.length} bytes`);

      // Upload to cloud if enabled
      if (UPLOAD_TO_CLOUD) {
        const uploaded = await mdsClient.uploadChunk(packet.data);
        if (uploaded) {
          stats.chunksUploaded++;
          console.log(`  ✓ Uploaded to Memfault cloud`);
        } else {
          stats.uploadErrors++;
          console.log(`  ✗ Upload failed`);
        }
      } else {
        console.log(`  (Upload disabled - set UPLOAD_TO_CLOUD=true to enable)`);
      }
    });

    // Handle incoming HID data
    device.on('data', (data) => {
      // Let MDS process first - returns true if it handled the data
      if (mdsClient.process(Buffer.from(data))) {
        return;  // MDS handled it
      }

      // Not MDS data - handle as custom report
      // (Add your custom HID report handling here)
    });

    device.on('error', (error) => {
      console.error('HID error:', error);
    });

    // Enable streaming
    console.log('Enabling diagnostic streaming...');
    await mdsClient.enableStreaming();
    console.log('✓ Streaming enabled\n');
    console.log('Listening for diagnostic chunks... (Press Ctrl+C to stop)\n');

    // Print statistics periodically
    const statsInterval = setInterval(() => {
      printStatistics();
    }, 30000); // Every 30 seconds

    // Handle graceful shutdown
    const shutdown = async () => {
      console.log('\n\nShutting down...');
      clearInterval(statsInterval);

      console.log('Disabling streaming...');
      await mdsClient.disableStreaming();

      console.log('Cleaning up...');
      mdsClient.destroy();
      device.close();

      printStatistics();
      console.log('\n✓ Shutdown complete');
      process.exit(0);
    };

    process.on('SIGINT', shutdown);
    process.on('SIGTERM', shutdown);

  } catch (error) {
    console.error('Error:', error);
    device.close();
    process.exit(1);
  }
}

/**
 * Print statistics
 */
function printStatistics() {
  console.log('\n╔════════════════════════════════════════════════════════════╗');
  console.log('║  Statistics                                                ║');
  console.log('╠════════════════════════════════════════════════════════════╣');
  console.log(`║  Chunks received:   ${stats.chunksReceived.toString().padEnd(40)}║`);
  console.log(`║  Bytes received:    ${stats.bytesReceived.toString().padEnd(40)}║`);
  if (UPLOAD_TO_CLOUD) {
    console.log(`║  Chunks uploaded:   ${stats.chunksUploaded.toString().padEnd(40)}║`);
    console.log(`║  Upload errors:     ${stats.uploadErrors.toString().padEnd(40)}║`);
  }
  if (stats.lastChunkTime) {
    console.log(`║  Last chunk:        ${stats.lastChunkTime.toLocaleTimeString().padEnd(40)}║`);
  }
  console.log('╚════════════════════════════════════════════════════════════╝\n');
}

// Run the application
main().catch(console.error);
