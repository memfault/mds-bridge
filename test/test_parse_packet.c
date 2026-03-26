/**
 * @file test_parse_packet.c
 * @brief Test suite for MDS stream packet parser
 *
 * Tests mds_parse_stream_packet() indirectly via mds_process_stream_from_bytes().
 * The parser is static in mds_protocol.c but accessible through the public API.
 */

#include "mds_bridge/mds_protocol.h"
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

/**
 * Helper: build an HID stream report buffer from sequence, length, and payload.
 * Format: [sequence_byte, length_byte, payload_data...]
 * Returns the total buffer length (2 + payload_len).
 */
static size_t build_stream_buffer(uint8_t *buf, size_t buf_size,
                                   uint8_t sequence, uint8_t length,
                                   const uint8_t *payload, size_t payload_len) {
    if (buf_size < 2) {
        return 0;
    }
    buf[0] = sequence;
    buf[1] = length;
    if (payload != NULL && payload_len > 0) {
        size_t copy = payload_len;
        if (copy > buf_size - 2) {
            copy = buf_size - 2;
        }
        memcpy(&buf[2], payload, copy);
    }
    return 2 + payload_len;
}

int main(void) {
    int ret;
    mds_session_t *session = NULL;
    mds_stream_packet_t packet;
    mds_device_config_t config;
    uint8_t buffer[128];
    size_t buf_len;

    printf("MDS Parse Packet Test Suite\n");
    printf("===========================\n");

    /* Create a session with NULL backend (allowed for buffer-based parsing) */
    ret = mds_session_create(NULL, &session);
    if (ret != 0) {
        printf("FATAL: Failed to create session: %d\n", ret);
        return 1;
    }

    /* Initialize a dummy config (not used for parsing, but required by API) */
    memset(&config, 0, sizeof(config));
    strncpy(config.data_uri, "https://test.memfault.com/api/v0/chunks/test",
            sizeof(config.data_uri) - 1);
    strncpy(config.authorization, "Memfault-Project-Key:test_key",
            sizeof(config.authorization) - 1);

    /* ================================================================
     * Test 1: Valid 61-byte real chunk INIT
     * ================================================================ */
    TEST_START("Valid 61-byte real chunk INIT");

    buf_len = build_stream_buffer(buffer, sizeof(buffer),
                                   0x00, (uint8_t)REAL_CHUNK_INIT_SIZE,
                                   REAL_CHUNK_INIT, REAL_CHUNK_INIT_SIZE);
    memset(&packet, 0, sizeof(packet));
    ret = mds_process_stream_from_bytes(session, &config, buffer, buf_len, &packet);
    TEST_ASSERT(ret == 0, "Parse returns success");
    TEST_ASSERT(packet.data_len == REAL_CHUNK_INIT_SIZE, "Data length is 61");
    TEST_ASSERT(memcmp(packet.data, REAL_CHUNK_INIT, REAL_CHUNK_INIT_SIZE) == 0,
                "Exact bytes match REAL_CHUNK_INIT");
    TEST_ASSERT(packet.sequence == 0, "Sequence is 0");

    /* ================================================================
     * Test 2: Valid small chunk (9-byte LAST)
     * ================================================================ */
    TEST_START("Valid small chunk (9-byte LAST)");

    buf_len = build_stream_buffer(buffer, sizeof(buffer),
                                   0x01, (uint8_t)REAL_CHUNK_LAST_SIZE,
                                   REAL_CHUNK_LAST, REAL_CHUNK_LAST_SIZE);
    memset(&packet, 0, sizeof(packet));
    ret = mds_process_stream_from_bytes(session, &config, buffer, buf_len, &packet);
    TEST_ASSERT(ret == 0, "Parse returns success");
    TEST_ASSERT(packet.data_len == REAL_CHUNK_LAST_SIZE, "Data length is 9");
    TEST_ASSERT(memcmp(packet.data, REAL_CHUNK_LAST, REAL_CHUNK_LAST_SIZE) == 0,
                "Exact bytes match REAL_CHUNK_LAST");

    /* ================================================================
     * Test 3: Zero-length payload
     * ================================================================ */
    TEST_START("Zero-length payload [seq=5, len=0]");

    buffer[0] = 5;  /* sequence = 5 */
    buffer[1] = 0;  /* length = 0 */
    memset(&packet, 0xFF, sizeof(packet));
    ret = mds_process_stream_from_bytes(session, &config, buffer, 2, &packet);
    TEST_ASSERT(ret == 0, "Parse returns success");
    TEST_ASSERT(packet.data_len == 0, "Data length is 0");
    TEST_ASSERT(packet.sequence == 5, "Sequence is 5");

    /* ================================================================
     * Test 4: Max-length payload (61 bytes)
     * ================================================================ */
    TEST_START("Max-length payload (61 bytes)");

    uint8_t max_payload[61];
    for (int i = 0; i < 61; i++) {
        max_payload[i] = (uint8_t)(i & 0xFF);
    }
    buf_len = build_stream_buffer(buffer, sizeof(buffer),
                                   0x06, 61, max_payload, 61);
    memset(&packet, 0, sizeof(packet));
    ret = mds_process_stream_from_bytes(session, &config, buffer, buf_len, &packet);
    TEST_ASSERT(ret == 0, "Parse returns success");
    TEST_ASSERT(packet.data_len == 61, "Data length is 61");

    /* ================================================================
     * Test 5: Length exceeds max (len=62 in header)
     * ================================================================ */
    TEST_START("Length exceeds max (len=62 in header)");

    uint8_t oversize_buf[64];
    oversize_buf[0] = 0x00;  /* sequence */
    oversize_buf[1] = 62;    /* length = 62 (exceeds MDS_MAX_CHUNK_DATA_LEN=61) */
    memset(&oversize_buf[2], 0xAA, 62);
    ret = mds_process_stream_from_bytes(session, &config, oversize_buf, 64, &packet);
    TEST_ASSERT(ret < 0, "Returns error for oversized payload");

    /* ================================================================
     * Test 6: Buffer too short (1 byte only)
     * ================================================================ */
    TEST_START("Buffer too short (1 byte only)");

    buffer[0] = 0x00;
    ret = mds_process_stream_from_bytes(session, &config, buffer, 1, &packet);
    TEST_ASSERT(ret < 0, "Returns error for 1-byte buffer");

    /* ================================================================
     * Test 7: Buffer shorter than declared length
     * ================================================================ */
    TEST_START("Buffer shorter than declared length");

    buffer[0] = 0x00;  /* sequence */
    buffer[1] = 20;    /* declares 20 bytes of payload */
    /* but total buffer is only 5 bytes (2 header + 3 data) */
    ret = mds_process_stream_from_bytes(session, &config, buffer, 5, &packet);
    TEST_ASSERT(ret < 0, "Returns error when buffer shorter than declared length");

    /* ================================================================
     * Test 8: NULL buffer
     * ================================================================ */
    TEST_START("NULL buffer");

    ret = mds_process_stream_from_bytes(session, &config, NULL, 10, &packet);
    TEST_ASSERT(ret < 0, "Returns error for NULL buffer");

    /* ================================================================
     * Test 9: Sequence extraction for values 0, 1, 15, 30, 31
     * ================================================================ */
    TEST_START("Sequence extraction: 0, 1, 15, 30, 31");

    uint8_t test_sequences[] = {0, 1, 15, 30, 31};
    const char *seq_names[] = {
        "Sequence 0 extracted correctly",
        "Sequence 1 extracted correctly",
        "Sequence 15 extracted correctly",
        "Sequence 30 extracted correctly",
        "Sequence 31 extracted correctly"
    };

    for (int i = 0; i < 5; i++) {
        /* Reset session sequence tracking to allow any sequence */
        mds_session_destroy(session);
        session = NULL;
        ret = mds_session_create(NULL, &session);
        if (ret != 0) {
            printf("FATAL: Failed to recreate session\n");
            return 1;
        }

        buffer[0] = test_sequences[i];  /* sequence in low 5 bits */
        buffer[1] = 1;                   /* 1 byte of payload */
        buffer[2] = 0xAA;               /* dummy payload */
        memset(&packet, 0, sizeof(packet));
        ret = mds_process_stream_from_bytes(session, &config, buffer, 3, &packet);
        TEST_ASSERT(ret == 0 && packet.sequence == test_sequences[i], seq_names[i]);
    }

    /* ================================================================
     * Test 10: Upper bits masked: sequence byte 0xFF gives sequence 31
     * ================================================================ */
    TEST_START("Upper bits masked: 0xFF -> sequence 31");

    mds_session_destroy(session);
    session = NULL;
    ret = mds_session_create(NULL, &session);
    if (ret != 0) {
        printf("FATAL: Failed to recreate session\n");
        return 1;
    }

    buffer[0] = 0xFF;  /* all bits set, sequence should be 0x1F = 31 */
    buffer[1] = 1;     /* 1 byte of payload */
    buffer[2] = 0xBB;
    memset(&packet, 0, sizeof(packet));
    ret = mds_process_stream_from_bytes(session, &config, buffer, 3, &packet);
    TEST_ASSERT(ret == 0, "Parse returns success");
    TEST_ASSERT(packet.sequence == 31, "Sequence byte 0xFF gives sequence 31");

    /* Cleanup */
    mds_session_destroy(session);

    /* Print summary */
    printf("\n========================================\n");
    printf("Parse Packet Test Summary\n");
    printf("========================================\n");
    printf("Total tests:  %d\n", test_count);
    printf("Assertions:   %d total (%d passed, %d failed)\n",
           test_passed + test_failed, test_passed, test_failed);
    printf("Result:       %s\n", test_failed == 0 ? "PASS" : "FAIL");
    printf("========================================\n\n");

    return test_failed == 0 ? 0 : 1;
}
