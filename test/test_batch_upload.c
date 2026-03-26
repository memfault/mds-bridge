/**
 * @file test_batch_upload.c
 * @brief Test suite for multipart/mixed batch upload via chunks_uploader
 *
 * Tests chunks_uploader_batch_callback() using the mock libcurl to capture
 * and verify the exact POST body format.
 */

#include "mds_bridge/chunks_uploader.h"
#include "mock_libcurl.h"
#include "test_data.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

static int test_count = 0;
static int test_passed = 0;
static int test_failed = 0;

#define TEST_START(name) \
    do { \
        printf("\n=== Test %d: %s ===\n", ++test_count, name); \
    } while(0)

#define TEST_ASSERT(condition, message) \
    if (condition) { printf("  ✓ %s\n", message); test_passed++; } \
    else { printf("  ✗ %s\n", message); test_failed++; }

static const char *TEST_URI = "https://chunks.memfault.com/api/v0/chunks/test";
static const char *TEST_AUTH = "Memfault-Project-Key:test_key";

/**
 * Helper: search for a byte sequence within a larger buffer.
 * Returns 1 if needle is found in haystack, 0 otherwise.
 */
static int find_bytes(const uint8_t *haystack, size_t haystack_len,
                      const uint8_t *needle, size_t needle_len) {
    if (needle_len > haystack_len) {
        return 0;
    }
    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        if (memcmp(&haystack[i], needle, needle_len) == 0) {
            return 1;
        }
    }
    return 0;
}

int main(void) {
    int ret;
    chunks_uploader_t *uploader = NULL;
    size_t body_len;
    const uint8_t *body;
    const char *headers;

    printf("MDS Batch Upload Test Suite\n");
    printf("===========================\n");

    /* ================================================================
     * Test 1: Single chunk uses simple POST
     * ================================================================ */
    TEST_START("Single chunk uses simple POST");

    mock_curl_reset();
    mock_curl_set_response(200, CURLE_OK);
    uploader = chunks_uploader_create();
    TEST_ASSERT(uploader != NULL, "Uploader created");

    const uint8_t single_chunk[] = {0x48, 0x01, 0x02, 0x03, 0x04};
    const uint8_t *chunks_arr[1] = { single_chunk };
    size_t lens_arr[1] = { sizeof(single_chunk) };

    ret = chunks_uploader_batch_callback(TEST_URI, TEST_AUTH,
                                          chunks_arr, lens_arr, 1, uploader);
    TEST_ASSERT(ret == 0, "Batch with 1 chunk succeeds");

    body = mock_curl_get_last_post_body(&body_len);
    TEST_ASSERT(body_len == sizeof(single_chunk), "Body length matches single chunk");
    TEST_ASSERT(memcmp(body, single_chunk, sizeof(single_chunk)) == 0,
                "Body is raw chunk data (not multipart)");

    chunks_uploader_destroy(uploader);

    /* ================================================================
     * Test 2: Two chunks produce multipart body with boundary
     * ================================================================ */
    TEST_START("Two chunks produce multipart body");

    mock_curl_reset();
    mock_curl_set_response(200, CURLE_OK);
    uploader = chunks_uploader_create();
    TEST_ASSERT(uploader != NULL, "Uploader created");

    const uint8_t chunk_a[] = {0x10, 0x20, 0x30};
    const uint8_t chunk_b[] = {0x40, 0x50};
    const uint8_t *two_chunks[2] = { chunk_a, chunk_b };
    size_t two_lens[2] = { sizeof(chunk_a), sizeof(chunk_b) };

    ret = chunks_uploader_batch_callback(TEST_URI, TEST_AUTH,
                                          two_chunks, two_lens, 2, uploader);
    TEST_ASSERT(ret == 0, "Batch with 2 chunks succeeds");

    body = mock_curl_get_last_post_body(&body_len);
    TEST_ASSERT(body_len > 0, "Body is non-empty");

    /* Check body starts with "--mds-bridge-chunk-boundary\r\n" */
    const char *boundary_start = "--mds-bridge-chunk-boundary\r\n";
    TEST_ASSERT(body_len >= strlen(boundary_start) &&
                memcmp(body, boundary_start, strlen(boundary_start)) == 0,
                "Body starts with --mds-bridge-chunk-boundary");

    /* Check that body contains two Content-Length parts */
    const char *cl_str = "Content-Length:";
    int cl_count = 0;
    for (size_t i = 0; i + strlen(cl_str) <= body_len; i++) {
        if (memcmp(&body[i], cl_str, strlen(cl_str)) == 0) {
            cl_count++;
        }
    }
    TEST_ASSERT(cl_count == 2, "Body contains two Content-Length headers");

    chunks_uploader_destroy(uploader);

    /* ================================================================
     * Test 3: Exact body format verification
     * ================================================================ */
    TEST_START("Exact multipart body format");

    mock_curl_reset();
    mock_curl_set_response(200, CURLE_OK);
    uploader = chunks_uploader_create();
    TEST_ASSERT(uploader != NULL, "Uploader created");

    const uint8_t small_a[] = {0xAA, 0xBB};
    const uint8_t small_b[] = {0xCC, 0xDD, 0xEE};
    const uint8_t *small_chunks[2] = { small_a, small_b };
    size_t small_lens[2] = { sizeof(small_a), sizeof(small_b) };

    ret = chunks_uploader_batch_callback(TEST_URI, TEST_AUTH,
                                          small_chunks, small_lens, 2, uploader);
    TEST_ASSERT(ret == 0, "Batch upload succeeds");

    body = mock_curl_get_last_post_body(&body_len);

    /* Build expected multipart body */
    /*
     * --mds-bridge-chunk-boundary\r\n
     * Content-Length: 2\r\n
     * \r\n
     * <0xAA 0xBB>\r\n
     * --mds-bridge-chunk-boundary\r\n
     * Content-Length: 3\r\n
     * \r\n
     * <0xCC 0xDD 0xEE>\r\n
     * --mds-bridge-chunk-boundary--\r\n
     */
    uint8_t expected[512];
    size_t elen = 0;
    const char *p;

    p = "--mds-bridge-chunk-boundary\r\nContent-Length: 2\r\n\r\n";
    memcpy(&expected[elen], p, strlen(p));
    elen += strlen(p);
    expected[elen++] = 0xAA;
    expected[elen++] = 0xBB;
    expected[elen++] = '\r';
    expected[elen++] = '\n';

    p = "--mds-bridge-chunk-boundary\r\nContent-Length: 3\r\n\r\n";
    memcpy(&expected[elen], p, strlen(p));
    elen += strlen(p);
    expected[elen++] = 0xCC;
    expected[elen++] = 0xDD;
    expected[elen++] = 0xEE;
    expected[elen++] = '\r';
    expected[elen++] = '\n';

    p = "--mds-bridge-chunk-boundary--\r\n";
    memcpy(&expected[elen], p, strlen(p));
    elen += strlen(p);

    TEST_ASSERT(body_len == elen, "Body length matches expected");
    if (body_len == elen) {
        TEST_ASSERT(memcmp(body, expected, elen) == 0,
                    "Body bytes match expected exactly");
    } else {
        printf("    Expected %zu bytes, got %zu bytes\n", elen, body_len);
        TEST_ASSERT(0, "Body bytes match expected exactly (skipped - length mismatch)");
    }

    chunks_uploader_destroy(uploader);

    /* ================================================================
     * Test 4: Content-Type header for multipart
     * ================================================================ */
    TEST_START("Content-Type header for multipart");

    mock_curl_reset();
    mock_curl_set_response(200, CURLE_OK);
    uploader = chunks_uploader_create();
    TEST_ASSERT(uploader != NULL, "Uploader created");

    const uint8_t ct_a[] = {0x01};
    const uint8_t ct_b[] = {0x02};
    const uint8_t *ct_chunks[2] = { ct_a, ct_b };
    size_t ct_lens[2] = { sizeof(ct_a), sizeof(ct_b) };

    ret = chunks_uploader_batch_callback(TEST_URI, TEST_AUTH,
                                          ct_chunks, ct_lens, 2, uploader);
    TEST_ASSERT(ret == 0, "Batch upload succeeds");

    headers = mock_curl_get_last_headers();
    TEST_ASSERT(strstr(headers, "multipart/mixed; boundary=mds-bridge-chunk-boundary") != NULL,
                "Headers contain correct Content-Type for multipart");

    chunks_uploader_destroy(uploader);

    /* ================================================================
     * Test 5: NULL params return -EINVAL
     * ================================================================ */
    TEST_START("NULL params return -EINVAL");

    mock_curl_reset();
    mock_curl_set_response(200, CURLE_OK);
    uploader = chunks_uploader_create();
    TEST_ASSERT(uploader != NULL, "Uploader created");

    const uint8_t dummy[] = {0x01};
    const uint8_t *dummy_arr[1] = { dummy };
    size_t dummy_lens[1] = { 1 };

    ret = chunks_uploader_batch_callback(NULL, TEST_AUTH,
                                          dummy_arr, dummy_lens, 1, uploader);
    TEST_ASSERT(ret == -EINVAL, "NULL uri returns -EINVAL");

    ret = chunks_uploader_batch_callback(TEST_URI, NULL,
                                          dummy_arr, dummy_lens, 1, uploader);
    TEST_ASSERT(ret == -EINVAL, "NULL auth_header returns -EINVAL");

    ret = chunks_uploader_batch_callback(TEST_URI, TEST_AUTH,
                                          NULL, dummy_lens, 1, uploader);
    TEST_ASSERT(ret == -EINVAL, "NULL chunks returns -EINVAL");

    ret = chunks_uploader_batch_callback(TEST_URI, TEST_AUTH,
                                          dummy_arr, NULL, 1, uploader);
    TEST_ASSERT(ret == -EINVAL, "NULL chunk_lens returns -EINVAL");

    ret = chunks_uploader_batch_callback(TEST_URI, TEST_AUTH,
                                          dummy_arr, dummy_lens, 0, uploader);
    TEST_ASSERT(ret == -EINVAL, "num_chunks=0 returns -EINVAL");

    ret = chunks_uploader_batch_callback(TEST_URI, TEST_AUTH,
                                          dummy_arr, dummy_lens, 1, NULL);
    TEST_ASSERT(ret == -EINVAL, "NULL user_data returns -EINVAL");

    chunks_uploader_destroy(uploader);

    /* ================================================================
     * Test 6: HTTP 500 error returns -EIO
     * ================================================================ */
    TEST_START("HTTP 500 error returns -EIO");

    mock_curl_reset();
    mock_curl_set_response(500, CURLE_OK);
    uploader = chunks_uploader_create();
    TEST_ASSERT(uploader != NULL, "Uploader created");

    ret = chunks_uploader_batch_callback(TEST_URI, TEST_AUTH,
                                          dummy_arr, dummy_lens, 1, uploader);
    TEST_ASSERT(ret == -EIO, "HTTP 500 returns -EIO");

    chunks_uploader_destroy(uploader);

    /* ================================================================
     * Test 7: Stats: 3 chunks with sizes 10, 20, 30
     * ================================================================ */
    TEST_START("Stats: 3 chunks (10, 20, 30 bytes)");

    mock_curl_reset();
    mock_curl_set_response(200, CURLE_OK);
    uploader = chunks_uploader_create();
    TEST_ASSERT(uploader != NULL, "Uploader created");

    uint8_t stat_a[10], stat_b[20], stat_c[30];
    memset(stat_a, 0x11, sizeof(stat_a));
    memset(stat_b, 0x22, sizeof(stat_b));
    memset(stat_c, 0x33, sizeof(stat_c));

    const uint8_t *stat_chunks[3] = { stat_a, stat_b, stat_c };
    size_t stat_lens[3] = { 10, 20, 30 };

    ret = chunks_uploader_batch_callback(TEST_URI, TEST_AUTH,
                                          stat_chunks, stat_lens, 3, uploader);
    TEST_ASSERT(ret == 0, "Batch of 3 chunks succeeds");

    chunks_upload_stats_t stats;
    ret = chunks_uploader_get_stats(uploader, &stats);
    TEST_ASSERT(ret == 0, "Stats retrieved");
    TEST_ASSERT(stats.chunks_uploaded == 3, "chunks_uploaded == 3");
    TEST_ASSERT(stats.bytes_uploaded == 60, "bytes_uploaded == 60");

    chunks_uploader_destroy(uploader);

    /* ================================================================
     * Test 8: Binary data with null bytes
     * ================================================================ */
    TEST_START("Binary data with null bytes");

    mock_curl_reset();
    mock_curl_set_response(200, CURLE_OK);
    uploader = chunks_uploader_create();
    TEST_ASSERT(uploader != NULL, "Uploader created");

    const uint8_t bin_chunk[] = {0x01, 0x00, 0x00, 0x03};
    const uint8_t *bin_arr[1] = { bin_chunk };
    size_t bin_lens[1] = { sizeof(bin_chunk) };

    ret = chunks_uploader_batch_callback(TEST_URI, TEST_AUTH,
                                          bin_arr, bin_lens, 1, uploader);
    TEST_ASSERT(ret == 0, "Upload with null bytes succeeds");

    body = mock_curl_get_last_post_body(&body_len);
    TEST_ASSERT(body_len == sizeof(bin_chunk), "Body length includes all bytes");

    /* Verify all 4 bytes appear intact (not truncated at 0x00) */
    int all_bytes_present = (body_len >= 4 &&
                             body[0] == 0x01 &&
                             body[1] == 0x00 &&
                             body[2] == 0x00 &&
                             body[3] == 0x03);
    TEST_ASSERT(all_bytes_present, "All bytes including 0x00 appear in body");

    chunks_uploader_destroy(uploader);

    /* ================================================================
     * Test 9: Real LAST chunk CRC integrity
     * ================================================================ */
    TEST_START("Real LAST chunk CRC integrity in multipart");

    mock_curl_reset();
    mock_curl_set_response(200, CURLE_OK);
    uploader = chunks_uploader_create();
    TEST_ASSERT(uploader != NULL, "Uploader created");

    /* Use two chunks to trigger multipart path: a dummy + REAL_CHUNK_LAST */
    const uint8_t crc_dummy[] = {0xFF};
    const uint8_t *crc_chunks[2] = { crc_dummy, REAL_CHUNK_LAST };
    size_t crc_lens[2] = { sizeof(crc_dummy), REAL_CHUNK_LAST_SIZE };

    ret = chunks_uploader_batch_callback(TEST_URI, TEST_AUTH,
                                          crc_chunks, crc_lens, 2, uploader);
    TEST_ASSERT(ret == 0, "Batch with REAL_CHUNK_LAST succeeds");

    body = mock_curl_get_last_post_body(&body_len);

    /* CRC bytes are the last 2 bytes of REAL_CHUNK_LAST: 0x77, 0x3B */
    uint8_t crc_bytes[2] = { REAL_CHUNK_LAST[7], REAL_CHUNK_LAST[8] };
    TEST_ASSERT(find_bytes(body, body_len, crc_bytes, 2),
                "CRC bytes (0x77, 0x3B) appear intact in multipart body");

    /* Also verify the full REAL_CHUNK_LAST appears in the body */
    TEST_ASSERT(find_bytes(body, body_len, REAL_CHUNK_LAST, REAL_CHUNK_LAST_SIZE),
                "Full REAL_CHUNK_LAST appears intact in multipart body");

    chunks_uploader_destroy(uploader);

    /* ================================================================
     * Test 10: Auth header formatting
     * ================================================================ */
    TEST_START("Auth header: verify mock headers contain auth");

    mock_curl_reset();
    mock_curl_set_response(200, CURLE_OK);
    uploader = chunks_uploader_create();
    TEST_ASSERT(uploader != NULL, "Uploader created");

    const uint8_t auth_chunk[] = {0x01, 0x02};
    const uint8_t *auth_arr[1] = { auth_chunk };
    size_t auth_lens[1] = { sizeof(auth_chunk) };

    ret = chunks_uploader_batch_callback(TEST_URI, "Memfault-Project-Key:test_key",
                                          auth_arr, auth_lens, 1, uploader);
    TEST_ASSERT(ret == 0, "Upload with auth succeeds");

    headers = mock_curl_get_last_headers();
    /* The chunks_uploader formats "Name:Value" as "Name: Value" for curl */
    TEST_ASSERT(strstr(headers, "Memfault-Project-Key: test_key") != NULL,
                "Headers contain properly formatted auth header");

    chunks_uploader_destroy(uploader);

    /* Print summary */
    printf("\n========================================\n");
    printf("Batch Upload Test Summary\n");
    printf("========================================\n");
    printf("Total tests:  %d\n", test_count);
    printf("Assertions:   %d total (%d passed, %d failed)\n",
           test_passed + test_failed, test_passed, test_failed);
    printf("Result:       %s\n", test_failed == 0 ? "PASS" : "FAIL");
    printf("========================================\n\n");

    return test_failed == 0 ? 0 : 1;
}
