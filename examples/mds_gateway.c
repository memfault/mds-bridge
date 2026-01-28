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
#include "mds_bridge/memfault_hid.h"
#include "mds_bridge/platform_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#ifndef _WIN32
#include <unistd.h>  /* For usleep */
#endif

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
    bool verbose = false;
    int dry_run_chunk_count = 0;

    /* Parse arguments */
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <vid> <pid> [--dry-run] [--verbose]\n", argv[0]);
        fprintf(stderr, "\n");
        fprintf(stderr, "Arguments:\n");
        fprintf(stderr, "  vid        Vendor ID (hex, e.g., 2fe3)\n");
        fprintf(stderr, "  pid        Product ID (hex, e.g., 0007)\n");
        fprintf(stderr, "  --dry-run  Print chunks without uploading to Memfault cloud\n");
        fprintf(stderr, "  --verbose  Enable verbose HTTP output (curl details)\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "Examples:\n");
        fprintf(stderr, "  %s 2fe3 0007            # Upload to Memfault cloud\n", argv[0]);
        fprintf(stderr, "  %s 2fe3 0007 --dry-run  # Print only, no upload\n", argv[0]);
        fprintf(stderr, "  %s 2fe3 0007 --verbose  # Upload with detailed HTTP logging\n", argv[0]);
        fprintf(stderr, "\n");
        return 1;
    }

    if (sscanf(argv[1], "%x", &vid) != 1 || sscanf(argv[2], "%x", &pid) != 1) {
        fprintf(stderr, "Invalid VID/PID format. Use hex format (e.g., 2fe3)\n");
        return 1;
    }

    /* Check for optional flags */
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--dry-run") == 0) {
            dry_run = true;
            printf("DRY RUN mode - chunks will be printed but NOT uploaded\n\n");
        } else if (strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
            printf("Verbose mode enabled\n\n");
        }
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

        /* Enable verbose output if requested */
        chunks_uploader_set_verbose(uploader, verbose);

        /* Register the uploader callback */
        ret = mds_set_upload_callback(session, chunks_uploader_callback, uploader);
        if (ret != 0) {
            fprintf(stderr, "Failed to set upload callback\n");
            goto cleanup;
        }
        printf("HTTP uploader configured\n\n");
    }

    /* Flush any stale HID data before enabling streaming */
    printf("Flushing stale HID data...\n");
    {
        mds_stream_packet_t flush_pkt;
        int flush_count = 0;
        while (mds_stream_read_packet(session, &flush_pkt, 10) == 0) {
            flush_count++;
        }
        if (flush_count > 0) {
            printf("Flushed %d stale packets\n", flush_count);
        }
    }

    /* Enable streaming */
    printf("Enabling diagnostic data streaming...\n");
    ret = mds_stream_enable(session);
    if (ret != 0) {
        fprintf(stderr, "Failed to enable streaming\n");
        goto cleanup;
    }
    printf("Streaming enabled - ready to receive\n\n");

    printf("=========================================\n");
    printf("Gateway running. Press Ctrl+C to stop.\n");
    printf("=========================================\n\n");

    /*
     * Buffered packet processing to prevent HID buffer overflow.
     *
     * Problem: If we upload synchronously after each packet read, and the
     * device sends packets faster than our HTTP roundtrip, HID packets
     * accumulate in the kernel buffer and can be dropped.
     *
     * Solution: Read all available packets first (non-blocking), buffer them,
     * then upload the batch. This ensures we drain the HID buffer quickly.
     */
    #define CHUNK_BUFFER_SIZE 128
    typedef struct {
        uint8_t data[MDS_MAX_CHUNK_DATA_LEN];
        size_t len;
    } buffered_chunk_t;

    buffered_chunk_t *chunk_buffer = malloc(CHUNK_BUFFER_SIZE * sizeof(buffered_chunk_t));
    if (chunk_buffer == NULL) {
        fprintf(stderr, "Failed to allocate chunk buffer\n");
        goto cleanup;
    }

    int chunk_count = 0;
    int error_count = 0;
    int consecutive_io_errors = 0;
    uint8_t expected_seq = 0;  /* Track expected sequence for gap detection */
    bool first_packet = true;
    int dropped_packets = 0;

    while (keep_running) {
        /* Check for device disconnection (consecutive I/O errors) */
        if (consecutive_io_errors >= 3) {
            printf("\nDevice disconnected (consecutive I/O errors)\n");
            printf("Waiting for device to reconnect...\n");

            /* Clean up current session */
            mds_session_destroy(session);
            session = NULL;

            /* Wait for device to reappear */
            while (keep_running) {
                #ifdef _WIN32
                Sleep(1000);
                #else
                usleep(1000000);  /* 1 second */
                #endif

                ret = mds_session_create_hid(vid, pid, NULL, &session);
                if (ret == 0) {
                    printf("Device reconnected!\n");

                    /* Re-read config and re-enable streaming */
                    ret = mds_read_device_config(session, &config);
                    if (ret != 0) {
                        fprintf(stderr, "Failed to read device config after reconnect\n");
                        mds_session_destroy(session);
                        session = NULL;
                        continue;
                    }

                    /* Re-register upload callback */
                    if (!dry_run && uploader) {
                        mds_set_upload_callback(session, chunks_uploader_callback, uploader);
                    } else if (dry_run) {
                        mds_set_upload_callback(session, dry_run_callback, &dry_run_chunk_count);
                    }

                    ret = mds_stream_enable(session);
                    if (ret != 0) {
                        fprintf(stderr, "Failed to enable streaming after reconnect\n");
                        mds_session_destroy(session);
                        session = NULL;
                        continue;
                    }

                    printf("Streaming re-enabled, resuming...\n\n");
                    consecutive_io_errors = 0;
                    first_packet = true;
                    break;
                }
            }

            if (!keep_running || session == NULL) {
                break;
            }
        }
        /* Phase 1: Drain all available HID packets into buffer (short timeout) */
        size_t buffered_count = 0;
        while (buffered_count < CHUNK_BUFFER_SIZE) {
            mds_stream_packet_t packet;
            /* Use short timeout (10ms) to quickly drain buffer */
            ret = mds_stream_read_packet(session, &packet, 10);

            if (ret == 0) {
                /* Success - reset I/O error counter */
                consecutive_io_errors = 0;

                /* Check for sequence gaps (dropped packets) */
                if (first_packet) {
                    expected_seq = packet.sequence;
                    first_packet = false;
                    printf("First packet received, sequence=%u\n", packet.sequence);
                } else if (packet.sequence != expected_seq) {
                    /* Calculate how many packets we missed */
                    int gap = (packet.sequence - expected_seq) & 0x1F;
                    if (gap > 16) gap -= 32;  /* Handle wrap-around */
                    if (gap > 0) {
                        dropped_packets += gap;
                        fprintf(stderr, "*** SEQUENCE GAP: expected %u, got %u (missed %d packets, total dropped: %d) ***\n",
                                expected_seq, packet.sequence, gap, dropped_packets);
                    } else if (gap < 0) {
                        fprintf(stderr, "*** DUPLICATE/OUT-OF-ORDER: expected %u, got %u ***\n",
                                expected_seq, packet.sequence);
                    }
                }
                expected_seq = (packet.sequence + 1) & 0x1F;

                /* Buffer this chunk */
                chunk_buffer[buffered_count].len = packet.data_len;
                memcpy(chunk_buffer[buffered_count].data, packet.data, packet.data_len);
                buffered_count++;
            } else if (ret == -ETIMEDOUT || ret == MEMFAULT_HID_ERROR_TIMEOUT) {
                /* No more packets available - this is normal */
                break;
            } else {
                /* I/O error - possible disconnection */
                consecutive_io_errors++;
                if (consecutive_io_errors == 1) {
                    fprintf(stderr, "I/O error reading from device (error %d)\n", ret);
                }
                break;
            }
        }

        /* Phase 2: Upload buffered chunks */
        if (buffered_count > 0) {
            if (buffered_count > 1) {
                printf("Buffered %zu chunks, uploading...\n", buffered_count);
            }

            for (size_t i = 0; i < buffered_count; i++) {
                chunk_count++;

                if (dry_run) {
                    /* Call dry-run callback directly */
                    dry_run_callback(config.data_uri, config.authorization,
                                     chunk_buffer[i].data, chunk_buffer[i].len,
                                     &dry_run_chunk_count);
                } else {
                    /* Upload via HTTP */
                    ret = chunks_uploader_callback(config.data_uri, config.authorization,
                                                    chunk_buffer[i].data, chunk_buffer[i].len,
                                                    uploader);
                    if (ret != 0) {
                        fprintf(stderr, "Upload failed for chunk #%d\n", chunk_count);
                    }
                }
            }

            if (!dry_run && buffered_count > 0) {
                chunks_upload_stats_t stats;
                chunks_uploader_get_stats(uploader, &stats);
                printf("Processed %zu chunks (total: %d), uploaded: %zu chunks, %zu bytes\n",
                       buffered_count, chunk_count, stats.chunks_uploaded, stats.bytes_uploaded);
            }
        } else {
            /* No packets available - wait a bit before next poll */
            /* Use platform_sleep if available, otherwise just continue */
            #ifdef _WIN32
            Sleep(100);
            #else
            usleep(100000);  /* 100ms */
            #endif
        }
    }

    free(chunk_buffer);

    printf("\nShutting down...\n");

    /* Disable streaming (if session still valid) */
    if (session) {
        printf("Disabling streaming...\n");
        mds_stream_disable(session);
    }

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
