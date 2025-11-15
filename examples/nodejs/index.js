/**
 * Memfault HID + MDS Example Application
 *
 * This example demonstrates using Node-HID for both:
 * 1. Custom application-specific HID communication
 * 2. Memfault Diagnostic Service (MDS) streaming in parallel
 *
 * The MDS protocol logic is handled by the C library via FFI,
 * while Node-HID manages all actual HID I/O.
 */

import HID from 'node-hid';
import { MDSClient } from './mds-client.js';
import { MDS_REPORT_ID } from './bindings.js';

// Configuration - Update these values for your device
const CONFIG = {
  vendorId: 0x1234,    // Replace with your device's VID
  productId: 0x5678,   // Replace with your device's PID
  uploadChunks: true,  // Set to true to upload chunks to Memfault cloud
};

/**
 * Custom application logic
 * This represents your existing HID application code
 */
class CustomHIDApp {
  constructor(device) {
    this.device = device;
  }

  /**
   * Example: Send a custom command to the device
   * This could be any application-specific HID communication
   */
  sendCustomCommand(reportId, data) {
    console.log(`[CustomApp] Sending custom command (Report ID: 0x${reportId.toString(16)})`);
    const report = [reportId, ...data];
    this.device.write(report);
  }

  /**
   * Example: Process custom HID reports
   * Returns true if the report was handled, false otherwise
   */
  processCustomReport(data) {
    const reportId = data[0];

    // Ignore MDS reports - let MDSClient handle those
    if (reportId === MDS_REPORT_ID.STREAM_DATA) {
      return false;
    }

    // Handle your custom reports here
    // For this example, we'll just log them
    console.log(`[CustomApp] Received report 0x${reportId.toString(16)}: ${data.slice(1, 8).join(' ')}...`);
    return true;
  }

  /**
   * Example: Periodic application task
   */
  async doPeriodicTask() {
    // Your application logic here
    // For example: reading sensors, updating UI, etc.
    console.log('[CustomApp] Performing periodic task...');
  }
}

/**
 * Main application - coordinates custom app and MDS
 */
class Application {
  constructor() {
    this.device = null;
    this.mdsClient = null;
    this.customApp = null;
    this.running = false;
    this.stats = {
      chunksReceived: 0,
      chunksUploaded: 0,
      customReports: 0,
    };
  }

  /**
   * Find and open the HID device
   */
  async openDevice() {
    console.log(`Looking for HID device (VID: 0x${CONFIG.vendorId.toString(16)}, PID: 0x${CONFIG.productId.toString(16)})...`);

    const devices = HID.devices();
    const deviceInfo = devices.find(
      (d) => d.vendorId === CONFIG.vendorId && d.productId === CONFIG.productId
    );

    if (!deviceInfo) {
      console.log('\nAvailable devices:');
      devices.forEach((d) => {
        console.log(`  - VID: 0x${d.vendorId.toString(16).padStart(4, '0')}, ` +
                    `PID: 0x${d.productId.toString(16).padStart(4, '0')}, ` +
                    `${d.manufacturer} ${d.product}`);
      });
      throw new Error('Device not found. Update CONFIG with your device VID/PID.');
    }

    console.log(`Found device: ${deviceInfo.manufacturer} ${deviceInfo.product}`);
    console.log(`  Path: ${deviceInfo.path}`);

    this.device = new HID.HID(deviceInfo.path);
    console.log('Device opened successfully\n');
  }

  /**
   * Initialize MDS and custom app
   */
  async initialize() {
    // Create custom app instance
    this.customApp = new CustomHIDApp(this.device);

    // Create and initialize MDS client
    this.mdsClient = new MDSClient(this.device);
    await this.mdsClient.initialize();

    const config = this.mdsClient.getConfig();
    console.log('MDS Device Configuration:');
    console.log(`  Device ID: ${config.deviceIdentifier}`);
    console.log(`  Data URI: ${config.dataUri}`);
    console.log(`  Auth: ${config.authorization.substring(0, 30)}...`);
    console.log(`  Features: 0x${config.supportedFeatures.toString(16)}\n`);

    // Set up chunk callback
    this.mdsClient.setChunkCallback(async (packet) => {
      this.stats.chunksReceived++;
      console.log(`[MDS] Chunk received: seq=${packet.sequence}, len=${packet.length} bytes`);

      // Upload chunk if enabled
      if (CONFIG.uploadChunks) {
        const success = await this.mdsClient.uploadChunk(packet.data);
        if (success) {
          this.stats.chunksUploaded++;
          console.log(`[MDS] Chunk uploaded successfully`);
        }
      }
    });

    // Set up HID data handler
    this.device.on('data', (data) => this.handleHIDData(Buffer.from(data)));
    this.device.on('error', (error) => {
      console.error('[HID] Error:', error);
      this.running = false;
    });
  }

  /**
   * Handle incoming HID data
   * Routes data to either custom app or MDS client
   */
  handleHIDData(data) {
    // Try custom app first
    const handled = this.customApp.processCustomReport(data);
    if (handled) {
      this.stats.customReports++;
      return;
    }

    // Let MDS client handle it
    this.mdsClient.processHIDData(data);
  }

  /**
   * Start the application
   */
  async start() {
    console.log('Starting application...\n');

    // Enable MDS streaming
    await this.mdsClient.enableStreaming();
    console.log('[MDS] Streaming enabled\n');

    this.running = true;

    // Example: Periodic custom app task
    const customTaskInterval = setInterval(() => {
      if (!this.running) {
        clearInterval(customTaskInterval);
        return;
      }
      this.customApp.doPeriodicTask();
    }, 10000); // Every 10 seconds

    // Example: Send a custom command periodically
    // Uncomment this if you want to test custom HID commands
    // const customCommandInterval = setInterval(() => {
    //   if (!this.running) {
    //     clearInterval(customCommandInterval);
    //     return;
    //   }
    //   this.customApp.sendCustomCommand(0x10, [0x01, 0x02, 0x03]);
    // }, 5000);

    // Stats reporting
    const statsInterval = setInterval(() => {
      if (!this.running) {
        clearInterval(statsInterval);
        return;
      }
      console.log(`\n[Stats] Chunks: ${this.stats.chunksReceived} received, ` +
                  `${this.stats.chunksUploaded} uploaded, ` +
                  `Custom reports: ${this.stats.customReports}\n`);
    }, 30000); // Every 30 seconds

    console.log('Application running. Press Ctrl+C to exit.\n');
  }

  /**
   * Stop the application
   */
  async stop() {
    console.log('\nStopping application...');
    this.running = false;

    if (this.mdsClient) {
      await this.mdsClient.disableStreaming();
      this.mdsClient.destroy();
    }

    if (this.device) {
      this.device.close();
    }

    console.log('Application stopped.');
    console.log(`\nFinal stats:`);
    console.log(`  Chunks received: ${this.stats.chunksReceived}`);
    console.log(`  Chunks uploaded: ${this.stats.chunksUploaded}`);
    console.log(`  Custom reports: ${this.stats.customReports}`);
  }
}

/**
 * Main entry point
 */
async function main() {
  console.log('='.repeat(60));
  console.log('Memfault HID + MDS Example Application');
  console.log('='.repeat(60));
  console.log();

  const app = new Application();

  // Handle graceful shutdown
  process.on('SIGINT', async () => {
    await app.stop();
    process.exit(0);
  });

  process.on('SIGTERM', async () => {
    await app.stop();
    process.exit(0);
  });

  try {
    await app.openDevice();
    await app.initialize();
    await app.start();
  } catch (error) {
    console.error('Error:', error.message);
    await app.stop();
    process.exit(1);
  }
}

// Run the application
main().catch((error) => {
  console.error('Fatal error:', error);
  process.exit(1);
});
