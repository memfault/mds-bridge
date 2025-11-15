/**
 * @file send_receive.c
 * @brief Example: Send and receive HID reports
 */

#include "memfault_hid/memfault_hid.h"
#include <stdio.h>
#include <string.h>

#define REPORT_ID 0x01
#define TIMEOUT_MS 1000

int main(int argc, char *argv[]) {
    int ret;
    uint16_t vid, pid;

    if (argc < 3) {
        printf("Usage: %s <VID> <PID>\n", argv[0]);
        printf("Example: %s 0x1234 0x5678\n", argv[0]);
        return 1;
    }

    sscanf(argv[1], "%hx", &vid);
    sscanf(argv[2], "%hx", &pid);

    printf("Memfault HID Library - Send/Receive Example\n");
    printf("Version: %s\n\n", memfault_hid_version_string());

    /* Initialize library */
    ret = memfault_hid_init();
    if (ret != MEMFAULT_HID_SUCCESS) {
        fprintf(stderr, "Failed to initialize: %s\n",
                memfault_hid_error_string(ret));
        return 1;
    }

    /* Open device */
    memfault_hid_device_t *device = NULL;
    printf("Opening device (VID: 0x%04X, PID: 0x%04X)...\n", vid, pid);
    ret = memfault_hid_open(vid, pid, NULL, &device);
    if (ret != MEMFAULT_HID_SUCCESS) {
        fprintf(stderr, "Failed to open device: %s\n",
                memfault_hid_error_string(ret));
        memfault_hid_exit();
        return 1;
    }

    printf("Device opened successfully\n\n");

    /* Configure report filter (example: only handle Report IDs 0x01-0x0F) */
    uint8_t report_ids[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                            0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
    memfault_hid_report_filter_t filter = {
        .report_ids = report_ids,
        .num_report_ids = sizeof(report_ids),
        .filter_enabled = true
    };

    ret = memfault_hid_set_report_filter(device, &filter);
    if (ret != MEMFAULT_HID_SUCCESS) {
        fprintf(stderr, "Failed to set report filter: %s\n",
                memfault_hid_error_string(ret));
    } else {
        printf("Report filter configured (Report IDs 0x01-0x0F)\n\n");
    }

    /* Send a report */
    uint8_t send_data[32];
    memset(send_data, 0, sizeof(send_data));
    strcpy((char *)send_data, "Hello from memfault_hid!");

    printf("Sending report (ID: 0x%02X, %zu bytes)...\n", REPORT_ID, sizeof(send_data));
    ret = memfault_hid_write_report(device, REPORT_ID, send_data,
                                     sizeof(send_data), TIMEOUT_MS);
    if (ret < 0) {
        fprintf(stderr, "Failed to write report: %s\n",
                memfault_hid_error_string(ret));
    } else {
        printf("Sent %d bytes\n\n", ret);
    }

    /* Receive a report */
    uint8_t recv_data[64];
    uint8_t recv_report_id = 0;

    printf("Waiting for report (timeout: %d ms)...\n", TIMEOUT_MS);
    ret = memfault_hid_read_report(device, &recv_report_id, recv_data,
                                    sizeof(recv_data), TIMEOUT_MS);
    if (ret < 0) {
        if (ret == MEMFAULT_HID_ERROR_TIMEOUT) {
            printf("Timeout waiting for report\n");
        } else {
            fprintf(stderr, "Failed to read report: %s\n",
                    memfault_hid_error_string(ret));
        }
    } else {
        printf("Received %d bytes (Report ID: 0x%02X)\n", ret, recv_report_id);
        printf("Data: ");
        for (int i = 0; i < ret && i < 32; i++) {
            printf("%02X ", recv_data[i]);
        }
        printf("\n");
    }

    /* Cleanup */
    memfault_hid_close(device);
    memfault_hid_exit();

    printf("\nDone\n");
    return 0;
}
