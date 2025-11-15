/**
 * @file enumerate_devices.c
 * @brief Example: Enumerate and list HID devices
 */

#include "memfault_hid/memfault_hid.h"
#include <stdio.h>
#include <wchar.h>

int main(int argc, char *argv[]) {
    int ret;
    uint16_t vid = 0x0000;  /* 0 = all vendors */
    uint16_t pid = 0x0000;  /* 0 = all products */

    /* Parse command line arguments */
    if (argc >= 3) {
        sscanf(argv[1], "%hx", &vid);
        sscanf(argv[2], "%hx", &pid);
    }

    printf("Memfault HID Library - Device Enumeration Example\n");
    printf("Version: %s\n\n", memfault_hid_version_string());

    /* Initialize library */
    ret = memfault_hid_init();
    if (ret != MEMFAULT_HID_SUCCESS) {
        fprintf(stderr, "Failed to initialize HID library: %s\n",
                memfault_hid_error_string(ret));
        return 1;
    }

    /* Enumerate devices */
    memfault_hid_device_info_t *devices = NULL;
    size_t num_devices = 0;

    printf("Enumerating devices (VID: 0x%04X, PID: 0x%04X)...\n", vid, pid);
    ret = memfault_hid_enumerate(vid, pid, &devices, &num_devices);
    if (ret != MEMFAULT_HID_SUCCESS) {
        fprintf(stderr, "Failed to enumerate devices: %s\n",
                memfault_hid_error_string(ret));
        memfault_hid_exit();
        return 1;
    }

    printf("Found %zu device(s)\n\n", num_devices);

    /* Print device information */
    for (size_t i = 0; i < num_devices; i++) {
        printf("Device %zu:\n", i + 1);
        printf("  Path:             %s\n", devices[i].path);
        printf("  VID:PID:          0x%04X:0x%04X\n",
               devices[i].vendor_id, devices[i].product_id);
        printf("  Serial Number:    %ls\n", devices[i].serial_number);
        printf("  Manufacturer:     %ls\n", devices[i].manufacturer);
        printf("  Product:          %ls\n", devices[i].product);
        printf("  Release Number:   0x%04X\n", devices[i].release_number);
        printf("  Usage Page:       0x%04X\n", devices[i].usage_page);
        printf("  Usage:            0x%04X\n", devices[i].usage);
        printf("  Interface Number: %d\n", devices[i].interface_number);
        printf("\n");
    }

    /* Cleanup */
    memfault_hid_free_device_list(devices);
    memfault_hid_exit();

    return 0;
}
