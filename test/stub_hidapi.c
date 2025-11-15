/**
 * @file stub_hidapi.c
 * @brief Stub implementations of HID functions for upload-only tests
 *
 * These stubs are needed when linking mds_protocol.c for upload tests
 * without needing the full HID implementation.
 */

#include "memfault_hid/memfault_hid.h"
#include <errno.h>

/* Stub implementations - these should never be called in upload-only tests */

int memfault_hid_write_report(memfault_hid_device_t *device, uint8_t report_id,
                               const uint8_t *data, size_t length, int timeout_ms) {
    (void)device;
    (void)report_id;
    (void)data;
    (void)length;
    (void)timeout_ms;
    return -ENOSYS;  /* Not implemented */
}

int memfault_hid_read_report(memfault_hid_device_t *device, uint8_t *report_id,
                              uint8_t *data, size_t max_length, int timeout_ms) {
    (void)device;
    (void)report_id;
    (void)data;
    (void)max_length;
    (void)timeout_ms;
    return -ENOSYS;  /* Not implemented */
}

int memfault_hid_get_feature_report(memfault_hid_device_t *device, uint8_t report_id,
                                     uint8_t *data, size_t max_length) {
    (void)device;
    (void)report_id;
    (void)data;
    (void)max_length;
    return -ENOSYS;  /* Not implemented */
}

int memfault_hid_set_feature_report(memfault_hid_device_t *device, uint8_t report_id,
                                     const uint8_t *data, size_t length) {
    (void)device;
    (void)report_id;
    (void)data;
    (void)length;
    return -ENOSYS;  /* Not implemented */
}
