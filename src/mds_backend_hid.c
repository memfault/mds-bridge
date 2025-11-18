/**
 * @file mds_backend_hid.c
 * @brief HID backend implementation for MDS protocol
 *
 * This backend implements the MDS transport layer using USB HID.
 */

#include "memfault_hid/mds_backend.h"
#include "memfault_hid_internal.h"
#include <stdlib.h>
#include <string.h>

/**
 * HID backend internal state
 */
typedef struct {
    mds_backend_t base;               /**< Base backend structure */
    memfault_hid_device_t *device;    /**< HID device handle */
} mds_hid_backend_t;

/**
 * Read operation for HID backend
 *
 * Routes to appropriate HID function based on report ID:
 * - Reports 0x01-0x05: Feature reports (GET_FEATURE)
 * - Report 0x06: Input report (READ)
 */
static int hid_backend_read(void *impl_data, uint8_t report_id,
                             uint8_t *buffer, size_t length, int timeout_ms) {
    mds_hid_backend_t *hid_backend = (mds_hid_backend_t *)impl_data;

    /* Report 0x06 is an input report (stream data) */
    if (report_id == 0x06) {
        uint8_t read_report_id = 0;
        int result = memfault_hid_read_report(hid_backend->device, &read_report_id,
                                               buffer, length, timeout_ms);

        /* Verify we got the expected report ID */
        if (result > 0 && read_report_id != report_id) {
            return MEMFAULT_HID_ERROR_IO;
        }

        return result;
    }

    /* All other reports (0x01-0x05) are feature reports */
    return memfault_hid_get_feature_report(hid_backend->device, report_id,
                                            buffer, length);
}

/**
 * Write operation for HID backend
 *
 * Currently only used for stream control (report 0x05)
 * Uses SET_FEATURE for all writes.
 */
static int hid_backend_write(void *impl_data, uint8_t report_id,
                              const uint8_t *buffer, size_t length) {
    mds_hid_backend_t *hid_backend = (mds_hid_backend_t *)impl_data;

    return memfault_hid_set_feature_report(hid_backend->device, report_id,
                                            buffer, length);
}

/**
 * Destroy HID backend
 *
 * Closes the HID device and frees the backend structure.
 */
static void hid_backend_destroy(void *impl_data) {
    mds_hid_backend_t *hid_backend = (mds_hid_backend_t *)impl_data;

    if (hid_backend) {
        if (hid_backend->device) {
            memfault_hid_close(hid_backend->device);
        }
        free(hid_backend);
    }
}

/**
 * HID backend operations vtable
 */
static const mds_backend_ops_t hid_backend_ops = {
    .read = hid_backend_read,
    .write = hid_backend_write,
    .destroy = hid_backend_destroy,
};

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
                            mds_backend_t **backend) {
    if (!backend) {
        return MEMFAULT_HID_ERROR_INVALID_PARAM;
    }

    /* Initialize HID library if not already done */
    int result = memfault_hid_init();
    if (result < 0) {
        return result;
    }

    /* Allocate backend structure */
    mds_hid_backend_t *hid_backend = calloc(1, sizeof(mds_hid_backend_t));
    if (!hid_backend) {
        return MEMFAULT_HID_ERROR_NO_MEM;
    }

    /* Initialize base backend */
    hid_backend->base.ops = &hid_backend_ops;
    hid_backend->base.impl_data = hid_backend;

    /* Open HID device */
    result = memfault_hid_open(vendor_id, product_id, serial_number,
                               &hid_backend->device);
    if (result < 0) {
        free(hid_backend);
        return result;
    }

    *backend = &hid_backend->base;
    return 0;
}

/**
 * Create HID backend from device path
 *
 * @param path Device path (from device enumeration)
 * @param backend Pointer to receive backend instance
 *
 * @return 0 on success, negative error code otherwise
 */
int mds_backend_hid_create_path(const char *path, mds_backend_t **backend) {
    if (!path || !backend) {
        return MEMFAULT_HID_ERROR_INVALID_PARAM;
    }

    /* Initialize HID library if not already done */
    int result = memfault_hid_init();
    if (result < 0) {
        return result;
    }

    /* Allocate backend structure */
    mds_hid_backend_t *hid_backend = calloc(1, sizeof(mds_hid_backend_t));
    if (!hid_backend) {
        return MEMFAULT_HID_ERROR_NO_MEM;
    }

    /* Initialize base backend */
    hid_backend->base.ops = &hid_backend_ops;
    hid_backend->base.impl_data = hid_backend;

    /* Open HID device by path */
    result = memfault_hid_open_path(path, &hid_backend->device);
    if (result < 0) {
        free(hid_backend);
        return result;
    }

    *backend = &hid_backend->base;
    return 0;
}
