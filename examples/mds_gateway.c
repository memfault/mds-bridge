/**
 * @file mds_gateway.c
 * @brief Example MDS gateway that forwards diagnostic chunks to Memfault cloud
 *
 * This example demonstrates the full workflow:
 * 1. Connect to HID device
 * 2. Read MDS device configuration
 * 3. Enable diagnostic data streaming
 * 4. Receive and upload chunks to Memfault cloud
 *
 * Usage:
 *   ./mds_gateway <vid> <pid>
 *
 * Example:
 *   ./mds_gateway 1234 5678
 */

#include "memfault_hid/memfault_hid.h"
#include "memfault_hid/mds_protocol.h"

#ifdef MEMFAULT_HID_MDS_UPLOAD_ENABLED
#include "memfault_hid/mds_upload.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

static volatile sig_atomic_t keep_running = 1;

static void signal_handler(int signum) {
    (void)signum;
    keep_running = 0;
}

/* Custom upload callback example */
static int custom_upload_callback(const char *uri,
                                   const char *auth_header,
                                   const uint8_t *chunk_data,
                                   size_t chunk_len,
                                   void *user_data) {
    int *chunk_count = (int *)user_data;
    (*chunk_count)++;

    printf("\n[Custom Uploader] Chunk #%d\n", *chunk_count);
    printf("  URI: %s\n", uri);
    printf("  Auth: %s\n", auth_header);
    printf("  Size: %zu bytes\n", chunk_len);
    printf("  Data: ");
    for (size_t i = 0; i < (chunk_len < 16 ? chunk_len : 16); i++) {
        printf("%02X ", chunk_data[i]);
    }
    if (chunk_len > 16) {
        printf("... (%zu bytes total)", chunk_len);
    }
    printf("\n");

    /* In a real application, you would POST this data using your HTTP client */
    /* For now, we just log it */
    return 0;
}

int main(int argc, char *argv[]) {
    int ret;
    unsigned int vid, pid;
    memfault_hid_device_t *device = NULL;
    mds_session_t *session = NULL;
    mds_device_config_t config;

#ifdef MEMFAULT_HID_MDS_UPLOAD_ENABLED
    mds_uploader_t *uploader = NULL;
    bool use_builtin_uploader = true;
#else
    bool use_builtin_uploader = false;
#endif

    int custom_chunk_count = 0;

    /* Parse arguments */
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <vid> <pid> [--custom-upload]\n", argv[0]);
        fprintf(stderr, "\n");
        fprintf(stderr, "Arguments:\n");
        fprintf(stderr, "  vid              Vendor ID (hex, e.g., 1234)\n");
        fprintf(stderr, "  pid              Product ID (hex, e.g., 5678)\n");
        fprintf(stderr, "  --custom-upload  Use custom upload callback instead of built-in\n");
        fprintf(stderr, "\n");
#ifdef MEMFAULT_HID_MDS_UPLOAD_ENABLED
        fprintf(stderr, "Built-in HTTP uploader: ENABLED (libcurl)\n");
#else
        fprintf(stderr, "Built-in HTTP uploader: DISABLED\n");
#endif
        return 1;
    }

    if (sscanf(argv[1], "%x", &vid) != 1 || sscanf(argv[2], "%x", &pid) != 1) {
        fprintf(stderr, "Invalid VID/PID format. Use hex format (e.g., 1234)\n");
        return 1;
    }

    /* Check for custom upload flag */
    if (argc >= 4 && strcmp(argv[3], "--custom-upload") == 0) {
        use_builtin_uploader = false;
        printf("Using custom upload callback\n");
    }

    /* Set up signal handler for graceful shutdown */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("=========================================\n");
    printf("Memfault MDS Gateway\n");
    printf("=========================================\n\n");

    /* Initialize library */
    printf("Initializing HID library...\n");
    ret = memfault_hid_init();
    if (ret != MEMFAULT_HID_SUCCESS) {
        fprintf(stderr, "Failed to initialize: %s\n", memfault_hid_error_string(ret));
        return 1;
    }

    /* Open device */
    printf("Opening device %04X:%04X...\n", vid, pid);
    ret = memfault_hid_open(vid, pid, NULL, &device);
    if (ret != MEMFAULT_HID_SUCCESS) {
        fprintf(stderr, "Failed to open device: %s\n", memfault_hid_error_string(ret));
        memfault_hid_exit();
        return 1;
    }
    printf("Device opened successfully\n\n");

    /* Create MDS session */
    printf("Creating MDS session...\n");
    ret = mds_session_create(device, &session);
    if (ret != 0) {
        fprintf(stderr, "Failed to create MDS session\n");
        goto cleanup;
    }

    /* Read device configuration */
    printf("Reading device configuration...\n");
    ret = mds_read_device_config(session, &config);
    if (ret != 0) {
        fprintf(stderr, "Failed to read device configuration\n");
        goto cleanup;
    }

    printf("\n--- Device Configuration ---\n");
    printf("Device ID:     %s\n", config.device_identifier);
    printf("Data URI:      %s\n", config.data_uri);
    printf("Authorization: %s\n", config.authorization);
    printf("Features:      0x%08X\n", config.supported_features);
    printf("----------------------------\n\n");

    /* Set up upload callback */
#ifdef MEMFAULT_HID_MDS_UPLOAD_ENABLED
    if (use_builtin_uploader) {
        printf("Setting up built-in HTTP uploader (libcurl)...\n");
        uploader = mds_uploader_create();
        if (uploader == NULL) {
            fprintf(stderr, "Failed to create uploader\n");
            goto cleanup;
        }

        /* Enable verbose output for demonstration */
        mds_uploader_set_verbose(uploader, true);

        /* Register the uploader callback */
        ret = mds_set_upload_callback(session, mds_uploader_callback, uploader);
        if (ret != 0) {
            fprintf(stderr, "Failed to set upload callback\n");
            goto cleanup;
        }
        printf("Built-in uploader configured\n\n");
    } else
#endif
    {
        printf("Setting up custom upload callback...\n");
        ret = mds_set_upload_callback(session, custom_upload_callback, &custom_chunk_count);
        if (ret != 0) {
            fprintf(stderr, "Failed to set custom upload callback\n");
            goto cleanup;
        }
        printf("Custom callback configured\n\n");
    }

    /* Enable streaming */
    printf("Enabling diagnostic data streaming...\n");
    ret = mds_stream_enable(session);
    if (ret != 0) {
        fprintf(stderr, "Failed to enable streaming\n");
        goto cleanup;
    }
    printf("Streaming enabled\n\n");

    printf("=========================================\n");
    printf("Gateway running. Press Ctrl+C to stop.\n");
    printf("=========================================\n\n");

    /* Process stream packets */
    int chunk_count = 0;
    while (keep_running) {
        /* Process one packet with 5 second timeout */
        ret = mds_stream_process(session, &config, 5000);

        if (ret == 0) {
            chunk_count++;
            printf("Processed chunk #%d\n", chunk_count);

#ifdef MEMFAULT_HID_MDS_UPLOAD_ENABLED
            if (use_builtin_uploader) {
                /* Print upload statistics */
                mds_upload_stats_t stats;
                mds_uploader_get_stats(uploader, &stats);
                printf("  Total uploaded: %zu chunks, %zu bytes\n",
                       stats.chunks_uploaded, stats.bytes_uploaded);
                if (stats.upload_failures > 0) {
                    printf("  Upload failures: %zu\n", stats.upload_failures);
                }
            }
#endif
        } else if (ret == -ETIMEDOUT) {
            /* Timeout is normal - no data available */
            continue;
        } else {
            fprintf(stderr, "Error processing stream: %d\n", ret);
            break;
        }
    }

    printf("\nShutting down...\n");

    /* Disable streaming */
    printf("Disabling streaming...\n");
    mds_stream_disable(session);

cleanup:
    /* Print final statistics */
#ifdef MEMFAULT_HID_MDS_UPLOAD_ENABLED
    if (uploader) {
        printf("\n--- Upload Statistics ---\n");
        mds_upload_stats_t stats;
        mds_uploader_get_stats(uploader, &stats);
        printf("Chunks uploaded:   %zu\n", stats.chunks_uploaded);
        printf("Bytes uploaded:    %zu\n", stats.bytes_uploaded);
        printf("Upload failures:   %zu\n", stats.upload_failures);
        printf("Last HTTP status:  %ld\n", stats.last_http_status);
        printf("-------------------------\n\n");

        mds_uploader_destroy(uploader);
    }
#else
    if (!use_builtin_uploader) {
        printf("\n--- Custom Upload Statistics ---\n");
        printf("Chunks processed: %d\n", custom_chunk_count);
        printf("--------------------------------\n\n");
    }
#endif

    /* Cleanup */
    if (session) {
        mds_session_destroy(session);
    }

    if (device) {
        memfault_hid_close(device);
    }

    memfault_hid_exit();

    printf("Gateway stopped\n");
    return 0;
}
