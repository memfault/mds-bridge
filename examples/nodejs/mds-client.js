/**
 * MDS Client - Using Custom Node HID Backend
 *
 * This demonstrates the pluggable backend architecture by creating
 * a custom backend that bridges node-hid with the MDS C library. This approach:
 *
 * 1. Creates a NodeHIDBackend that implements the backend interface
 * 2. Registers it with the C library via FFI callbacks
 * 3. Uses high-level MDS API functions instead of buffer-based parsing
 * 4. Demonstrates transport-agnostic protocol design
 */

import {
  lib,
  ref,
  mds_session_ptr,
  mds_stream_packet_t,
  mds_device_config_t,
  MDS_REPORT_ID,
  MDS_MAX_DEVICE_ID_LEN,
  MDS_MAX_URI_LEN,
  MDS_MAX_AUTH_LEN,
} from './bindings.js';
import { NodeHIDBackend } from './node-hid-backend.js';

/**
 * MDS Client - Using pluggable backend architecture
 */
export class MDSClient {
  /**
   * @param {Object} hidDevice - node-hid device instance
   */
  constructor(hidDevice) {
    this.device = hidDevice;
    this.backend = null;
    this.session = null;
    this.config = null;
    this.streaming = false;
    this.onChunk = null;
  }

  /**
   * Initialize the MDS session with custom backend
   */
  async initialize() {
    console.log('[MDSClient] Creating Node HID backend...');

    // Create custom backend using node-hid
    this.backend = new NodeHIDBackend(this.device);

    // Create MDS session with our custom backend
    const sessionPtr = ref.alloc(mds_session_ptr);
    const result = lib.mds_session_create(this.backend.getBackendRef(), sessionPtr);

    if (result < 0) {
      throw new Error(`Failed to create MDS session: ${result}`);
    }

    this.session = sessionPtr.deref();
    console.log('[MDSClient] MDS session created successfully');

    // Read device configuration using high-level API
    await this.readDeviceConfig();
  }

  /**
   * Read device configuration using high-level C API
   *
   * This demonstrates the benefit of the backend architecture:
   * - No manual buffer parsing needed
   * - No manual HID feature report reads
   * - The C library handles all the protocol details
   */
  async readDeviceConfig() {
    console.log('[MDSClient] Reading device configuration...');

    // Use high-level API - the C library will call our backend for HID operations
    const config = new mds_device_config_t();
    const result = lib.mds_read_device_config(this.session, config.ref());

    if (result < 0) {
      throw new Error(`Failed to read device config: ${result}`);
    }

    // Extract configuration from struct
    this.config = {
      supportedFeatures: config.supported_features,
      deviceIdentifier: ref.readCString(config.device_identifier.buffer, 0),
      dataUri: ref.readCString(config.data_uri.buffer, 0),
      authorization: ref.readCString(config.authorization.buffer, 0),
    };

    console.log('[MDSClient] Device configuration:');
    console.log(`  Device ID: ${this.config.deviceIdentifier}`);
    console.log(`  Data URI: ${this.config.dataUri}`);
    console.log(`  Auth: ${this.config.authorization}`);
    console.log(`  Features: 0x${this.config.supportedFeatures.toString(16)}`);

    return this.config;
  }

  /**
   * Enable diagnostic data streaming using high-level API
   */
  async enableStreaming() {
    console.log('[MDSClient] Enabling streaming...');

    // Use high-level API - no need to manually build control packets
    const result = lib.mds_stream_enable(this.session);

    if (result < 0) {
      throw new Error(`Failed to enable streaming: ${result}`);
    }

    this.streaming = true;
    console.log('[MDSClient] Streaming enabled');
  }

  /**
   * Disable diagnostic data streaming using high-level API
   */
  async disableStreaming() {
    console.log('[MDSClient] Disabling streaming...');

    const result = lib.mds_stream_disable(this.session);

    if (result < 0) {
      throw new Error(`Failed to disable streaming: ${result}`);
    }

    this.streaming = false;
    console.log('[MDSClient] Streaming disabled');
  }

  /**
   * Process transport data packet
   *
   * Call this for every packet received from your transport layer
   * (HID report, Serial frame, BLE notification, etc.).
   *
   * For multiplexed transports (HID, Serial):
   *   - Expects data[0] to be channel ID (report ID / framing byte)
   *   - data[1:] is payload
   *
   * For pre-demultiplexed transports (BLE):
   *   - Use processStreamData() instead
   *
   * @param {Buffer} data - Raw packet data from transport (including channel ID)
   * @returns {boolean} True if this was MDS stream data (handled internally),
   *                    False if this should be handled by application
   */
  process(data) {
    if (!data || data.length < 1) {
      return false;
    }

    const channelId = data[0];

    // Only process MDS stream data channel (0x06)
    if (channelId !== 0x06) { // MDS_REPORT_ID_STREAM_DATA
      return false;
    }

    // Process the stream data payload
    const payload = data.slice(1);
    this._processStreamPayload(payload);
    return true;
  }

  /**
   * Process MDS stream data payload directly
   *
   * Use this for transports that pre-demultiplex channels (e.g., BLE GATT
   * characteristics). When you receive data from the MDS stream characteristic,
   * call this method directly.
   *
   * @param {Buffer} payload - MDS stream packet payload (without channel ID prefix)
   */
  processStreamData(payload) {
    this._processStreamPayload(payload);
  }

  /**
   * Internal method to process MDS stream packet payload
   * @private
   * @param {Buffer} payload - Stream packet payload (sequence + data)
   */
  _processStreamPayload(payload) {
    if (!payload || payload.length === 0) {
      return;
    }

    // Parse the stream packet using buffer-based API
    // (We can't use mds_stream_read_packet here because it would block)
    const packet = new mds_stream_packet_t();
    const result = lib.mds_parse_stream_packet(payload, payload.length, packet.ref());

    if (result < 0) {
      console.error(`[MDSClient] Failed to parse stream packet: ${result}`);
      return;
    }

    // Validate sequence using C library
    const lastSeq = lib.mds_get_last_sequence(this.session);
    if (lastSeq !== 31) { // MDS_SEQUENCE_MAX
      const isValid = lib.mds_validate_sequence(lastSeq, packet.sequence);
      if (!isValid) {
        console.warn(`[MDSClient] Sequence error: expected ${(lastSeq + 1) & 0x1F}, got ${packet.sequence}`);
      }
    }

    // Update last sequence
    lib.mds_update_last_sequence(this.session, packet.sequence);

    // Extract chunk data
    const chunkData = Buffer.from(packet.data.buffer.slice(0, packet.data_len));

    console.log(`[MDSClient] Received chunk: seq=${packet.sequence}, len=${packet.data_len}`);

    // Trigger callback if registered
    if (this.onChunk) {
      this.onChunk({
        sequence: packet.sequence,
        data: chunkData,
        length: packet.data_len,
      });
    }
  }

  /**
   * Upload chunk data to Memfault cloud
   *
   * @param {Buffer} chunkData - Chunk data to upload
   */
  async uploadChunk(chunkData) {
    if (!this.config) {
      throw new Error('Device config not loaded');
    }

    // Parse authorization header (format: "HeaderName:HeaderValue" or "HeaderName: HeaderValue")
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

      console.log(`[MDSClient] Chunk uploaded: ${chunkData.length} bytes`);
      return true;
    } catch (error) {
      console.error('[MDSClient] Failed to upload chunk:', error);
      return false;
    }
  }

  /**
   * Set callback for received chunks
   *
   * @param {Function} callback - Callback function(packet) where packet has {sequence, data, length}
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

    if (this.session) {
      lib.mds_session_destroy(this.session);
      this.session = null;
    }

    if (this.backend) {
      this.backend.destroy();
      this.backend = null;
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
}
