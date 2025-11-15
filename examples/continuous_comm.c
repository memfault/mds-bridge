/**
 * @file continuous_comm.c
 * @brief Example: Continuous communication with a HID device
 */

#include "memfault_hid/memfault_hid.h"
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>

static volatile bool g_running = true;

static void signal_handler(int sig) {
    (void)sig;
    g_running = false;
}

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

    printf("Memfault HID Library - Continuous Communication Example\n");
    printf("Version: %s\n", memfault_hid_version_string());
    printf("Press Ctrl+C to stop\n\n");

    /* Set up signal handler */
    signal(SIGINT, signal_handler);

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

    /* Main communication loop */
    uint32_t packet_count = 0;
    uint8_t recv_data[64];
    uint8_t recv_report_id;

    printf("Starting communication loop...\n");
    while (g_running) {
        /* Try to read a report (non-blocking) */
        ret = memfault_hid_read_report(device, &recv_report_id, recv_data,
                                        sizeof(recv_data), 100);

        if (ret > 0) {
            packet_count++;
            printf("Packet #%u: Received %d bytes (Report ID: 0x%02X)\n",
                   packet_count, ret, recv_report_id);

            /* Echo the data back */
            ret = memfault_hid_write_report(device, recv_report_id,
                                             recv_data, ret, 100);
            if (ret < 0) {
                fprintf(stderr, "Failed to write report: %s\n",
                        memfault_hid_error_string(ret));
            }
        } else if (ret == MEMFAULT_HID_ERROR_TIMEOUT) {
            /* No data available, continue */
        } else if (ret < 0) {
            fprintf(stderr, "Read error: %s\n", memfault_hid_error_string(ret));
            break;
        }
    }

    /* Cleanup */
    printf("\nShutting down...\n");
    printf("Total packets processed: %u\n", packet_count);

    memfault_hid_close(device);
    memfault_hid_exit();

    return 0;
}
