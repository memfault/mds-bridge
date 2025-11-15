/**
 * MDS Client - High-level wrapper for Memfault Diagnostic Service
 *
 * This class provides a JavaScript API for the MDS protocol, using Node-HID
 * for HID I/O and the C library (via FFI) for protocol logic.
 */

import {
  lib,
  ref,
  voidPtr,
  mds_session_ptr,
  mds_stream_packet_t,
  MDS_REPORT_ID,
  MDS_STREAM_MODE,
  MDS_MAX_DEVICE_ID_LEN,
  MDS_MAX_URI_LEN,
  MDS_MAX_AUTH_LEN,
  MDS_MAX_CHUNK_DATA_LEN,
  MDS_SEQUENCE_MAX,
} from './bindings.js';

/**
 * MDS Client for managing diagnostic data streaming
 */
export class MDSClient {
  /**
   * @param {Object} hidDevice - node-hid device instance
   */
  constructor(hidDevice) {
    this.device = hidDevice;
    this.session = null;
    this.config = null;
    this.streaming = false;
    this.onChunk = null;
  }

  /**
   * Initialize the MDS session
   */
  async initialize() {
    // Create MDS session (without HID device handle - we manage that in JS)
    const sessionPtr = ref.alloc(mds_session_ptr);
    const result = lib.mds_session_create(voidPtr.NULL, sessionPtr);

    if (result < 0) {
      throw new Error(`Failed to create MDS session: ${result}`);
    }

    this.session = sessionPtr.deref();

    // Read device configuration
    await this.readDeviceConfig();
  }

  /**
   * Read device configuration from the device
   */
  async readDeviceConfig() {
    const config = {};

    // Read supported features (Feature Report 0x01)
    const featuresData = this.device.getFeatureReport(MDS_REPORT_ID.SUPPORTED_FEATURES, 4);
    const featuresBuffer = Buffer.from(featuresData.slice(1)); // Skip report ID
    const featuresPtr = ref.alloc(ref.types.uint32);
    lib.mds_parse_supported_features(featuresBuffer, featuresBuffer.length, featuresPtr);
    config.supportedFeatures = featuresPtr.deref();

    // Read device identifier (Feature Report 0x02)
    const deviceIdData = this.device.getFeatureReport(MDS_REPORT_ID.DEVICE_IDENTIFIER, MDS_MAX_DEVICE_ID_LEN);
    const deviceIdBuffer = Buffer.from(deviceIdData.slice(1)); // Skip report ID
    const deviceIdStr = Buffer.alloc(MDS_MAX_DEVICE_ID_LEN);
    lib.mds_parse_device_identifier(deviceIdBuffer, deviceIdBuffer.length, deviceIdStr, MDS_MAX_DEVICE_ID_LEN);
    config.deviceIdentifier = deviceIdStr.toString('utf8').replace(/\0.*$/g, '');

    // Read data URI (Feature Report 0x03)
    const uriData = this.device.getFeatureReport(MDS_REPORT_ID.DATA_URI, MDS_MAX_URI_LEN);
    const uriBuffer = Buffer.from(uriData.slice(1)); // Skip report ID
    const uriStr = Buffer.alloc(MDS_MAX_URI_LEN);
    lib.mds_parse_data_uri(uriBuffer, uriBuffer.length, uriStr, MDS_MAX_URI_LEN);
    config.dataUri = uriStr.toString('utf8').replace(/\0.*$/g, '');

    // Read authorization (Feature Report 0x04)
    const authData = this.device.getFeatureReport(MDS_REPORT_ID.AUTHORIZATION, MDS_MAX_AUTH_LEN);
    const authBuffer = Buffer.from(authData.slice(1)); // Skip report ID
    const authStr = Buffer.alloc(MDS_MAX_AUTH_LEN);
    lib.mds_parse_authorization(authBuffer, authBuffer.length, authStr, MDS_MAX_AUTH_LEN);
    config.authorization = authStr.toString('utf8').replace(/\0.*$/g, '');

    this.config = config;
    return config;
  }

  /**
   * Enable diagnostic data streaming
   */
  async enableStreaming() {
    const buffer = Buffer.alloc(1);
    const bytesWritten = lib.mds_build_stream_control(true, buffer, buffer.length);

    if (bytesWritten < 0) {
      throw new Error(`Failed to build stream control: ${bytesWritten}`);
    }

    // Send output report (Report ID 0x05)
    const report = Buffer.concat([Buffer.from([MDS_REPORT_ID.STREAM_CONTROL]), buffer]);
    this.device.write(Array.from(report));

    this.streaming = true;
  }

  /**
   * Disable diagnostic data streaming
   */
  async disableStreaming() {
    const buffer = Buffer.alloc(1);
    const bytesWritten = lib.mds_build_stream_control(false, buffer, buffer.length);

    if (bytesWritten < 0) {
      throw new Error(`Failed to build stream control: ${bytesWritten}`);
    }

    // Send output report (Report ID 0x05)
    const report = Buffer.concat([Buffer.from([MDS_REPORT_ID.STREAM_CONTROL]), buffer]);
    this.device.write(Array.from(report));

    this.streaming = false;
  }

  /**
   * Process incoming HID data
   * Call this when you receive data from the HID device
   *
   * @param {Buffer} data - Raw HID input report data (including report ID)
   */
  processHIDData(data) {
    const reportId = data[0];

    // Only process MDS stream data reports
    if (reportId !== MDS_REPORT_ID.STREAM_DATA) {
      return null;
    }

    // Parse the stream packet
    const packetBuffer = data.slice(1); // Remove report ID
    const packet = new mds_stream_packet_t();
    const result = lib.mds_parse_stream_packet(packetBuffer, packetBuffer.length, packet.ref());

    if (result < 0) {
      console.error(`Failed to parse stream packet: ${result}`);
      return null;
    }

    // Validate sequence number
    const lastSeq = lib.mds_get_last_sequence(this.session);
    if (lastSeq !== MDS_SEQUENCE_MAX) {
      const isValid = lib.mds_validate_sequence(lastSeq, packet.sequence);
      if (!isValid) {
        console.warn(`Sequence validation failed: expected ${(lastSeq + 1) & 0x1F}, got ${packet.sequence}`);
      }
    }

    // Update last sequence
    lib.mds_update_last_sequence(this.session, packet.sequence);

    // Extract chunk data
    const chunkData = Buffer.from(packet.data.buffer.slice(0, packet.data_len));

    // Trigger callback if registered
    if (this.onChunk) {
      this.onChunk({
        sequence: packet.sequence,
        data: chunkData,
        length: packet.data_len,
      });
    }

    return {
      sequence: packet.sequence,
      data: chunkData,
      length: packet.data_len,
    };
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

    // Parse authorization header (format: "HeaderName:HeaderValue")
    const authParts = this.config.authorization.split(':', 2);
    const authHeader = authParts.length === 2 ? { [authParts[0]]: authParts[1] } : {};

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

      return true;
    } catch (error) {
      console.error('Failed to upload chunk:', error);
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
    if (this.session) {
      lib.mds_session_destroy(this.session);
      this.session = null;
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
