/**
 * @file mds_protocol.h
 * @brief Memfault Diagnostic Service (MDS) Protocol
 *
 * This implements the Memfault Diagnostic Service protocol, adapted from
 * the BLE GATT service specification. It provides a transport-agnostic
 * interface for bridging diagnostic data from embedded devices to gateway
 * applications.
 *
 * The protocol supports multiple transport backends (HID, Serial, BLE, etc.)
 * through a pluggable backend interface.
 *
 * Protocol Overview:
 * - Feature reports provide device information and configuration
 * - Control reports enable/disable data streaming
 * - Stream reports deliver diagnostic chunk data
 */

#ifndef MDS_BRIDGE_MDS_PROTOCOL_H
#define MDS_BRIDGE_MDS_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <wchar.h>

/* Backend interface - include the backend header */
#include "mds_backend.h"

/* ============================================================================
 * Report ID Definitions
 * ========================================================================== */

/** Feature Report: Supported features bitmask (currently 0x00) */
#define MDS_REPORT_ID_SUPPORTED_FEATURES    0x01

/** Feature Report: Device identifier string */
#define MDS_REPORT_ID_DEVICE_IDENTIFIER     0x02

/** Feature Report: Data URI for uploading chunks */
#define MDS_REPORT_ID_DATA_URI              0x03

/** Feature Report: Authorization header (e.g., project key) */
#define MDS_REPORT_ID_AUTHORIZATION         0x04

/** Output Report: Stream control (enable/disable) */
#define MDS_REPORT_ID_STREAM_CONTROL        0x05

/** Input Report: Stream data packets (chunk data) */
#define MDS_REPORT_ID_STREAM_DATA           0x06

/* ============================================================================
 * Constants
 * ========================================================================== */

/** Maximum device identifier length */
#define MDS_MAX_DEVICE_ID_LEN               64

/** Maximum URI length */
#define MDS_MAX_URI_LEN                     128

/** Maximum authorization header length */
#define MDS_MAX_AUTH_LEN                    128

/** Maximum chunk data per packet (after sequence byte) */
#define MDS_MAX_CHUNK_DATA_LEN              63

/* ============================================================================
 * Stream Control Modes
 * ========================================================================== */

/** Stream control mode: Streaming disabled */
#define MDS_STREAM_MODE_DISABLED            0x00

/** Stream control mode: Streaming enabled */
#define MDS_STREAM_MODE_ENABLED             0x01

/* ============================================================================
 * Stream Data Packet Format
 * ========================================================================== */

/** Sequence counter mask (bits 0-4 of byte 0) */
#define MDS_SEQUENCE_MASK                   0x1F

/** Sequence counter max value (wraps at 31) */
#define MDS_SEQUENCE_MAX                    31

/* ============================================================================
 * Data Structures
 * ========================================================================== */

/**
 * @brief MDS device configuration
 *
 * This structure contains the device information and credentials
 * used for diagnostic data upload.
 */
typedef struct {
    /** Supported features bitmask (currently always 0x00) */
    uint32_t supported_features;

    /** Device identifier (null-terminated string) */
    char device_identifier[MDS_MAX_DEVICE_ID_LEN];

    /** Data URI for chunk upload (null-terminated string) */
    char data_uri[MDS_MAX_URI_LEN];

    /** Authorization header (null-terminated string) */
    char authorization[MDS_MAX_AUTH_LEN];
} mds_device_config_t;

/**
 * @brief MDS stream data packet
 *
 * Packet format for diagnostic chunk data.
 * Byte 0: Sequence counter (bits 0-4) + reserved (bits 5-7)
 * Byte 1+: Chunk data payload
 */
typedef struct {
    /** Sequence counter (0-31, wraps around) */
    uint8_t sequence;

    /** Chunk data payload */
    uint8_t data[MDS_MAX_CHUNK_DATA_LEN];

    /** Length of valid data in the data array */
    size_t data_len;
} mds_stream_packet_t;

/**
 * @brief Callback for uploading chunk data to the cloud
 *
 * This callback is invoked for each received chunk packet. The implementation
 * should POST the chunk data to the Memfault cloud.
 *
 * Expected HTTP request:
 * - Method: POST
 * - URL: uri parameter
 * - Headers:
 *   - Authorization header from auth_header (format: "HeaderName:HeaderValue")
 *   - Content-Type: application/octet-stream
 * - Body: chunk_data (chunk_len bytes)
 *
 * @param uri Data URI to upload to (from device config)
 * @param auth_header Authorization header (format: "HeaderName:HeaderValue")
 * @param chunk_data Chunk data bytes to upload
 * @param chunk_len Length of chunk data
 * @param user_data User-provided context pointer
 *
 * @return 0 on success, negative error code on failure
 */
typedef int (*mds_chunk_upload_callback_t)(const char *uri,
                                            const char *auth_header,
                                            const uint8_t *chunk_data,
                                            size_t chunk_len,
                                            void *user_data);

/* ============================================================================
 * MDS Session Management
 * ========================================================================== */

/**
 * @brief Opaque handle to an MDS session
 */
typedef struct mds_session mds_session_t;

/**
 * @brief Create an MDS session with a custom backend
 *
 * This is the generic session creation function that accepts any backend
 * implementation. Most users will prefer the convenience functions below.
 *
 * @param backend Backend instance (session takes ownership)
 * @param session Pointer to receive session handle
 *
 * @return 0 on success, negative error code otherwise
 *
 * @note The session takes ownership of the backend and will destroy it
 *       when mds_session_destroy() is called.
 */
int mds_session_create(mds_backend_t *backend,
                       mds_session_t **session);

/**
 * @brief Create an MDS session over HID (convenience function)
 *
 * This convenience function creates an HID backend and MDS session in one call.
 * The HID device is opened using the specified VID/PID and closed when the
 * session is destroyed.
 *
 * @param vendor_id USB Vendor ID
 * @param product_id USB Product ID
 * @param serial_number Serial number (NULL for any device)
 * @param session Pointer to receive session handle
 *
 * @return 0 on success, negative error code otherwise
 *
 * Example:
 * @code
 * mds_session_t *session;
 * int ret = mds_session_create_hid(0x1234, 0x5678, NULL, &session);
 * if (ret == 0) {
 *     // Use session...
 *     mds_session_destroy(session);  // Also closes HID device
 * }
 * @endcode
 */
int mds_session_create_hid(uint16_t vendor_id,
                            uint16_t product_id,
                            const wchar_t *serial_number,
                            mds_session_t **session);

/**
 * @brief Create an MDS session over HID using device path (convenience function)
 *
 * This convenience function creates an HID backend from a device path and
 * MDS session in one call. Useful when you've already enumerated devices
 * and want to connect to a specific one.
 *
 * @param path Device path (from device enumeration)
 * @param session Pointer to receive session handle
 *
 * @return 0 on success, negative error code otherwise
 */
int mds_session_create_hid_path(const char *path,
                                 mds_session_t **session);

/**
 * @brief Destroy an MDS session
 *
 * Disables streaming (if enabled), destroys the backend (which closes the
 * underlying transport), and frees all resources.
 *
 * @param session Session handle to destroy
 */
void mds_session_destroy(mds_session_t *session);

/* ============================================================================
 * Device Configuration
 * ========================================================================== */

/**
 * @brief Read device configuration from the device
 *
 * Reads the supported features, device identifier, data URI, and
 * authorization header from the device using feature reports.
 *
 * @param session MDS session handle
 * @param config Pointer to receive device configuration
 *
 * @return 0 on success, negative error code otherwise
 */
int mds_read_device_config(mds_session_t *session,
                           mds_device_config_t *config);

/**
 * @brief Get supported features
 *
 * @param session MDS session handle
 * @param features Pointer to receive features bitmask
 *
 * @return 0 on success, negative error code otherwise
 */
int mds_get_supported_features(mds_session_t *session,
                               uint32_t *features);

/**
 * @brief Get device identifier
 *
 * @param session MDS session handle
 * @param device_id Buffer to receive device identifier (null-terminated)
 * @param max_len Maximum length of buffer
 *
 * @return 0 on success, negative error code otherwise
 */
int mds_get_device_identifier(mds_session_t *session,
                              char *device_id,
                              size_t max_len);

/**
 * @brief Get data URI
 *
 * @param session MDS session handle
 * @param uri Buffer to receive URI (null-terminated)
 * @param max_len Maximum length of buffer
 *
 * @return 0 on success, negative error code otherwise
 */
int mds_get_data_uri(mds_session_t *session,
                     char *uri,
                     size_t max_len);

/**
 * @brief Get authorization header
 *
 * @param session MDS session handle
 * @param auth Buffer to receive authorization (null-terminated)
 * @param max_len Maximum length of buffer
 *
 * @return 0 on success, negative error code otherwise
 */
int mds_get_authorization(mds_session_t *session,
                         char *auth,
                         size_t max_len);

/* ============================================================================
 * Stream Control
 * ========================================================================== */

/**
 * @brief Enable diagnostic data streaming
 *
 * Sends a stream control output report to enable streaming.
 * After enabling, the device will begin sending chunk data via input reports.
 *
 * @param session MDS session handle
 *
 * @return 0 on success, negative error code otherwise
 */
int mds_stream_enable(mds_session_t *session);

/**
 * @brief Disable diagnostic data streaming
 *
 * Sends a stream control output report to disable streaming.
 *
 * @param session MDS session handle
 *
 * @return 0 on success, negative error code otherwise
 */
int mds_stream_disable(mds_session_t *session);

/* ============================================================================
 * Stream Data Reception
 * ========================================================================== */

/**
 * @brief Read a stream data packet
 *
 * Reads a diagnostic chunk data packet from the device.
 * This is a blocking call with the specified timeout.
 *
 * @param session MDS session handle
 * @param packet Pointer to receive stream packet
 * @param timeout_ms Timeout in milliseconds (0 = non-blocking, -1 = infinite)
 *
 * @return 0 on success, negative error code otherwise
 *         -ETIMEDOUT if no data available within timeout
 */
int mds_stream_read_packet(mds_session_t *session,
                           mds_stream_packet_t *packet,
                           int timeout_ms);

/**
 * @brief Process a stream packet from a byte buffer
 *
 * Processes a stream packet from a buffer (event-driven I/O):
 * 1. Parses the stream packet from the buffer
 * 2. Validates the sequence number (logs warning if invalid)
 * 3. Updates sequence tracking
 * 4. Triggers upload callback if registered (with mds_set_upload_callback)
 * 5. Optionally returns parsed packet to caller
 *
 * This is the primary function for event-driven/non-blocking I/O patterns.
 * Use this when you receive HID reports via callbacks or event loops.
 *
 * @param session MDS session handle
 * @param config Device configuration (contains URI and auth for upload callback)
 * @param buffer Buffer containing stream packet (sequence byte + data)
 * @param buffer_len Length of buffer
 * @param packet Optional pointer to receive parsed packet (NULL to skip)
 *
 * @return 0 on success, negative error code otherwise
 *         Returns upload callback error code if upload fails
 *
 * Example:
 * @code
 * // Register upload callback (optional)
 * mds_set_upload_callback(session, my_upload_fn, NULL);
 *
 * // In your HID input report callback
 * void on_hid_data(uint8_t *report, size_t len) {
 *     if (report[0] == MDS_REPORT_ID_STREAM_DATA) {
 *         // Process packet - upload callback triggered automatically!
 *         int ret = mds_process_stream_from_bytes(
 *             session, &config, &report[1], len - 1, NULL
 *         );
 *     }
 * }
 * @endcode
 */
int mds_process_stream_from_bytes(mds_session_t *session,
                                   const mds_device_config_t *config,
                                   const uint8_t *buffer,
                                   size_t buffer_len,
                                   mds_stream_packet_t *packet);

/* ============================================================================
 * Chunk Upload
 * ========================================================================== */

/**
 * @brief Set chunk upload callback
 *
 * Registers a callback that will be invoked to upload each received chunk.
 * This enables automatic chunk forwarding to the Memfault cloud when using
 * mds_process_stream() or mds_process_stream_from_bytes().
 *
 * @param session MDS session handle
 * @param callback Upload callback function (NULL to disable)
 * @param user_data User context pointer passed to callback
 *
 * @return 0 on success, negative error code otherwise
 */
int mds_set_upload_callback(mds_session_t *session,
                             mds_chunk_upload_callback_t callback,
                             void *user_data);

/**
 * @brief Process a stream packet by reading from the device
 *
 * Reads a stream packet from the device and processes it:
 * 1. Reads packet from device (blocking with timeout)
 * 2. Validates the sequence number (logs warning if invalid)
 * 3. Updates sequence tracking
 * 4. Triggers upload callback if registered (with mds_set_upload_callback)
 * 5. Optionally returns parsed packet to caller
 *
 * This is the primary function for blocking I/O patterns. Call this in a loop
 * after enabling streaming.
 *
 * @param session MDS session handle
 * @param config Device configuration (contains URI and auth for upload callback)
 * @param timeout_ms Timeout in milliseconds for reading packets
 * @param packet Optional pointer to receive parsed packet (NULL to skip)
 *
 * @return 0 on success, negative error code otherwise
 *         -ETIMEDOUT if no data available within timeout
 *         Returns upload callback error code if upload fails
 *
 * Example:
 * @code
 * // Register upload callback (optional)
 * mds_set_upload_callback(session, my_upload_fn, NULL);
 *
 * // Process packets in a loop
 * while (running) {
 *     int ret = mds_process_stream(session, &config, 1000, NULL);
 *     if (ret == 0) {
 *         // Packet received and upload callback was triggered
 *     }
 * }
 * @endcode
 */
int mds_process_stream(mds_session_t *session,
                       const mds_device_config_t *config,
                       int timeout_ms,
                       mds_stream_packet_t *packet);


#ifdef __cplusplus
}
#endif

#endif /* MDS_BRIDGE_MDS_PROTOCOL_H */
