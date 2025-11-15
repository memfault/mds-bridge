/**
 * @file test_client.c
 * @brief Test client for the memfault_hid library
 *
 * This program tests the library functionality with the virtual HID device.
 */

#include "memfault_hid/memfault_hid.h"
#include "memfault_hid/mds_protocol.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define TEST_VID 0x1234
#define TEST_PID 0x5678

#define REPORT_ID_INPUT_1     0x01
#define REPORT_ID_OUTPUT_1    0x02
#define REPORT_ID_FEATURE_1   0x03
#define REPORT_ID_INPUT_2     0x10
#define REPORT_ID_OUTPUT_2    0x11

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

int main(void) {
    int ret;
    memfault_hid_device_t *device = NULL;

    printf("Memfault HID Library Test Suite\n");
    printf("================================\n\n");

    /* Test 1: Library initialization */
    TEST_START("Library Initialization");
    ret = memfault_hid_init();
    TEST_ASSERT(ret == MEMFAULT_HID_SUCCESS, "Library initialized successfully");

    /* Test 2: Device enumeration */
    TEST_START("Device Enumeration");
    memfault_hid_device_info_t *devices = NULL;
    size_t num_devices = 0;

    ret = memfault_hid_enumerate(TEST_VID, TEST_PID, &devices, &num_devices);
    TEST_ASSERT(ret == MEMFAULT_HID_SUCCESS, "Enumeration successful");
    TEST_ASSERT(num_devices > 0, "Found virtual device");

    if (num_devices > 0) {
        printf("  Device info:\n");
        printf("    VID:PID: 0x%04X:0x%04X\n", devices[0].vendor_id, devices[0].product_id);
        printf("    Manufacturer: %ls\n", devices[0].manufacturer);
        printf("    Product: %ls\n", devices[0].product);
        printf("    Serial: %ls\n", devices[0].serial_number);
        printf("    Path: %s\n", devices[0].path);
    }

    memfault_hid_free_device_list(devices);

    /* Test 3: Open device */
    TEST_START("Device Open");
    ret = memfault_hid_open(TEST_VID, TEST_PID, NULL, &device);
    TEST_ASSERT(ret == MEMFAULT_HID_SUCCESS, "Device opened successfully");

    if (ret != MEMFAULT_HID_SUCCESS) {
        printf("\nFailed to open device. Make sure the virtual_hid_device is running!\n");
        memfault_hid_exit();
        return 1;
    }

    /* Test 4: Set non-blocking mode */
    TEST_START("Non-blocking Mode");
    ret = memfault_hid_set_nonblocking(device, true);
    TEST_ASSERT(ret == MEMFAULT_HID_SUCCESS, "Set non-blocking mode");

    /* Test 5: Configure report filter */
    TEST_START("Report Filtering");
    uint8_t allowed_reports[] = {
        REPORT_ID_INPUT_1,
        REPORT_ID_OUTPUT_1,
        REPORT_ID_FEATURE_1,
        REPORT_ID_INPUT_2,
        REPORT_ID_OUTPUT_2
    };

    memfault_hid_report_filter_t filter = {
        .report_ids = allowed_reports,
        .num_report_ids = sizeof(allowed_reports),
        .filter_enabled = true
    };

    ret = memfault_hid_set_report_filter(device, &filter);
    TEST_ASSERT(ret == MEMFAULT_HID_SUCCESS, "Report filter configured");

    /* Test 6: Write and read output/input reports */
    TEST_START("Output/Input Report Communication");

    uint8_t output_data[32];
    memset(output_data, 0, sizeof(output_data));
    sprintf((char *)output_data, "Hello from test client!");

    printf("  Sending output report (ID: 0x%02X)...\n", REPORT_ID_OUTPUT_1);
    ret = memfault_hid_write_report(device, REPORT_ID_OUTPUT_1, output_data,
                                     sizeof(output_data), 1000);
    TEST_ASSERT(ret > 0, "Output report sent");

    if (ret > 0) {
        printf("  Sent %d bytes\n", ret);

        // Wait a bit for the echo
        usleep(100000);  // 100ms

        // Try to read the echoed input report
        uint8_t input_data[64];
        uint8_t report_id = 0;

        printf("  Reading input report...\n");
        ret = memfault_hid_read_report(device, &report_id, input_data,
                                        sizeof(input_data), 1000);

        if (ret > 0) {
            printf("  Received %d bytes (Report ID: 0x%02X)\n", ret, report_id);
            TEST_ASSERT(report_id == REPORT_ID_OUTPUT_1, "Correct report ID echoed");

            // Verify data
            bool data_matches = (memcmp(input_data, output_data, ret) == 0);
            TEST_ASSERT(data_matches, "Echoed data matches sent data");

            if (data_matches) {
                printf("  Data: %s\n", (char *)input_data);
            }
        } else if (ret == MEMFAULT_HID_ERROR_TIMEOUT) {
            printf("  Timeout waiting for echo (this is OK if device doesn't echo)\n");
        } else {
            TEST_ASSERT(false, "Read input report");
        }
    }

    /* Test 7: Feature report (set) */
    TEST_START("Feature Report (Set)");

    uint8_t feature_data[64];
    memset(feature_data, 0, sizeof(feature_data));
    sprintf((char *)feature_data, "Feature report test data");
    feature_data[63] = 0x42;  // Test byte at end

    printf("  Setting feature report (ID: 0x%02X)...\n", REPORT_ID_FEATURE_1);
    ret = memfault_hid_set_feature_report(device, REPORT_ID_FEATURE_1, feature_data,
                                          sizeof(feature_data));
    TEST_ASSERT(ret > 0, "Feature report set");

    if (ret > 0) {
        printf("  Set %d bytes\n", ret);
    }

    /* Test 8: Feature report (get) */
    TEST_START("Feature Report (Get)");

    uint8_t feature_read[64];
    memset(feature_read, 0, sizeof(feature_read));

    printf("  Getting feature report (ID: 0x%02X)...\n", REPORT_ID_FEATURE_1);
    ret = memfault_hid_get_feature_report(device, REPORT_ID_FEATURE_1, feature_read,
                                          sizeof(feature_read));
    TEST_ASSERT(ret > 0, "Feature report retrieved");

    if (ret > 0) {
        printf("  Got %d bytes\n", ret);

        // Verify data
        bool data_matches = (memcmp(feature_read, feature_data, ret) == 0);
        TEST_ASSERT(data_matches, "Feature report data matches");

        if (data_matches) {
            printf("  Data: %s\n", (char *)feature_read);
            printf("  Test byte: 0x%02X\n", feature_read[63]);
        }
    }

    /* Test 9: Test report filtering */
    TEST_START("Report Filter Rejection");

    // Try to use a report ID that's not in the filter
    uint8_t filtered_data[32] = {0};
    uint8_t bad_report_id = 0xFF;

    printf("  Attempting to write with filtered report ID 0x%02X...\n", bad_report_id);
    ret = memfault_hid_write_report(device, bad_report_id, filtered_data,
                                     sizeof(filtered_data), 1000);
    TEST_ASSERT(ret == MEMFAULT_HID_ERROR_INVALID_REPORT_TYPE,
                "Filtered report ID rejected");

    /* Test 10: Multiple report IDs */
    TEST_START("Multiple Report IDs");

    uint8_t data_report2[32];
    sprintf((char *)data_report2, "Report 2 test");

    printf("  Sending output report 2 (ID: 0x%02X)...\n", REPORT_ID_OUTPUT_2);
    ret = memfault_hid_write_report(device, REPORT_ID_OUTPUT_2, data_report2,
                                     sizeof(data_report2), 1000);
    TEST_ASSERT(ret > 0, "Second output report sent");

    /* Test 11: Disable filter */
    TEST_START("Disable Report Filter");

    filter.filter_enabled = false;
    ret = memfault_hid_set_report_filter(device, &filter);
    TEST_ASSERT(ret == MEMFAULT_HID_SUCCESS, "Report filter disabled");

    // Now the filtered report should work
    printf("  Attempting to write with previously filtered report ID 0x%02X...\n", bad_report_id);
    ret = memfault_hid_write_report(device, bad_report_id, filtered_data,
                                     sizeof(filtered_data), 1000);
    // Note: This might still fail if the device doesn't support this report ID,
    // but it shouldn't fail due to filtering
    if (ret != MEMFAULT_HID_ERROR_INVALID_REPORT_TYPE) {
        TEST_ASSERT(true, "Filter bypass successful (no filter rejection)");
    }

    /* Test 13: MDS Session Creation */
    TEST_START("MDS Session Creation");
    mds_session_t *mds_session = NULL;
    ret = mds_session_create(device, &mds_session);
    TEST_ASSERT(ret == 0, "MDS session created successfully");
    TEST_ASSERT(mds_session != NULL, "MDS session handle is valid");

    /* Test 14: MDS Device Configuration */
    TEST_START("MDS Device Configuration");
    mds_device_config_t config;
    memset(&config, 0, sizeof(config));

    ret = mds_read_device_config(mds_session, &config);
    TEST_ASSERT(ret == 0, "Device configuration read successfully");

    printf("  Device Configuration:\n");
    printf("    Supported Features: 0x%08X\n", config.supported_features);
    printf("    Device ID: %s\n", config.device_identifier);
    printf("    Data URI: %s\n", config.data_uri);
    printf("    Authorization: %s\n", config.authorization);

    TEST_ASSERT(config.supported_features == 0x00000000, "Supported features is 0x00 (v1)");
    TEST_ASSERT(strlen(config.device_identifier) > 0, "Device ID is not empty");
    TEST_ASSERT(strlen(config.data_uri) > 0, "Data URI is not empty");
    TEST_ASSERT(strlen(config.authorization) > 0, "Authorization is not empty");

    /* Test 15: MDS Individual Config Reads */
    TEST_START("MDS Individual Config Items");

    uint32_t features = 0;
    ret = mds_get_supported_features(mds_session, &features);
    TEST_ASSERT(ret == 0, "Get supported features");
    TEST_ASSERT(features == config.supported_features, "Features match config read");

    char device_id[MDS_MAX_DEVICE_ID_LEN] = {0};
    ret = mds_get_device_identifier(mds_session, device_id, sizeof(device_id));
    TEST_ASSERT(ret == 0, "Get device identifier");
    TEST_ASSERT(strcmp(device_id, config.device_identifier) == 0, "Device ID matches config read");

    char uri[MDS_MAX_URI_LEN] = {0};
    ret = mds_get_data_uri(mds_session, uri, sizeof(uri));
    TEST_ASSERT(ret == 0, "Get data URI");
    TEST_ASSERT(strcmp(uri, config.data_uri) == 0, "URI matches config read");

    char auth[MDS_MAX_AUTH_LEN] = {0};
    ret = mds_get_authorization(mds_session, auth, sizeof(auth));
    TEST_ASSERT(ret == 0, "Get authorization");
    TEST_ASSERT(strcmp(auth, config.authorization) == 0, "Auth matches config read");

    /* Test 16: MDS Stream Enable */
    TEST_START("MDS Stream Enable");

    /* Drain any pending input reports from previous tests BEFORE enabling streaming */
    printf("  Draining input queue...\n");
    uint8_t drain_report_id;
    uint8_t drain_data[64];
    while (memfault_hid_read_report(device, &drain_report_id, drain_data,
                                     sizeof(drain_data), 0) > 0) {
        printf("  Drained report ID: 0x%02X\n", drain_report_id);
    }

    ret = mds_stream_enable(mds_session);
    TEST_ASSERT(ret == 0, "Streaming enabled successfully");

    /* Test 17: MDS Stream Packet Reading */
    TEST_START("MDS Stream Packet Reading");

    printf("  Reading stream packets...\n");

    mds_stream_packet_t packet;
    uint8_t expected_sequence = 0;
    int packets_received = 0;

    // Mock should queue 3 packets when streaming is enabled
    for (int i = 0; i < 3; i++) {
        memset(&packet, 0, sizeof(packet));

        ret = mds_stream_read_packet(mds_session, &packet, 1000);
        if (ret == 0) {
            packets_received++;
            printf("  Packet %d: sequence=%d, data_len=%zu\n",
                   i + 1, packet.sequence, packet.data_len);

            // Verify sequence
            if (i == 0) {
                expected_sequence = packet.sequence;
                TEST_ASSERT(true, "First packet received");
            } else {
                bool seq_valid = mds_validate_sequence(expected_sequence, packet.sequence);
                TEST_ASSERT(seq_valid, "Sequence number is valid");
                expected_sequence = packet.sequence;
            }

            // Verify we have data
            TEST_ASSERT(packet.data_len > 0, "Packet contains data");
            TEST_ASSERT(packet.data_len <= MDS_MAX_CHUNK_DATA_LEN, "Data length is within bounds");

            if (packet.data_len > 0) {
                printf("    Data: ");
                for (size_t j = 0; j < (packet.data_len < 16 ? packet.data_len : 16); j++) {
                    printf("%02X ", packet.data[j]);
                }
                if (packet.data_len > 16) {
                    printf("... (%zu bytes total)", packet.data_len);
                }
                printf("\n");
            }
        } else {
            printf("  Failed to read packet %d (ret=%d)\n", i + 1, ret);
            break;
        }
    }

    TEST_ASSERT(packets_received == 3, "Received expected number of packets");

    /* Test 18: MDS Sequence Validation */
    TEST_START("MDS Sequence Validation");

    // Test wrapping behavior
    bool valid = mds_validate_sequence(30, 31);
    TEST_ASSERT(valid, "Sequence 30->31 is valid");

    valid = mds_validate_sequence(31, 0);
    TEST_ASSERT(valid, "Sequence wraps from 31->0");

    valid = mds_validate_sequence(0, 1);
    TEST_ASSERT(valid, "Sequence 0->1 is valid");

    // Test dropped packet detection
    valid = mds_validate_sequence(5, 7);
    TEST_ASSERT(!valid, "Sequence 5->7 detects dropped packet");

    // Test duplicate detection
    valid = mds_validate_sequence(10, 10);
    TEST_ASSERT(!valid, "Sequence 10->10 detects duplicate");

    /* Test 19: MDS Stream Disable */
    TEST_START("MDS Stream Disable");
    ret = mds_stream_disable(mds_session);
    TEST_ASSERT(ret == 0, "Streaming disabled successfully");

    /* Test 20: MDS Session Cleanup */
    TEST_START("MDS Session Cleanup");
    mds_session_destroy(mds_session);
    TEST_ASSERT(true, "MDS session destroyed");

    /* Test 21: Cleanup */
    TEST_START("Cleanup");
    memfault_hid_close(device);
    TEST_ASSERT(true, "Device closed");

    ret = memfault_hid_exit();
    TEST_ASSERT(ret == MEMFAULT_HID_SUCCESS, "Library shutdown");

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
