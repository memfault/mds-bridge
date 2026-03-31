/**
 * @file mds_gateway_demo.c
 * @brief CES Demo version of MDS gateway with enhanced visual output
 *
 * This is a booth-friendly version with:
 * - Colorized output for better visibility
 * - Large status messages
 * - Clear progress indicators
 * - Auto-reconnect on device disconnect (fault/reset)
 *
 * Usage:
 *   ./mds_gateway_demo <vid> <pid>
 */

#include "mds_bridge/mds_protocol.h"
#include "mds_bridge/chunks_uploader.h"
#include "mds_bridge/memfault_hid.h"
#include "mds_bridge/platform_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#ifndef _WIN32
#include <unistd.h>  /* For usleep */
#endif

/* ANSI color codes */
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_BLUE    "\033[1;34m"
#define COLOR_MAGENTA "\033[1;35m"
#define COLOR_CYAN    "\033[1;36m"
#define COLOR_WHITE   "\033[1;37m"

/* Box drawing */
#define BOX_TOP    "╔════════════════════════════════════════════════════════════╗"
#define BOX_MID    "║                                                            ║"
#define BOX_BOTTOM "╚════════════════════════════════════════════════════════════╝"

static volatile sig_atomic_t keep_running = 1;

static void signal_handler(int signum) {
    (void)signum;
    keep_running = 0;
}

/* Print a large status box */
static void print_status_box(const char *status, const char *color) {
    printf("\n%s%s%s\n", color, BOX_TOP, COLOR_RESET);
    printf("%s║%s", color, COLOR_RESET);
    printf("                     %-34s", status);
    printf("%s║%s\n", color, COLOR_RESET);
    printf("%s%s%s\n\n", color, BOX_BOTTOM, COLOR_RESET);
}

/* Print section header */
static void print_header(const char *title) {
    printf("\n%s━━━ %s ━━━%s\n", COLOR_CYAN, title, COLOR_RESET);
}

/* Print success message */
static void print_success(const char *msg) {
    printf("%s✓%s %s\n", COLOR_GREEN, COLOR_RESET, msg);
}

/* Print error message */
static void print_error(const char *msg) {
    printf("%s✗%s %s\n", COLOR_RED, COLOR_RESET, msg);
}

/* Print info message */
static void print_info(const char *msg) {
    printf("%s▸%s %s\n", COLOR_BLUE, COLOR_RESET, msg);
}

/* Print warning message */
static void print_warning(const char *msg) {
    printf("%s⚠%s  %s\n", COLOR_YELLOW, COLOR_RESET, msg);
}

/* Cross-platform sleep */
static void demo_sleep_ms(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

/* Get timestamp string */
static void get_timestamp(char *buf, size_t len) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buf, len, "%H:%M:%S", tm_info);
}

/* Upload callback with visual feedback */
static int demo_upload_callback(const char *uri,
                                 const char *auth_header,
                                 const uint8_t *chunk_data,
                                 size_t chunk_len,
                                 void *user_data) {
    chunks_uploader_t *uploader = (chunks_uploader_t *)user_data;
    char timestamp[32];
    int ret;

    get_timestamp(timestamp, sizeof(timestamp));

    printf("\n%s[%s] 📤 UPLOADING CHUNK...%s\n", COLOR_YELLOW, timestamp, COLOR_RESET);
    printf("  Size: %zu bytes\n", chunk_len);

    /* Use the actual uploader */
    ret = chunks_uploader_callback(uri, auth_header, chunk_data, chunk_len, uploader);

    if (ret == 0) {
        chunks_upload_stats_t stats;
        chunks_uploader_get_stats(uploader, &stats);
        printf("%s[%s] ✓ UPLOAD SUCCESSFUL%s\n", COLOR_GREEN, timestamp, COLOR_RESET);
        printf("  Total uploaded: %s%zu chunks%s, %s%zu bytes%s\n",
               COLOR_GREEN, stats.chunks_uploaded, COLOR_RESET,
               COLOR_GREEN, stats.bytes_uploaded, COLOR_RESET);
    } else {
        printf("%s[%s] ✗ UPLOAD FAILED%s\n", COLOR_RED, timestamp, COLOR_RESET);
    }

    return ret;
}

int main(int argc, char *argv[]) {
    int ret;
    unsigned int vid, pid;
    mds_session_t *session = NULL;
    mds_device_config_t config;
    chunks_uploader_t *uploader = NULL;
    int total_packet_count = 0;
    int connection_count = 0;

    /* Parse arguments */
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <vid> <pid>\n", argv[0]);
        fprintf(stderr, "\n");
        fprintf(stderr, "Arguments:\n");
        fprintf(stderr, "  vid        Vendor ID (hex, e.g., 2fe3)\n");
        fprintf(stderr, "  pid        Product ID (hex)\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "Example:\n");
        fprintf(stderr, "  %s 2fe3 0007\n", argv[0]);
        fprintf(stderr, "\n");
        return 1;
    }

    if (sscanf(argv[1], "%x", &vid) != 1 || sscanf(argv[2], "%x", &pid) != 1) {
        fprintf(stderr, "Invalid VID/PID format. Use hex format (e.g., 1915)\n");
        return 1;
    }

    /* Set up signal handler for graceful shutdown */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Banner */
    printf("\n");
    printf("%s", COLOR_CYAN);
    printf("  ╔═══════════════════════════════════════════════════════════╗\n");
    printf("  ║                                                           ║\n");
    printf("  ║           MEMFAULT DIAGNOSTIC SERVICE GATEWAY             ║\n");
    printf("  ║                                                           ║\n");
    printf("  ║                    CES 2026 Demo                          ║\n");
    printf("  ║                                                           ║\n");
    printf("  ╚═══════════════════════════════════════════════════════════╝\n");
    printf("%s", COLOR_RESET);
    printf("\n");

    /* Create HTTP uploader (persistent across reconnections) */
    uploader = chunks_uploader_create();
    if (uploader == NULL) {
        print_error("Failed to create uploader");
        return 1;
    }

    /* Disable verbose curl output for cleaner demo */
    chunks_uploader_set_verbose(uploader, false);

    /*
     * Outer reconnection loop - handles device disconnect/reconnect
     * This allows the demo to continue after device faults/resets
     */
    while (keep_running) {
        connection_count++;

        if (connection_count == 1) {
            print_status_box("🔍 INITIALIZING...", COLOR_BLUE);
        } else {
            print_status_box("🔄 RECONNECTING...", COLOR_YELLOW);
            printf("  %sConnection attempt #%d%s\n", COLOR_WHITE, connection_count, COLOR_RESET);
        }

        /* Try to connect to device */
        print_header("DEVICE CONNECTION");
        printf("  Target Device: %s%04X:%04X%s\n", COLOR_WHITE, vid, pid, COLOR_RESET);

        /* Polling loop - wait for device to appear */
        int connect_attempts = 0;
        while (keep_running) {
            connect_attempts++;

            if (connect_attempts == 1) {
                printf("  Connecting...\n");
            } else if (connect_attempts % 10 == 0) {
                char timestamp[32];
                get_timestamp(timestamp, sizeof(timestamp));
                printf("  %s[%s] Still waiting for device... (attempt %d)%s\n",
                       COLOR_YELLOW, timestamp, connect_attempts, COLOR_RESET);
            }

            ret = mds_session_create_hid(vid, pid, NULL, &session);
            if (ret == 0) {
                break;  /* Connected! */
            }

            /* First attempt failure - show troubleshooting on initial connect only */
            if (connect_attempts == 1 && connection_count == 1) {
                print_warning("Device not found - waiting for connection...");
                printf("\n");
                print_info("Troubleshooting:");
                printf("  1. Check device is plugged in\n");
                printf("  2. Verify VID/PID are correct\n");
                printf("  3. Try running with sudo\n");
                printf("  4. Check 'lsusb' output\n");
                printf("\n");
            }

            /* Wait before retry */
            demo_sleep_ms(500);
        }

        if (!keep_running) {
            break;
        }

        print_success("Connected to device");
        print_status_box("✓ DEVICE CONNECTED", COLOR_GREEN);

        /* Read device configuration */
        print_header("DEVICE CONFIGURATION");
        ret = mds_read_device_config(session, &config);
        if (ret != 0) {
            print_error("Failed to read device configuration");
            mds_session_destroy(session);
            session = NULL;
            print_warning("Will retry connection...");
            demo_sleep_ms(1000);
            continue;  /* Try reconnecting */
        }

        printf("  %sDevice ID:%s     %s\n", COLOR_CYAN, COLOR_RESET, config.device_identifier);
        printf("  %sData URI:%s      %s\n", COLOR_CYAN, COLOR_RESET, config.data_uri);
        printf("  %sFeatures:%s      0x%08X\n", COLOR_CYAN, COLOR_RESET, config.supported_features);
        print_success("Configuration read successfully");

        /* Register upload callback */
        print_header("UPLOAD CONFIGURATION");
        ret = mds_set_upload_callback(session, demo_upload_callback, uploader);
        if (ret != 0) {
            print_error("Failed to set upload callback");
            mds_session_destroy(session);
            session = NULL;
            continue;
        }

        print_success("Upload callback registered");
        printf("  Target: %s%s%s\n", COLOR_WHITE, config.data_uri, COLOR_RESET);

        /* Enable streaming */
        print_header("STREAM CONTROL");
        ret = mds_stream_enable(session);
        if (ret != 0) {
            print_error("Failed to enable streaming");
            mds_session_destroy(session);
            session = NULL;
            continue;
        }

        print_success("Diagnostic streaming enabled");
        print_status_box("⚡ GATEWAY ACTIVE", COLOR_GREEN);

        printf("\n");
        printf("%s┌────────────────────────────────────────────────────────────┐%s\n", COLOR_WHITE, COLOR_RESET);
        printf("%s│  Waiting for diagnostic data from device...               │%s\n", COLOR_WHITE, COLOR_RESET);
        printf("%s│  Press button on nRF device to trigger fault              │%s\n", COLOR_WHITE, COLOR_RESET);
        printf("%s│  Gateway will auto-reconnect after device resets          │%s\n", COLOR_WHITE, COLOR_RESET);
        printf("%s│  Press Ctrl+C to stop                                     │%s\n", COLOR_WHITE, COLOR_RESET);
        printf("%s└────────────────────────────────────────────────────────────┘%s\n", COLOR_WHITE, COLOR_RESET);
        printf("\n");

        /* Inner packet processing loop */
        int packet_count = 0;
        int error_count = 0;
        time_t last_activity = time(NULL);
        bool device_disconnected = false;

        while (keep_running && !device_disconnected) {
            /* Process one packet with 1 second timeout */
            ret = mds_process_stream(session, &config, 1000, NULL);

            if (ret == 0) {
                packet_count++;
                total_packet_count++;
                last_activity = time(NULL);
                error_count = 0;  /* Reset error count on success */

                char timestamp[32];
                get_timestamp(timestamp, sizeof(timestamp));
                printf("%s[%s] 📦 Packet #%d received (session: %d, total: %d)%s\n",
                       COLOR_CYAN, timestamp, packet_count, packet_count, total_packet_count, COLOR_RESET);

            } else if (ret == -ETIMEDOUT || ret == MEMFAULT_HID_ERROR_TIMEOUT) {
                /* Timeout is normal - show heartbeat every 10 seconds */
                time_t now = time(NULL);
                if (now - last_activity >= 10) {
                    char timestamp[32];
                    get_timestamp(timestamp, sizeof(timestamp));
                    printf("%s[%s] 💓 Waiting...%s\n", COLOR_BLUE, timestamp, COLOR_RESET);
                    last_activity = now;
                }
                error_count = 0;  /* Timeout is not an error */
                continue;

            } else {
                /* I/O error - likely device disconnected */
                error_count++;

                if (error_count == 1) {
                    char timestamp[32];
                    get_timestamp(timestamp, sizeof(timestamp));
                    printf("%s[%s] ⚠ Stream error (ret=%d) - checking device...%s\n",
                           COLOR_YELLOW, timestamp, ret, COLOR_RESET);
                }

                /* After several errors, assume device disconnected */
                if (error_count >= 5) {
                    print_status_box("📴 DEVICE DISCONNECTED", COLOR_YELLOW);
                    printf("\n");
                    printf("  %sSession packets:%s  %d\n", COLOR_CYAN, COLOR_RESET, packet_count);
                    printf("  %sTotal packets:%s    %d\n", COLOR_CYAN, COLOR_RESET, total_packet_count);
                    printf("\n");
                    print_info("Device may have reset after fault - waiting to reconnect...");
                    device_disconnected = true;
                }

                /* Small delay between error retries */
                demo_sleep_ms(100);
            }
        }

        /* Clean up this session */
        if (session) {
            /* Try to disable streaming gracefully (may fail if disconnected) */
            mds_stream_disable(session);
            mds_session_destroy(session);
            session = NULL;
        }

        /* Wait a bit before trying to reconnect */
        if (keep_running && device_disconnected) {
            printf("\n");
            print_info("Waiting for device to reboot...");
            demo_sleep_ms(2000);  /* Give device time to reboot */
        }
    }

    /* Final shutdown */
    printf("\n");
    print_status_box("🛑 SHUTTING DOWN...", COLOR_YELLOW);

    /* Clean up session if still exists */
    if (session) {
        print_info("Disabling streaming...");
        mds_stream_disable(session);
        mds_session_destroy(session);
        print_success("Streaming disabled");
    }

    /* Print final statistics */
    if (uploader) {
        print_header("FINAL STATISTICS");
        chunks_upload_stats_t stats;
        chunks_uploader_get_stats(uploader, &stats);

        printf("  Connections:       %s%d%s\n", COLOR_GREEN, connection_count, COLOR_RESET);
        printf("  Packets received:  %s%d%s\n", COLOR_GREEN, total_packet_count, COLOR_RESET);
        printf("  Chunks uploaded:   %s%zu%s\n", COLOR_GREEN, stats.chunks_uploaded, COLOR_RESET);
        printf("  Bytes uploaded:    %s%zu%s\n", COLOR_GREEN, stats.bytes_uploaded, COLOR_RESET);

        if (stats.upload_failures > 0) {
            printf("  Upload failures:   %s%zu%s\n", COLOR_RED, stats.upload_failures, COLOR_RESET);
        }

        if (stats.last_http_status > 0) {
            const char *status_color = (stats.last_http_status >= 200 && stats.last_http_status < 300)
                                       ? COLOR_GREEN : COLOR_RED;
            printf("  Last HTTP status:  %s%ld%s\n", status_color, stats.last_http_status, COLOR_RESET);
        }

        chunks_uploader_destroy(uploader);
    }

    print_status_box("✓ GATEWAY STOPPED", COLOR_WHITE);
    printf("\n");

    return 0;
}
