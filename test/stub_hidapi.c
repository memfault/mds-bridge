/**
 * @file stub_hidapi.c
 * @brief Stub implementations of HID functions for upload-only tests
 *
 * These stubs are needed when linking mds_protocol.c for upload tests
 * without needing the full HID implementation.
 */

#include "../src/memfault_hid_internal.h"
#include <errno.h>

/* Stub implementations - these should never be called in upload-only tests */

int memfault_hid_init(void) {
    return MEMFAULT_HID_SUCCESS;  /* No-op for tests */
}

int memfault_hid_exit(void) {
    return MEMFAULT_HID_SUCCESS;  /* No-op for tests */
}

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

int memfault_hid_send_output_report(memfault_hid_device_t *device, uint8_t report_id,
                                     const uint8_t *data, size_t length) {
    (void)device;
    (void)report_id;
    (void)data;
    (void)length;
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

int memfault_hid_open(uint16_t vendor_id, uint16_t product_id,
                       const wchar_t *serial_number,
                       memfault_hid_device_t **device) {
    (void)vendor_id;
    (void)product_id;
    (void)serial_number;
    (void)device;
    return -ENOSYS;  /* Not implemented */
}

int memfault_hid_open_path(const char *path, memfault_hid_device_t **device) {
    (void)path;
    (void)device;
    return -ENOSYS;  /* Not implemented */
}

void memfault_hid_close(memfault_hid_device_t *device) {
    (void)device;
    /* Not implemented */
}
