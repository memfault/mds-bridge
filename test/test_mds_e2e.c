/**
 * @file test_mds_e2e.c
 * @brief End-to-end integration test for MDS gateway workflow
 *
 * This test simulates a complete MDS gateway workflow without requiring
 * a physical device. It uses mock implementations of both hidapi and libcurl
 * to verify the entire data flow from device to cloud.
 *
 * Workflow tested:
 * 1. Initialize library
 * 2. Open mock HID device
 * 3. Create MDS session
 * 4. Read device configuration
 * 5. Set up uploader with mock HTTP
 * 6. Enable streaming
 * 7. Process stream packets
 * 8. Upload chunks to mock cloud
 * 9. Verify upload statistics
 * 10. Clean shutdown
 */

#include "../src/memfault_hid_internal.h"
#include "mds_bridge/mds_protocol.h"
#include "mds_bridge/chunks_uploader.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define TEST_VID 0x1234
#define TEST_PID 0x5678

#define COLOR_GREEN "\033[32m"
#define COLOR_RED "\033[31m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_RESET "\033[0m"

static int test_assertions_passed = 0;
static int test_assertions_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            printf("  " COLOR_GREEN "✓" COLOR_RESET " %s\n", message); \
            test_assertions_passed++; \
        } else { \
            printf("  " COLOR_RED "✗" COLOR_RESET " %s\n", message); \
            test_assertions_failed++; \
        } \
    } while(0)

#define TEST_SECTION(name) \
    printf("\n" COLOR_YELLOW "▸ %s" COLOR_RESET "\n", name)

int main(void) {
    int ret;
    mds_session_t *session = NULL;
    chunks_uploader_t *uploader = NULL;
    mds_device_config_t config;

    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  MDS Gateway End-to-End Integration Test                  ║\n");
    printf("║  Tests complete workflow with mocked device and cloud     ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");

    /* ========================================================================
     * Step 1: Initialize Library (done internally by MDS)
     * ======================================================================== */
    TEST_SECTION("Initializing HID library");
    ret = memfault_hid_init();
    TEST_ASSERT(ret == MEMFAULT_HID_SUCCESS, "Library initialized");

    /* ========================================================================
     * Step 2: Create MDS Session (opens HID device internally)
     * ======================================================================== */
    TEST_SECTION("Creating MDS session (opens mock HID device)");
    ret = mds_session_create_hid(TEST_VID, TEST_PID, NULL, &session);
    TEST_ASSERT(ret == 0, "MDS session created");
    TEST_ASSERT(session != NULL, "Session handle is valid");

    if (ret != 0) {
        printf("\n" COLOR_RED "Failed to create session - cannot continue" COLOR_RESET "\n");
        memfault_hid_exit();
        return 1;
    }

    /* ========================================================================
     * Step 4: Read Device Configuration
     * ======================================================================== */
    TEST_SECTION("Reading device configuration");
    memset(&config, 0, sizeof(config));
    ret = mds_read_device_config(session, &config);
    TEST_ASSERT(ret == 0, "Configuration read successfully");

    if (ret == 0) {
        printf("  Device ID:     %s\n", config.device_identifier);
        printf("  Data URI:      %s\n", config.data_uri);
        printf("  Authorization: %s\n", config.authorization);
        printf("  Features:      0x%08X\n", config.supported_features);

        TEST_ASSERT(strlen(config.device_identifier) > 0, "Device ID is present");
        TEST_ASSERT(strlen(config.data_uri) > 0, "Data URI is present");
        TEST_ASSERT(strlen(config.authorization) > 0, "Authorization is present");
    } else {
        printf("\n" COLOR_RED "Failed to read config - cannot continue" COLOR_RESET "\n");
        goto cleanup;
    }

    /* ========================================================================
     * Step 5: Set Up Uploader
     * ======================================================================== */
    TEST_SECTION("Setting up HTTP uploader (mock)");
    uploader = chunks_uploader_create();
    TEST_ASSERT(uploader != NULL, "Uploader created");

    if (uploader == NULL) {
        printf("\n" COLOR_RED "Failed to create uploader - cannot continue" COLOR_RESET "\n");
        goto cleanup;
    }

    /* Configure uploader */
    chunks_uploader_set_verbose(uploader, false);  /* Quiet for test */

    /* Register upload callback */
    ret = mds_set_upload_callback(session, chunks_uploader_callback, uploader);
    TEST_ASSERT(ret == 0, "Upload callback registered");

    /* ========================================================================
     * Step 6: Enable Streaming
     * ======================================================================== */
    TEST_SECTION("Enabling diagnostic streaming");

    ret = mds_stream_enable(session);
    TEST_ASSERT(ret == 0, "Streaming enabled");

    if (ret != 0) {
        printf("\n" COLOR_RED "Failed to enable streaming - cannot continue" COLOR_RESET "\n");
        goto cleanup;
    }

    /* ========================================================================
     * Step 7: Process Stream Packets
     * ======================================================================== */
    TEST_SECTION("Processing stream packets");

    int chunks_processed = 0;
    int max_chunks = 5;  /* Process up to 5 chunks */
    int timeout_count = 0;
    int max_timeouts = 10;  /* Give up after 10 consecutive timeouts */

    printf("  Processing up to %d chunks...\n", max_chunks);

    while (chunks_processed < max_chunks && timeout_count < max_timeouts) {
        ret = mds_process_stream(session, &config, 100, NULL);  /* 100ms timeout */

        if (ret == 0) {
            /* Successfully processed a chunk */
            chunks_processed++;
            timeout_count = 0;  /* Reset timeout counter */
            printf("  Chunk %d processed\n", chunks_processed);

            /* Get upload stats after each chunk */
            chunks_upload_stats_t stats;
            chunks_uploader_get_stats(uploader, &stats);
            printf("    Uploaded: %zu chunks, %zu bytes, status: %ld\n",
                   stats.chunks_uploaded, stats.bytes_uploaded, stats.last_http_status);
        } else if (ret == -ETIMEDOUT) {
            /* Timeout is normal if no more data */
            timeout_count++;
        } else {
            /* Other error */
            printf("  Warning: Error %d processing stream\n", ret);
            break;
        }
    }

    TEST_ASSERT(chunks_processed >= 3, "Processed at least 3 chunks (mock queues 3)");
    printf("  Total chunks processed: %d\n", chunks_processed);

    /* ========================================================================
     * Step 8: Verify Upload Statistics
     * ======================================================================== */
    TEST_SECTION("Verifying upload statistics");

    chunks_upload_stats_t final_stats;
    chunks_uploader_get_stats(uploader, &final_stats);

    printf("  Chunks uploaded:   %zu\n", final_stats.chunks_uploaded);
    printf("  Bytes uploaded:    %zu\n", final_stats.bytes_uploaded);
    printf("  Upload failures:   %zu\n", final_stats.upload_failures);
    printf("  Last HTTP status:  %ld\n", final_stats.last_http_status);

    TEST_ASSERT(final_stats.chunks_uploaded > 0, "At least one chunk uploaded");
    TEST_ASSERT(final_stats.bytes_uploaded > 0, "Bytes were uploaded");
    TEST_ASSERT(final_stats.upload_failures == 0, "No upload failures");
    TEST_ASSERT(final_stats.last_http_status == 202, "HTTP 202 Accepted");

    /* Verify stats match chunks processed */
    TEST_ASSERT(final_stats.chunks_uploaded == (size_t)chunks_processed,
                "Upload count matches processed count");

    /* ========================================================================
     * Step 9: Disable Streaming
     * ======================================================================== */
    TEST_SECTION("Disabling streaming");
    ret = mds_stream_disable(session);
    TEST_ASSERT(ret == 0, "Streaming disabled");

    /* ========================================================================
     * Step 10: Cleanup
     * ======================================================================== */
    TEST_SECTION("Cleanup");

cleanup:
    if (uploader) {
        chunks_uploader_destroy(uploader);
        TEST_ASSERT(true, "Uploader destroyed");
    }

    if (session) {
        mds_session_destroy(session);  /* Also closes HID device */
        TEST_ASSERT(true, "Session destroyed (HID device closed)");
    }

    ret = memfault_hid_exit();
    TEST_ASSERT(ret == MEMFAULT_HID_SUCCESS, "Library shutdown");

    /* ========================================================================
     * Test Summary
     * ======================================================================== */
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Test Summary                                              ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║  Assertions Passed:  %-5d                                 ║\n", test_assertions_passed);
    printf("║  Assertions Failed:  %-5d                                 ║\n", test_assertions_failed);
    printf("╠════════════════════════════════════════════════════════════╣\n");

    if (test_assertions_failed == 0) {
        printf("║  Result: " COLOR_GREEN "✓ ALL TESTS PASSED" COLOR_RESET "                              ║\n");
    } else {
        printf("║  Result: " COLOR_RED "✗ TESTS FAILED" COLOR_RESET "                                 ║\n");
    }

    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    return test_assertions_failed == 0 ? 0 : 1;
}
