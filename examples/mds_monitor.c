/**
 * @file mds_monitor.c
 * @brief MDS Stream Monitor - displays Memfault diagnostic stream data
 *
 * This example monitors and displays MDS stream packets from a device.
 * Useful for debugging MDS streaming and inspecting diagnostic data.
 *
 * Usage:
 *   ./mds_monitor                    # Interactive device selection
 *   ./mds_monitor <vid> <pid>        # Specify VID/PID in hex
 *   ./mds_monitor 0x2fe3 0x0007
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <time.h>

#include "mds_bridge/memfault_hid.h"  /* For device enumeration */
#include "mds_bridge/mds_protocol.h"
#include "mds_bridge/platform_compat.h"

/* Global flag for clean shutdown */
static volatile bool g_running = true;

/* Statistics */
typedef struct {
    uint32_t packets_received;
    uint32_t bytes_received;
    uint32_t sequence_errors;
    uint32_t timeouts;
    time_t start_time;
} monitor_stats_t;

/**
 * Signal handler for graceful shutdown
 */
static void signal_handler(int signum) {
    (void)signum;
    g_running = false;
    printf("\nShutting down...\n");
}

/**
 * Print a hex dump of data
 */
static void print_hex_dump(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
        if ((i + 1) % 16 == 0) {
            printf("\n        ");
        } else if ((i + 1) % 8 == 0) {
            printf(" ");
        }
    }
    if (len % 16 != 0) {
        printf("\n");
    }
}

/**
 * Print MDS stream packet
 */
static void print_stream_packet(const mds_stream_packet_t *packet, monitor_stats_t *stats) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    printf("[%ld.%03ld] MDS Stream Packet\n",
           (long)ts.tv_sec, (long)(ts.tv_nsec / 1000000));
    printf("  Sequence:   %u (0x%02X)\n", packet->sequence, packet->sequence);
    printf("  Data Len:   %zu bytes\n", packet->data_len);
    printf("  Data:       ");
    print_hex_dump(packet->data, packet->data_len);
    printf("\n");

    stats->packets_received++;
    stats->bytes_received += packet->data_len;
}

/**
 * List all available HID devices
 */
static void list_devices(void) {
    printf("Enumerating HID devices...\n\n");

    memfault_hid_device_info_t *devices = NULL;
    size_t num_devices = 0;
    int ret = memfault_hid_enumerate(0, 0, &devices, &num_devices);

    if (ret != MEMFAULT_HID_SUCCESS) {
        fprintf(stderr, "Error: Failed to enumerate devices: %s\n",
                memfault_hid_error_string(ret));
        return;
    }

    if (num_devices == 0) {
        printf("No HID devices found.\n");
        return;
    }

    printf("Found %zu HID device(s):\n\n", num_devices);

    for (size_t i = 0; i < num_devices; i++) {
        printf("%zu. VID: 0x%04X, PID: 0x%04X\n", i + 1,
               devices[i].vendor_id, devices[i].product_id);
        printf("   Manufacturer: %ls\n", devices[i].manufacturer);
        printf("   Product:      %ls\n", devices[i].product);
        printf("   Path:         %s\n", devices[i].path);
        printf("\n");
    }

    memfault_hid_free_device_list(devices);
}

/**
 * Prompt user to select a device
 */
static char* select_device_interactive(void) {
    memfault_hid_device_info_t *devices = NULL;
    size_t num_devices = 0;
    int ret = memfault_hid_enumerate(0, 0, &devices, &num_devices);

    if (ret != MEMFAULT_HID_SUCCESS || num_devices == 0) {
        return NULL;
    }

    printf("Select device (1-%zu) or 0 to exit: ", num_devices);
    fflush(stdout);

    int selection = 0;
    if (scanf("%d", &selection) != 1 || selection < 1 || selection > (int)num_devices) {
        memfault_hid_free_device_list(devices);
        return NULL;
    }

    char *path = strdup(devices[selection - 1].path);
    memfault_hid_free_device_list(devices);

    return path;
}

/**
 * Find device by VID/PID
 */
static char* find_device_by_vid_pid(uint16_t vid, uint16_t pid) {
    memfault_hid_device_info_t *devices = NULL;
    size_t num_devices = 0;
    int ret = memfault_hid_enumerate(vid, pid, &devices, &num_devices);

    if (ret != MEMFAULT_HID_SUCCESS || num_devices == 0) {
        fprintf(stderr, "Error: No device found with VID:0x%04X PID:0x%04X\n", vid, pid);
        return NULL;
    }

    printf("Found device: %ls %ls\n",
           devices[0].manufacturer, devices[0].product);

    char *path = strdup(devices[0].path);
    memfault_hid_free_device_list(devices);

    return path;
}

/**
 * Monitor MDS stream
 */
static int monitor_mds_stream(const char *path) {
    mds_session_t *session = NULL;
    monitor_stats_t stats = {0};
    int ret;

    printf("\nOpening device and creating MDS session: %s\n", path);

    /* Create MDS session (opens HID device internally) */
    ret = mds_session_create_hid_path(path, &session);
    if (ret < 0) {
        fprintf(stderr, "Error: Failed to create MDS session: %d\n", ret);
        return ret;
    }

    printf("MDS session created successfully!\n\n");

    /* Read device configuration */
    mds_device_config_t config = {0};
    ret = mds_read_device_config(session, &config);
    if (ret < 0) {
        fprintf(stderr, "Warning: Failed to read device config: %d\n", ret);
        fprintf(stderr, "         Continuing without device configuration.\n\n");
    } else {
        printf("MDS Device Configuration:\n");
        printf("  Device ID:   %s\n", config.device_identifier);
        printf("  Data URI:    %s\n", config.data_uri);
        printf("  Auth:        %s\n", config.authorization[0] ? config.authorization : "none");
        printf("  Features:    0x%08X\n\n", config.supported_features);
    }

    /* Enable streaming */
    printf("Enabling MDS streaming...\n");
    ret = mds_stream_enable(session);
    if (ret < 0) {
        fprintf(stderr, "Error: Failed to enable streaming: %d\n", ret);
        mds_session_destroy(session);  /* Also closes HID device */
        return ret;
    }

    printf("Streaming enabled!\n");
    printf("Monitoring MDS stream... (Press Ctrl+C to stop)\n");
    printf("============================================================\n\n");

    stats.start_time = time(NULL);
    uint8_t last_seq = MDS_SEQUENCE_MAX;
    bool first_packet = true;

    while (g_running) {
        mds_stream_packet_t packet;

        /* Read packet with 100ms timeout */
        ret = mds_stream_read_packet(session, &packet, 100);

        if (ret == 0) {
            /* Got a packet */
            print_stream_packet(&packet, &stats);

            /* Validate sequence */
            if (!first_packet) {
                uint8_t expected = (last_seq + 1) & MDS_SEQUENCE_MASK;
                if (packet.sequence != expected) {
                    printf("  WARNING: Sequence error! Expected %u, got %u\n\n",
                           expected, packet.sequence);
                    stats.sequence_errors++;
                }
            }
            last_seq = packet.sequence;
            first_packet = false;

        } else if (ret == -ETIMEDOUT) {
            /* Timeout - normal, device might not have data to send */
            stats.timeouts++;
        } else {
            /* Other errors - warn but continue */
            if (stats.timeouts == 0) {
                /* Only print error on first occurrence */
                fprintf(stderr, "Warning: Error reading packet: %d (device might not be sending data yet)\n", ret);
            }
            stats.timeouts++;
        }

        /* Print stats every 100 timeouts (~10 seconds) */
        if (stats.timeouts > 0 && stats.timeouts % 100 == 0) {
            time_t elapsed = time(NULL) - stats.start_time;
            printf("[Stats] Packets: %u, Bytes: %u, Seq errors: %u, Elapsed: %ld sec\n\n",
                   stats.packets_received, stats.bytes_received,
                   stats.sequence_errors, (long)elapsed);
        }
    }

    /* Print final statistics */
    time_t elapsed = time(NULL) - stats.start_time;
    printf("\n");
    printf("Final Statistics:\n");
    printf("  Packets received:  %u\n", stats.packets_received);
    printf("  Bytes received:    %u\n", stats.bytes_received);
    printf("  Sequence errors:   %u\n", stats.sequence_errors);
    printf("  Elapsed time:      %ld seconds\n", (long)elapsed);
    if (elapsed > 0) {
        printf("  Throughput:        %.2f bytes/sec\n",
               (double)stats.bytes_received / elapsed);
    }
    printf("\n");

    /* Cleanup */
    printf("Disabling streaming...\n");
    mds_stream_disable(session);
    mds_session_destroy(session);  /* Also closes HID device */

    return 0;
}

/**
 * Print usage information
 */
static void print_usage(const char *program) {
    printf("Usage:\n");
    printf("  %s                    # Interactive mode - select from available devices\n", program);
    printf("  %s <vid> <pid>        # Monitor specific device by VID/PID (hex)\n", program);
    printf("\n");
    printf("Examples:\n");
    printf("  %s                    # Show all devices and select one\n", program);
    printf("  %s 0x2fe3 0x0007      # Monitor device with VID:0x2fe3 PID:0x0007\n", program);
    printf("\n");
}

int main(int argc, char *argv[]) {
    int ret;
    char *device_path = NULL;

    printf("============================================================\n");
    printf("Memfault MDS Stream Monitor\n");
    printf("============================================================\n\n");

    /* Install signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Initialize HID library */
    ret = memfault_hid_init();
    if (ret != MEMFAULT_HID_SUCCESS) {
        fprintf(stderr, "Error: Failed to initialize HID library: %s\n",
                memfault_hid_error_string(ret));
        return 1;
    }

    /* Parse command line arguments */
    if (argc == 1) {
        /* Interactive mode */
        list_devices();
        device_path = select_device_interactive();
        if (!device_path) {
            fprintf(stderr, "No device selected.\n");
            memfault_hid_exit();
            return 1;
        }
    } else if (argc == 3) {
        /* VID/PID specified */
        uint16_t vid = (uint16_t)strtol(argv[1], NULL, 16);
        uint16_t pid = (uint16_t)strtol(argv[2], NULL, 16);

        device_path = find_device_by_vid_pid(vid, pid);
        if (!device_path) {
            memfault_hid_exit();
            return 1;
        }
    } else {
        print_usage(argv[0]);
        memfault_hid_exit();
        return 1;
    }

    /* Monitor MDS stream */
    ret = monitor_mds_stream(device_path);

    /* Cleanup */
    free(device_path);
    memfault_hid_exit();

    printf("Goodbye!\n");

    return (ret < 0) ? 1 : 0;
}
