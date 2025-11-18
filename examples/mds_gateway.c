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
 *   ./mds_gateway <vid> <pid> [--dry-run]
 *
 * Examples:
 *   ./mds_gateway 1234 5678              # Upload to Memfault cloud
 *   ./mds_gateway 1234 5678 --dry-run    # Print chunks without uploading
 */

#include "mds_bridge/mds_protocol.h"
#include "mds_bridge/chunks_uploader.h"

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

/* Dry-run callback - prints chunks without uploading */
static int dry_run_callback(const char *uri,
                             const char *auth_header,
                             const uint8_t *chunk_data,
                             size_t chunk_len,
                             void *user_data) {
    int *chunk_count = (int *)user_data;
    (*chunk_count)++;

    printf("\n[DRY RUN] Chunk #%d (not uploading)\n", *chunk_count);
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

    return 0;
}

int main(int argc, char *argv[]) {
    int ret;
    unsigned int vid, pid;
    mds_session_t *session = NULL;
    mds_device_config_t config;
    chunks_uploader_t *uploader = NULL;
    bool dry_run = false;
    int dry_run_chunk_count = 0;

    /* Parse arguments */
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <vid> <pid> [--dry-run]\n", argv[0]);
        fprintf(stderr, "\n");
        fprintf(stderr, "Arguments:\n");
        fprintf(stderr, "  vid        Vendor ID (hex, e.g., 2fe3)\n");
        fprintf(stderr, "  pid        Product ID (hex, e.g., 0007)\n");
        fprintf(stderr, "  --dry-run  Print chunks without uploading to Memfault cloud\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "Examples:\n");
        fprintf(stderr, "  %s 2fe3 0007            # Upload to Memfault cloud\n", argv[0]);
        fprintf(stderr, "  %s 2fe3 0007 --dry-run  # Print only, no upload\n", argv[0]);
        fprintf(stderr, "\n");
        return 1;
    }

    if (sscanf(argv[1], "%x", &vid) != 1 || sscanf(argv[2], "%x", &pid) != 1) {
        fprintf(stderr, "Invalid VID/PID format. Use hex format (e.g., 2fe3)\n");
        return 1;
    }

    /* Check for dry-run flag */
    if (argc >= 4 && strcmp(argv[3], "--dry-run") == 0) {
        dry_run = true;
        printf("DRY RUN mode - chunks will be printed but NOT uploaded\n\n");
    }

    /* Set up signal handler for graceful shutdown */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("=========================================\n");
    printf("Memfault MDS Gateway\n");
    printf("=========================================\n\n");

    /* Create MDS session (opens HID device internally) */
    printf("Opening device %04X:%04X and creating MDS session...\n", vid, pid);
    ret = mds_session_create_hid(vid, pid, NULL, &session);
    if (ret != 0) {
        fprintf(stderr, "Failed to create MDS session: error %d\n", ret);
        return 1;
    }
    printf("MDS session created successfully\n\n");

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
    if (dry_run) {
        printf("Setting up dry-run callback (no upload)...\n");
        ret = mds_set_upload_callback(session, dry_run_callback, &dry_run_chunk_count);
        if (ret != 0) {
            fprintf(stderr, "Failed to set dry-run callback\n");
            goto cleanup;
        }
        printf("Dry-run callback configured\n\n");
    } else {
        printf("Setting up HTTP uploader (libcurl)...\n");
        uploader = chunks_uploader_create();
        if (uploader == NULL) {
            fprintf(stderr, "Failed to create uploader\n");
            goto cleanup;
        }

        /* Enable verbose output */
        chunks_uploader_set_verbose(uploader, true);

        /* Register the uploader callback */
        ret = mds_set_upload_callback(session, chunks_uploader_callback, uploader);
        if (ret != 0) {
            fprintf(stderr, "Failed to set upload callback\n");
            goto cleanup;
        }
        printf("HTTP uploader configured\n\n");
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
    int error_count = 0;
    while (keep_running) {
        /* Process one packet with 1 second timeout */
        ret = mds_process_stream(session, &config, 1000, NULL);

        if (ret == 0) {
            chunk_count++;
            if (!dry_run) {
                printf("Processed chunk #%d\n", chunk_count);
                /* Print upload statistics */
                chunks_upload_stats_t stats;
                chunks_uploader_get_stats(uploader, &stats);
                printf("  Total uploaded: %zu chunks, %zu bytes\n",
                       stats.chunks_uploaded, stats.bytes_uploaded);
                if (stats.upload_failures > 0) {
                    printf("  Upload failures: %zu\n", stats.upload_failures);
                }
            }
            /* In dry-run mode, the callback itself prints the chunk info */
        } else if (ret == -ETIMEDOUT) {
            /* Timeout is normal - no data available, continue */
            continue;
        } else {
            /* Other errors - warn but continue (device might not be sending data yet) */
            if (error_count == 0) {
                fprintf(stderr, "Warning: Error processing stream: %d (device might not be sending data yet)\n", ret);
            }
            error_count++;
        }
    }

    printf("\nShutting down...\n");

    /* Disable streaming */
    printf("Disabling streaming...\n");
    mds_stream_disable(session);

cleanup:
    /* Print final statistics */
    if (dry_run) {
        printf("\n--- Dry Run Statistics ---\n");
        printf("Chunks processed: %d\n", dry_run_chunk_count);
        printf("(Not uploaded - dry run mode)\n");
        printf("--------------------------\n\n");
    } else if (uploader) {
        printf("\n--- Upload Statistics ---\n");
        chunks_upload_stats_t stats;
        chunks_uploader_get_stats(uploader, &stats);
        printf("Chunks uploaded:   %zu\n", stats.chunks_uploaded);
        printf("Bytes uploaded:    %zu\n", stats.bytes_uploaded);
        printf("Upload failures:   %zu\n", stats.upload_failures);
        printf("Last HTTP status:  %ld\n", stats.last_http_status);
        printf("-------------------------\n\n");

        chunks_uploader_destroy(uploader);
    }

    /* Cleanup */
    if (session) {
        mds_session_destroy(session);  /* Also closes HID device */
    }

    printf("Gateway stopped\n");
    return 0;
}
