/**
 * @file memfault_hid.h
 * @brief Public HID device enumeration and initialization API
 *
 * This header provides the public API for HID device enumeration and library initialization.
 * Use this header when you need to list/enumerate HID devices or manage library lifecycle.
 *
 * For the MDS protocol API, use mds_protocol.h instead.
 */

#ifndef MDS_BRIDGE_MEMFAULT_HID_H
#define MDS_BRIDGE_MEMFAULT_HID_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief Error codes
 */
typedef enum {
    MEMFAULT_HID_SUCCESS = 0,
    MEMFAULT_HID_ERROR_INVALID_PARAM = -1,
    MEMFAULT_HID_ERROR_NOT_FOUND = -2,
    MEMFAULT_HID_ERROR_NO_DEVICE = -3,
    MEMFAULT_HID_ERROR_ACCESS_DENIED = -4,
    MEMFAULT_HID_ERROR_IO = -5,
    MEMFAULT_HID_ERROR_TIMEOUT = -6,
    MEMFAULT_HID_ERROR_BUSY = -7,
    MEMFAULT_HID_ERROR_NO_MEM = -8,
    MEMFAULT_HID_ERROR_NOT_SUPPORTED = -9,
    MEMFAULT_HID_ERROR_ALREADY_OPEN = -10,
    MEMFAULT_HID_ERROR_INVALID_REPORT_TYPE = -11,
    MEMFAULT_HID_ERROR_UNKNOWN = -99
} memfault_hid_error_t;

/**
 * @brief Device information structure
 */
typedef struct {
    char path[256];                  /* Platform-specific device path */
    uint16_t vendor_id;              /* USB Vendor ID */
    uint16_t product_id;             /* USB Product ID */
    wchar_t serial_number[128];      /* Serial number (wide string) */
    uint16_t release_number;         /* Device release number */
    wchar_t manufacturer[128];       /* Manufacturer string (wide string) */
    wchar_t product[128];            /* Product string (wide string) */
    uint16_t usage_page;             /* HID usage page */
    uint16_t usage;                  /* HID usage */
    int interface_number;            /* USB interface number */
} memfault_hid_device_info_t;

/* ============================================================================
 * Library Initialization
 * ========================================================================== */

/**
 * @brief Initialize the HID library
 *
 * This function must be called before device enumeration or opening devices.
 * It is safe to call multiple times (idempotent).
 *
 * Note: When using the high-level MDS API (mds_session_create_hid()), this
 * is called automatically by the backend.
 *
 * @return MEMFAULT_HID_SUCCESS on success, error code otherwise
 */
int memfault_hid_init(void);

/**
 * @brief Cleanup and shutdown the HID library
 *
 * This function should be called when done using the library.
 *
 * @return MEMFAULT_HID_SUCCESS on success, error code otherwise
 */
int memfault_hid_exit(void);

/* ============================================================================
 * Device Enumeration
 * ========================================================================== */

/**
 * @brief Enumerate all HID devices matching the specified VID/PID
 *
 * @param vendor_id USB Vendor ID (0x0000 for all vendors)
 * @param product_id USB Product ID (0x0000 for all products)
 * @param devices Pointer to receive array of device info structures
 * @param num_devices Pointer to receive the number of devices found
 *
 * @return MEMFAULT_HID_SUCCESS on success, error code otherwise
 *
 * @note The caller must free the returned device list using memfault_hid_free_device_list()
 * @note memfault_hid_init() must be called before calling this function
 */
int memfault_hid_enumerate(uint16_t vendor_id,
                           uint16_t product_id,
                           memfault_hid_device_info_t **devices,
                           size_t *num_devices);

/**
 * @brief Free device list returned by memfault_hid_enumerate()
 *
 * @param devices Device list to free
 */
void memfault_hid_free_device_list(memfault_hid_device_info_t *devices);

/* ============================================================================
 * Error Handling
 * ========================================================================== */

/**
 * @brief Get error string for error code
 *
 * @param error Error code (from memfault_hid_error_t)
 *
 * @return Human-readable error string
 */
const char *memfault_hid_error_string(int error);

#ifdef __cplusplus
}
#endif

#endif /* MDS_BRIDGE_MEMFAULT_HID_H */
