/**
 * @file test_upload.c
 * @brief Test suite for MDS upload functionality
 *
 * This tests the upload components with a mock libcurl implementation.
 */

#include "mds_bridge/mds_protocol.h"
#include "mds_bridge/chunks_uploader.h"
#include "mock_libcurl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int test_count = 0;
static int test_passed = 0;
static int test_failed = 0;

#define TEST_START(name) \
    do { \
        printf("\n=== Test %d: %s ===\n", ++test_count, name); \
    } while(0)

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            printf("  ✓ %s\n", message); \
            test_passed++; \
        } else { \
            printf("  ✗ %s\n", message); \
            test_failed++; \
        } \
    } while(0)

/* Test callback data structure */
typedef struct {
    int upload_count;
    char last_uri[256];
    char last_auth[256];
    size_t last_chunk_len;
    int last_result;
} upload_test_data_t;

/* Custom upload callback for testing */
static int test_upload_callback(const char *uri, const char *auth_header,
                                 const uint8_t *chunk_data, size_t chunk_len,
                                 void *user_data) {
    upload_test_data_t *data = (upload_test_data_t *)user_data;
    data->upload_count++;
    strncpy(data->last_uri, uri, sizeof(data->last_uri) - 1);
    strncpy(data->last_auth, auth_header, sizeof(data->last_auth) - 1);
    data->last_chunk_len = chunk_len;
    (void)chunk_data; /* Unused in test */
    return data->last_result; /* Return configured result */
}

int main(void) {
    int ret;

    printf("MDS Upload Test Suite\n");
    printf("=====================\n\n");

    /* Test 1: Custom Upload Callback */
    TEST_START("Custom Upload Callback");

    upload_test_data_t upload_data = {0};
    upload_data.last_result = 0;  /* Success */

    const char *test_uri = "https://chunks.memfault.com/api/v0/chunks/test";
    const char *test_auth = "Memfault-Project-Key:test_key_12345";
    const uint8_t test_chunk[] = {0x01, 0x02, 0x03, 0x04, 0x05};

    ret = test_upload_callback(test_uri, test_auth, test_chunk, sizeof(test_chunk), &upload_data);

    TEST_ASSERT(ret == 0, "Callback returns success");
    TEST_ASSERT(upload_data.upload_count == 1, "Upload count incremented");
    TEST_ASSERT(strcmp(upload_data.last_uri, test_uri) == 0, "URI captured correctly");
    TEST_ASSERT(strcmp(upload_data.last_auth, test_auth) == 0, "Auth captured correctly");
    TEST_ASSERT(upload_data.last_chunk_len == sizeof(test_chunk), "Chunk length correct");

    /* Test 2: Custom Callback Error Handling */
    TEST_START("Custom Callback Error Handling");

    upload_data.upload_count = 0;
    upload_data.last_result = -5;  /* Simulate error */

    ret = test_upload_callback(test_uri, test_auth, test_chunk, sizeof(test_chunk), &upload_data);

    TEST_ASSERT(ret == -5, "Callback returns configured error");
    TEST_ASSERT(upload_data.upload_count == 1, "Upload count still incremented");

    /* Test 3: Uploader Creation and Destruction */
    TEST_START("Uploader Lifecycle");

    chunks_uploader_t *uploader = chunks_uploader_create();
    TEST_ASSERT(uploader != NULL, "Uploader created successfully");

    chunks_uploader_destroy(uploader);
    TEST_ASSERT(true, "Uploader destroyed successfully");

    /* Test 4: Uploader Configuration */
    TEST_START("Uploader Configuration");

    uploader = chunks_uploader_create();
    TEST_ASSERT(uploader != NULL, "Uploader created");

    ret = chunks_uploader_set_timeout(uploader, 60000);
    TEST_ASSERT(ret == 0, "Timeout set successfully");

    ret = chunks_uploader_set_verbose(uploader, true);
    TEST_ASSERT(ret == 0, "Verbose mode set successfully");

    chunks_uploader_destroy(uploader);

    /* Test 5: Uploader Callback - Success */
    TEST_START("Uploader Callback - Success");

    mock_curl_reset();
    mock_curl_set_response(200, CURLE_OK);

    uploader = chunks_uploader_create();
    TEST_ASSERT(uploader != NULL, "Uploader created");

    ret = chunks_uploader_callback(test_uri, test_auth, test_chunk, sizeof(test_chunk), uploader);
    TEST_ASSERT(ret == 0, "Upload succeeded");
    TEST_ASSERT(mock_curl_get_request_count() == 1, "HTTP request was made");

    printf("  HTTP requests: %d\n", mock_curl_get_request_count());
    printf("  Last URL: %s\n", mock_curl_get_last_url());

    /* Test 6: Uploader Statistics */
    TEST_START("Uploader Statistics");

    chunks_upload_stats_t stats;
    ret = chunks_uploader_get_stats(uploader, &stats);
    TEST_ASSERT(ret == 0, "Stats retrieved successfully");
    TEST_ASSERT(stats.chunks_uploaded == 1, "Chunk count correct");
    TEST_ASSERT(stats.bytes_uploaded == sizeof(test_chunk), "Byte count correct");
    TEST_ASSERT(stats.upload_failures == 0, "No failures");
    TEST_ASSERT(stats.last_http_status == 200, "HTTP status correct");

    printf("  Chunks uploaded: %zu\n", stats.chunks_uploaded);
    printf("  Bytes uploaded: %zu\n", stats.bytes_uploaded);
    printf("  Failures: %zu\n", stats.upload_failures);

    /* Test 7: Uploader Callback - HTTP Error */
    TEST_START("Uploader Callback - HTTP Error");

    mock_curl_reset();
    mock_curl_set_response(404, CURLE_OK);

    ret = chunks_uploader_callback(test_uri, test_auth, test_chunk, sizeof(test_chunk), uploader);
    TEST_ASSERT(ret < 0, "Upload failed with HTTP error");

    ret = chunks_uploader_get_stats(uploader, &stats);
    TEST_ASSERT(stats.upload_failures == 1, "Failure count incremented");
    TEST_ASSERT(stats.last_http_status == 404, "HTTP 404 recorded");

    /* Test 8: Uploader Callback - Network Error */
    TEST_START("Uploader Callback - Network Error");

    mock_curl_reset();
    mock_curl_set_response(0, CURLE_COULDNT_CONNECT);

    ret = chunks_uploader_callback(test_uri, test_auth, test_chunk, sizeof(test_chunk), uploader);
    TEST_ASSERT(ret < 0, "Upload failed with network error");

    ret = chunks_uploader_get_stats(uploader, &stats);
    TEST_ASSERT(stats.upload_failures == 2, "Failure count incremented again");

    /* Test 9: Uploader Statistics Reset */
    TEST_START("Statistics Reset");

    ret = chunks_uploader_reset_stats(uploader);
    TEST_ASSERT(ret == 0, "Stats reset successfully");

    ret = chunks_uploader_get_stats(uploader, &stats);
    TEST_ASSERT(stats.chunks_uploaded == 0, "Chunk count reset");
    TEST_ASSERT(stats.bytes_uploaded == 0, "Byte count reset");
    TEST_ASSERT(stats.upload_failures == 0, "Failure count reset");

    /* Test 10: Invalid Authorization Header */
    TEST_START("Invalid Authorization Header");

    const char *bad_auth = "InvalidFormatNoColon";
    mock_curl_reset();
    mock_curl_set_response(200, CURLE_OK);

    ret = chunks_uploader_callback(test_uri, bad_auth, test_chunk, sizeof(test_chunk), uploader);
    TEST_ASSERT(ret < 0, "Rejects invalid auth header format");

    ret = chunks_uploader_get_stats(uploader, &stats);
    TEST_ASSERT(stats.upload_failures == 1, "Failure recorded for invalid auth");

    /* Test 11: Multiple Successful Uploads */
    TEST_START("Multiple Uploads");

    mock_curl_reset();
    chunks_uploader_reset_stats(uploader);
    mock_curl_set_response(200, CURLE_OK);

    for (int i = 0; i < 5; i++) {
        ret = chunks_uploader_callback(test_uri, test_auth, test_chunk, sizeof(test_chunk), uploader);
        TEST_ASSERT(ret == 0, "Upload succeeded");
    }

    ret = chunks_uploader_get_stats(uploader, &stats);
    TEST_ASSERT(stats.chunks_uploaded == 5, "All chunks uploaded");
    TEST_ASSERT(stats.bytes_uploaded == 5 * sizeof(test_chunk), "Total bytes correct");
    TEST_ASSERT(stats.upload_failures == 0, "No failures");

    printf("  Total chunks: %zu\n", stats.chunks_uploaded);
    printf("  Total bytes: %zu\n", stats.bytes_uploaded);
    printf("  HTTP requests: %d\n", mock_curl_get_request_count());

    /* Cleanup */
    TEST_START("Cleanup");
    chunks_uploader_destroy(uploader);
    TEST_ASSERT(true, "Uploader destroyed");

    /* Print summary */
    printf("\n========================================\n");
    printf("Test Summary\n");
    printf("========================================\n");
    printf("Total tests:  %d\n", test_count);
    printf("Assertions:   %d total (%d passed, %d failed)\n",
           test_passed + test_failed, test_passed, test_failed);
    printf("Result:       %s\n", test_failed == 0 ? "PASS" : "FAIL");
    printf("========================================\n\n");

    return test_failed == 0 ? 0 : 1;
}
