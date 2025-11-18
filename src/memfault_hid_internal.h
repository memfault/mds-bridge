/**
 * @file memfault_hid_internal.h
 * @brief Internal HID API - extends public API with device I/O operations
 *
 * This header includes the public API and adds internal functions for:
 * - Device opening/closing
 * - Report filtering
 * - Low-level report I/O (read/write/get/set)
 * - Device configuration
 *
 * Applications should generally use the public API in memfault_hid.h or
 * the high-level MDS API in mds_protocol.h instead.
 */

#ifndef MEMFAULT_HID_INTERNAL_H
#define MEMFAULT_HID_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Include public API */
#include "mds_bridge/memfault_hid.h"

/* Version information */
#define MEMFAULT_HID_VERSION_MAJOR 1
#define MEMFAULT_HID_VERSION_MINOR 0
#define MEMFAULT_HID_VERSION_PATCH 0

/* Maximum report size (increased to support MDS feature reports up to 128 bytes) */
#define MEMFAULT_HID_MAX_REPORT_SIZE 256

/**
 * @brief Report types (HID report types)
 */
typedef enum {
    MEMFAULT_HID_REPORT_TYPE_INPUT = 0x01,
    MEMFAULT_HID_REPORT_TYPE_OUTPUT = 0x02,
    MEMFAULT_HID_REPORT_TYPE_FEATURE = 0x03
} memfault_hid_report_type_t;

/**
 * @brief Opaque handle to a HID device
 */
#ifndef MEMFAULT_HID_DEVICE_T_DEFINED
#define MEMFAULT_HID_DEVICE_T_DEFINED
typedef struct memfault_hid_device memfault_hid_device_t;
#endif

/**
 * @brief Report filter configuration
 *
 * This structure allows the library to filter reports by Report ID,
 * enabling coexistence with other HID functionality in the application.
 */
typedef struct {
    uint8_t *report_ids;             /* Array of Report IDs to filter */
    size_t num_report_ids;           /* Number of Report IDs in the array */
    bool filter_enabled;             /* Enable/disable filtering */
} memfault_hid_report_filter_t;

/* ============================================================================
 * Device Management
 * ========================================================================== */

/**
 * @brief Open a HID device by path
 *
 * @param path Device path from device info structure
 * @param device Pointer to receive device handle
 *
 * @return MEMFAULT_HID_SUCCESS on success, error code otherwise
 */
int memfault_hid_open_path(const char *path, memfault_hid_device_t **device);

/**
 * @brief Open a HID device by VID/PID
 *
 * Opens the first device matching the specified VID/PID.
 *
 * @param vendor_id USB Vendor ID
 * @param product_id USB Product ID
 * @param serial_number Serial number (NULL for any)
 * @param device Pointer to receive device handle
 *
 * @return MEMFAULT_HID_SUCCESS on success, error code otherwise
 */
int memfault_hid_open(uint16_t vendor_id,
                      uint16_t product_id,
                      const wchar_t *serial_number,
                      memfault_hid_device_t **device);

/**
 * @brief Close a HID device
 *
 * @param device Device handle to close
 */
void memfault_hid_close(memfault_hid_device_t *device);

/**
 * @brief Get device information
 *
 * @param device Device handle
 * @param info Pointer to receive device information
 *
 * @return MEMFAULT_HID_SUCCESS on success, error code otherwise
 */
int memfault_hid_get_device_info(memfault_hid_device_t *device,
                                  memfault_hid_device_info_t *info);

/* ============================================================================
 * Report Filtering
 * ========================================================================== */

/**
 * @brief Configure report filtering for a device
 *
 * This allows the library to only handle specific Report IDs, enabling
 * other parts of the application to handle different Report IDs.
 *
 * @param device Device handle
 * @param filter Filter configuration
 *
 * @return MEMFAULT_HID_SUCCESS on success, error code otherwise
 */
int memfault_hid_set_report_filter(memfault_hid_device_t *device,
                                    const memfault_hid_report_filter_t *filter);

/**
 * @brief Get current report filter configuration
 *
 * @param device Device handle
 * @param filter Pointer to receive filter configuration
 *
 * @return MEMFAULT_HID_SUCCESS on success, error code otherwise
 */
int memfault_hid_get_report_filter(memfault_hid_device_t *device,
                                    memfault_hid_report_filter_t *filter);

/* ============================================================================
 * Report Communication
 * ========================================================================== */

/**
 * @brief Write an output or feature report to the device
 *
 * @param device Device handle
 * @param report_id Report ID (or 0 if device doesn't use Report IDs)
 * @param data Report data (excluding Report ID)
 * @param length Length of data
 * @param timeout_ms Timeout in milliseconds (0 for non-blocking, -1 for infinite)
 *
 * @return Number of bytes written on success, negative error code otherwise
 */
int memfault_hid_write_report(memfault_hid_device_t *device,
                               uint8_t report_id,
                               const uint8_t *data,
                               size_t length,
                               int timeout_ms);

/**
 * @brief Read an input report from the device
 *
 * @param device Device handle
 * @param report_id Pointer to receive Report ID (may be NULL if not needed)
 * @param data Buffer to receive report data
 * @param length Length of buffer
 * @param timeout_ms Timeout in milliseconds (0 for non-blocking, -1 for infinite)
 *
 * @return Number of bytes read on success, negative error code otherwise
 */
int memfault_hid_read_report(memfault_hid_device_t *device,
                              uint8_t *report_id,
                              uint8_t *data,
                              size_t length,
                              int timeout_ms);

/**
 * @brief Get a feature report from the device
 *
 * @param device Device handle
 * @param report_id Report ID to get
 * @param data Buffer to receive report data
 * @param length Length of buffer
 *
 * @return Number of bytes received on success, negative error code otherwise
 */
int memfault_hid_get_feature_report(memfault_hid_device_t *device,
                                     uint8_t report_id,
                                     uint8_t *data,
                                     size_t length);

/**
 * @brief Send an output report to the device (via SET_REPORT)
 *
 * This sends an OUTPUT report using a SET_REPORT control request.
 * This is different from hid_write() which sends data to the OUT endpoint.
 * Use this for HID devices that expect OUTPUT reports via control transfers.
 *
 * @param device Device handle
 * @param report_id Report ID
 * @param data Report data (without Report ID prefix)
 * @param length Length of data
 *
 * @return Number of bytes sent on success, negative error code otherwise
 */
int memfault_hid_send_output_report(memfault_hid_device_t *device,
                                     uint8_t report_id,
                                     const uint8_t *data,
                                     size_t length);

/**
 * @brief Send a feature report to the device
 *
 * @param device Device handle
 * @param report_id Report ID
 * @param data Report data
 * @param length Length of data
 *
 * @return Number of bytes sent on success, negative error code otherwise
 */
int memfault_hid_set_feature_report(memfault_hid_device_t *device,
                                     uint8_t report_id,
                                     const uint8_t *data,
                                     size_t length);

/* ============================================================================
 * Utility Functions
 * ========================================================================== */

/**
 * @brief Get library version string
 *
 * @return Version string (e.g., "1.0.0")
 */
const char *memfault_hid_version_string(void);

/**
 * @brief Set non-blocking mode for device reads
 *
 * @param device Device handle
 * @param nonblock True for non-blocking, false for blocking
 *
 * @return MEMFAULT_HID_SUCCESS on success, error code otherwise
 */
int memfault_hid_set_nonblocking(memfault_hid_device_t *device, bool nonblock);

#ifdef __cplusplus
}
#endif

#endif /* MEMFAULT_HID_INTERNAL_H */
