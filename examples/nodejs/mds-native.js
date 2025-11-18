/**
 * Native MDS Client - Using N-API addon with direct protocol functions
 *
 * This provides a clean JavaScript wrapper that handles HID I/O
 * and uses the native addon for MDS protocol parsing
 */

import { createRequire } from 'module';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const require = createRequire(import.meta.url);
const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

// Load the native addon - check if we're in build/ or root directory
let native;
try {
  // First try relative to current file (works from both root and build/)
  native = require(join(__dirname, 'Release/mds_bridge_native.node'));
} catch (e) {
  // Fall back to build/Release (when running from root)
  native = require('./build/Release/mds_bridge_native.node');
}

/**
 * MDS Client using native protocol functions with C session management
 */
export class MDSClient {
  constructor(hidDevice) {
    this.device = hidDevice;
    this.config = null;
    this.streaming = false;
    this.onChunk = null;
    this.session = null; // C session handle
  }

  async initialize() {
    // Create native session with backend
    this.session = native.createSession();

    // Read device config
    await this.readDeviceConfig();
  }

  async readDeviceConfig() {
    // Read all config feature reports and parse them directly
    const features = this._readFeatureReport(native.MDS_REPORT_ID.SUPPORTED_FEATURES, 4);
    const deviceId = this._readFeatureReport(native.MDS_REPORT_ID.DEVICE_IDENTIFIER, 64);
    const dataUri = this._readFeatureReport(native.MDS_REPORT_ID.DATA_URI, 128);
    const auth = this._readFeatureReport(native.MDS_REPORT_ID.AUTHORIZATION, 128);

    // Parse config (simple format: 4-byte features + null-terminated strings)
    this.config = {
      supportedFeatures: features.readUInt32LE(0),
      deviceIdentifier: this._readNullTermString(deviceId),
      dataUri: this._readNullTermString(dataUri),
      authorization: this._readNullTermString(auth),
    };

    return this.config;
  }

  async enableStreaming() {
    // Stream enable packet: [0x01] (1 byte)
    const packet = Buffer.from([0x01]);

    // Send as feature report
    this._writeFeatureReport(native.MDS_REPORT_ID.STREAM_CONTROL, packet);

    this.streaming = true;
    this.lastSequence = null;
  }

  async disableStreaming() {
    if (!this.streaming) {
      return; // Already disabled
    }

    try {
      // Stream disable packet: [0x00] (1 byte)
      const packet = Buffer.from([0x00]);

      // Send as feature report
      this._writeFeatureReport(native.MDS_REPORT_ID.STREAM_CONTROL, packet);

      this.streaming = false;
    } catch (error) {
      // Device might already be disconnected during shutdown
      this.streaming = false;
    }
  }

  /**
   * Process transport data packet
   */
  process(data) {
    if (!data || data.length < 1) {
      return false;
    }

    const channelId = data[0];

    // Only process MDS stream data channel (0x06)
    if (channelId !== native.MDS_REPORT_ID.STREAM_DATA) {
      return false;
    }

    // Process the stream data payload
    const payload = data.slice(1);
    this._processStreamPayload(payload);
    return true;
  }

  /**
   * Process MDS stream data payload directly
   */
  processStreamData(payload) {
    this._processStreamPayload(payload);
  }

  /**
   * Internal method to process MDS stream packet payload
   * Uses the native C API to handle parsing, sequence validation, and tracking
   */
  _processStreamPayload(payload) {
    if (!payload || payload.length === 0) {
      return;
    }

    try {
      // Process the stream packet using the native C API
      // Pass session, config, and buffer
      const packet = native.processStreamFromBytes(this.session, this.config, payload);

      // Trigger callback if registered
      if (this.onChunk) {
        this.onChunk(packet);
      }
    } catch (error) {
      console.error(`Failed to process stream packet:`, error);
    }
  }

  /**
   * Upload chunk data to Memfault cloud
   */
  async uploadChunk(chunkData) {
    if (!this.config) {
      throw new Error('Device config not loaded');
    }

    // Parse authorization header
    const authParts = this.config.authorization.split(':', 2);
    const authHeader = authParts.length === 2 ? { [authParts[0].trim()]: authParts[1].trim() } : {};

    try {
      const response = await fetch(this.config.dataUri, {
        method: 'POST',
        headers: {
          ...authHeader,
          'Content-Type': 'application/octet-stream',
        },
        body: chunkData,
      });

      if (!response.ok) {
        throw new Error(`Upload failed: ${response.status} ${response.statusText}`);
      }

      console.log(`Chunk uploaded: ${chunkData.length} bytes`);
      return true;
    } catch (error) {
      console.error('Failed to upload chunk:', error);
      return false;
    }
  }

  /**
   * Set callback for received chunks
   */
  setChunkCallback(callback) {
    this.onChunk = callback;
  }

  /**
   * Clean up resources
   */
  destroy() {
    if (this.streaming) {
      this.disableStreaming().catch(console.error);
    }
  }

  /**
   * Get device configuration
   */
  getConfig() {
    return this.config;
  }

  /**
   * Check if streaming is enabled
   */
  isStreaming() {
    return this.streaming;
  }

  // Helper: Read HID feature report
  _readFeatureReport(reportId, maxLength) {
    // getFeatureReport(reportId, bufferSize)
    // Returns array including the report ID as first byte
    const data = this.device.getFeatureReport(reportId, maxLength + 1);

    if (!data || data.length < 2) {
      throw new Error(`Failed to read feature report 0x${reportId.toString(16)}`);
    }

    // data[0] is the report ID, skip it
    return Buffer.from(data.slice(1));
  }

  // Helper: Write HID feature report
  _writeFeatureReport(reportId, payload) {
    // sendFeatureReport expects [report_id, ...data]
    const report = Buffer.concat([Buffer.from([reportId]), payload]);
    this.device.sendFeatureReport(Array.from(report));
  }

  // Helper: Read null-terminated string from buffer
  _readNullTermString(buffer) {
    const nullIndex = buffer.indexOf(0);
    if (nullIndex === -1) {
      return buffer.toString('utf8');
    }
    return buffer.toString('utf8', 0, nullIndex);
  }
}

export const MDS_REPORT_ID = native.MDS_REPORT_ID;
