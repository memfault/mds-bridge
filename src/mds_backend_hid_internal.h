/**
 * @file mds_backend_hid_internal.h
 * @brief Internal header for HID backend implementation
 *
 * This header is for internal use only and should not be installed as a public API.
 */

#ifndef MDS_BACKEND_HID_INTERNAL_H
#define MDS_BACKEND_HID_INTERNAL_H

#include "mds_bridge/mds_backend.h"
#include <stdint.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create HID backend from VID/PID
 *
 * @param vendor_id USB Vendor ID
 * @param product_id USB Product ID
 * @param serial_number Serial number (NULL for any device)
 * @param backend Pointer to receive backend instance
 *
 * @return 0 on success, negative error code otherwise
 */
int mds_backend_hid_create(uint16_t vendor_id, uint16_t product_id,
                            const wchar_t *serial_number,
                            mds_backend_t **backend);

/**
 * Create HID backend from device path
 *
 * @param path Device path (from device enumeration)
 * @param backend Pointer to receive backend instance
 *
 * @return 0 on success, negative error code otherwise
 */
int mds_backend_hid_create_path(const char *path, mds_backend_t **backend);

#ifdef __cplusplus
}
#endif

#endif /* MDS_BACKEND_HID_INTERNAL_H */
