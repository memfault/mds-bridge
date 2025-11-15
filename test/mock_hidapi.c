/**
 * @file mock_hidapi.c
 * @brief Mock implementation of hidapi for testing
 *
 * This provides a simulated HID device for testing the memfault_hid library
 * without requiring actual hardware or system permissions.
 */

#include <hidapi.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <stdio.h>
#include <stdbool.h>

/* Mock device configuration */
#define MOCK_VID 0x1234
#define MOCK_PID 0x5678

/* MDS Report IDs */
#define MDS_REPORT_ID_SUPPORTED_FEATURES  0x01
#define MDS_REPORT_ID_DEVICE_IDENTIFIER   0x02
#define MDS_REPORT_ID_DATA_URI            0x03
#define MDS_REPORT_ID_AUTHORIZATION       0x04
#define MDS_REPORT_ID_STREAM_CONTROL      0x05
#define MDS_REPORT_ID_STREAM_DATA         0x06

/* Mock device state */
typedef struct {
    bool open;
    bool nonblocking;

    /* Input report queue (for echoing output reports) */
    uint8_t input_queue[10][65];  /* Up to 10 reports, 64 bytes + report ID */
    size_t input_queue_len[10];
    size_t input_queue_head;
    size_t input_queue_tail;
    size_t input_queue_count;

    /* Feature report storage (by report ID) */
    uint8_t feature_reports[256][65];  /* One per report ID, 64 bytes + report ID */
    size_t feature_report_len[256];
    bool feature_report_set[256];

    /* MDS-specific state */
    bool mds_streaming_enabled;
    uint8_t mds_sequence_counter;
    size_t mds_chunk_sent_count;  /* For test verification */
} mock_device_state_t;

static mock_device_state_t g_mock_device = {0};
static bool g_initialized = false;

/* Device info for enumeration */
static struct hid_device_info g_device_info = {
    .path = "mock://device/1",
    .vendor_id = MOCK_VID,
    .product_id = MOCK_PID,
    .serial_number = L"TEST-001",
    .release_number = 0x0100,
    .manufacturer_string = L"Memfault Test",
    .product_string = L"Mock HID Device",
    .usage_page = 0xFF00,
    .usage = 0x0001,
    .interface_number = 0,
    .next = NULL
};

/* ============================================================================
 * Library Initialization
 * ========================================================================== */

int HID_API_EXPORT hid_init(void) {
    if (g_initialized) {
        return 0;
    }

    printf("[MOCK] hid_init()\n");
    memset(&g_mock_device, 0, sizeof(g_mock_device));
    g_initialized = true;
    return 0;
}

int HID_API_EXPORT hid_exit(void) {
    if (!g_initialized) {
        return 0;
    }

    printf("[MOCK] hid_exit()\n");
    g_initialized = false;
    return 0;
}

/* ============================================================================
 * Device Enumeration
 * ========================================================================== */

struct hid_device_info HID_API_EXPORT * hid_enumerate(unsigned short vendor_id,
                                                       unsigned short product_id) {
    printf("[MOCK] hid_enumerate(0x%04X, 0x%04X)\n", vendor_id, product_id);

    /* Return our mock device if VID/PID matches (or if both are 0) */
    if ((vendor_id == 0 && product_id == 0) ||
        (vendor_id == MOCK_VID && product_id == MOCK_PID)) {
        return &g_device_info;
    }

    return NULL;
}

void HID_API_EXPORT hid_free_enumeration(struct hid_device_info *devs) {
    printf("[MOCK] hid_free_enumeration(%p)\n", devs);
    /* Nothing to free - we return a static structure */
    (void)devs;
}

/* ============================================================================
 * MDS Initialization
 * ========================================================================== */

static void mds_initialize_feature_reports(void) {
    /* Initialize MDS Supported Features (Report ID 0x01) */
    uint8_t *features = g_mock_device.feature_reports[MDS_REPORT_ID_SUPPORTED_FEATURES];
    features[0] = MDS_REPORT_ID_SUPPORTED_FEATURES;
    /* Little-endian 32-bit 0x00000000 */
    features[1] = 0x00;
    features[2] = 0x00;
    features[3] = 0x00;
    features[4] = 0x00;
    g_mock_device.feature_report_len[MDS_REPORT_ID_SUPPORTED_FEATURES] = 5;
    g_mock_device.feature_report_set[MDS_REPORT_ID_SUPPORTED_FEATURES] = true;

    /* Initialize MDS Device Identifier (Report ID 0x02) */
    uint8_t *device_id = g_mock_device.feature_reports[MDS_REPORT_ID_DEVICE_IDENTIFIER];
    device_id[0] = MDS_REPORT_ID_DEVICE_IDENTIFIER;
    const char *id_str = "test-device-12345";
    strcpy((char *)&device_id[1], id_str);
    g_mock_device.feature_report_len[MDS_REPORT_ID_DEVICE_IDENTIFIER] = 1 + strlen(id_str) + 1;
    g_mock_device.feature_report_set[MDS_REPORT_ID_DEVICE_IDENTIFIER] = true;

    /* Initialize MDS Data URI (Report ID 0x03) */
    uint8_t *uri = g_mock_device.feature_reports[MDS_REPORT_ID_DATA_URI];
    uri[0] = MDS_REPORT_ID_DATA_URI;
    const char *uri_str = "https://chunks.memfault.com/api/v0/chunks/test-device";
    strcpy((char *)&uri[1], uri_str);
    g_mock_device.feature_report_len[MDS_REPORT_ID_DATA_URI] = 1 + strlen(uri_str) + 1;
    g_mock_device.feature_report_set[MDS_REPORT_ID_DATA_URI] = true;

    /* Initialize MDS Authorization (Report ID 0x04) */
    uint8_t *auth = g_mock_device.feature_reports[MDS_REPORT_ID_AUTHORIZATION];
    auth[0] = MDS_REPORT_ID_AUTHORIZATION;
    const char *auth_str = "Memfault-Project-Key:test_project_key_12345";
    strcpy((char *)&auth[1], auth_str);
    g_mock_device.feature_report_len[MDS_REPORT_ID_AUTHORIZATION] = 1 + strlen(auth_str) + 1;
    g_mock_device.feature_report_set[MDS_REPORT_ID_AUTHORIZATION] = true;

    printf("[MOCK] MDS feature reports initialized\n");
}

/* ============================================================================
 * Device Management
 * ========================================================================== */

hid_device * HID_API_EXPORT hid_open(unsigned short vendor_id,
                                      unsigned short product_id,
                                      const wchar_t *serial_number) {
    printf("[MOCK] hid_open(0x%04X, 0x%04X, %ls)\n",
           vendor_id, product_id, serial_number ? serial_number : L"NULL");

    if (vendor_id != MOCK_VID || product_id != MOCK_PID) {
        return NULL;
    }

    if (g_mock_device.open) {
        printf("[MOCK]   Device already open!\n");
        return NULL;
    }

    g_mock_device.open = true;
    g_mock_device.nonblocking = false;

    /* Initialize MDS feature reports */
    mds_initialize_feature_reports();

    /* Initialize MDS streaming state */
    g_mock_device.mds_streaming_enabled = false;
    g_mock_device.mds_sequence_counter = 0;
    g_mock_device.mds_chunk_sent_count = 0;

    /* Return a non-NULL pointer (the address of our mock state) */
    return (hid_device *)&g_mock_device;
}

hid_device * HID_API_EXPORT hid_open_path(const char *path) {
    printf("[MOCK] hid_open_path(%s)\n", path);

    if (strcmp(path, g_device_info.path) != 0) {
        return NULL;
    }

    if (g_mock_device.open) {
        printf("[MOCK]   Device already open!\n");
        return NULL;
    }

    g_mock_device.open = true;
    g_mock_device.nonblocking = false;

    /* Initialize MDS feature reports */
    mds_initialize_feature_reports();

    /* Initialize MDS streaming state */
    g_mock_device.mds_streaming_enabled = false;
    g_mock_device.mds_sequence_counter = 0;
    g_mock_device.mds_chunk_sent_count = 0;

    return (hid_device *)&g_mock_device;
}

void HID_API_EXPORT hid_close(hid_device *dev) {
    printf("[MOCK] hid_close(%p)\n", dev);

    if (dev == (hid_device *)&g_mock_device) {
        g_mock_device.open = false;

        /* Clear queues */
        g_mock_device.input_queue_head = 0;
        g_mock_device.input_queue_tail = 0;
        g_mock_device.input_queue_count = 0;
    }
}

/* ============================================================================
 * Report Communication
 * ========================================================================== */

/* Helper to queue a mock MDS stream data packet */
static void mds_queue_stream_packet(const char *chunk_data, size_t chunk_len) {
    if (g_mock_device.input_queue_count >= 10) {
        printf("[MOCK]   Input queue full, can't queue stream packet\n");
        return;
    }

    size_t idx = g_mock_device.input_queue_tail;
    uint8_t *packet = g_mock_device.input_queue[idx];

    /* Report ID */
    packet[0] = MDS_REPORT_ID_STREAM_DATA;

    /* Sequence byte (bits 0-4) */
    packet[1] = g_mock_device.mds_sequence_counter & 0x1F;

    /* Chunk data */
    if (chunk_len > 63) {
        chunk_len = 63;  /* Max payload */
    }
    memcpy(&packet[2], chunk_data, chunk_len);

    g_mock_device.input_queue_len[idx] = 2 + chunk_len;
    g_mock_device.input_queue_tail = (g_mock_device.input_queue_tail + 1) % 10;
    g_mock_device.input_queue_count++;

    /* Increment sequence counter (wraps at 31) */
    g_mock_device.mds_sequence_counter = (g_mock_device.mds_sequence_counter + 1) & 0x1F;
    g_mock_device.mds_chunk_sent_count++;

    printf("[MOCK]   Queued MDS stream packet #%zu (seq=%u, %zu bytes)\n",
           g_mock_device.mds_chunk_sent_count,
           packet[1], chunk_len);
}

int HID_API_EXPORT hid_write(hid_device *dev, const unsigned char *data, size_t length) {
    if (dev != (hid_device *)&g_mock_device || !g_mock_device.open) {
        return -1;
    }

    uint8_t report_id = data[0];
    printf("[MOCK] hid_write(report_id=0x%02X, length=%zu)\n", report_id, length);
    printf("[MOCK]   Data: ");
    for (size_t i = 0; i < length && i < 16; i++) {
        printf("%02X ", data[i]);
    }
    if (length > 16) {
        printf("...");
    }
    printf("\n");

    /* Handle MDS Stream Control (Report ID 0x05) */
    if (report_id == MDS_REPORT_ID_STREAM_CONTROL && length >= 2) {
        uint8_t mode = data[1];
        if (mode == 0x01) {  /* MDS_STREAM_MODE_ENABLED */
            printf("[MOCK]   MDS Streaming ENABLED\n");
            g_mock_device.mds_streaming_enabled = true;
            g_mock_device.mds_sequence_counter = 0;

            /* Queue some mock chunk data packets */
            mds_queue_stream_packet("MOCK_CHUNK_DATA_001", 19);
            mds_queue_stream_packet("MOCK_CHUNK_DATA_002", 19);
            mds_queue_stream_packet("MOCK_CHUNK_DATA_003", 19);
        } else {  /* MDS_STREAM_MODE_DISABLED */
            printf("[MOCK]   MDS Streaming DISABLED\n");
            g_mock_device.mds_streaming_enabled = false;
        }
        return (int)length;
    }

    /* Echo regular output reports back as input reports */
    if (g_mock_device.input_queue_count < 10) {
        size_t idx = g_mock_device.input_queue_tail;
        memcpy(g_mock_device.input_queue[idx], data, length);
        g_mock_device.input_queue_len[idx] = length;
        g_mock_device.input_queue_tail = (g_mock_device.input_queue_tail + 1) % 10;
        g_mock_device.input_queue_count++;
        printf("[MOCK]   Echoed to input queue (count=%zu)\n", g_mock_device.input_queue_count);
    } else {
        printf("[MOCK]   Input queue full, dropping echo\n");
    }

    return (int)length;
}

int HID_API_EXPORT hid_read(hid_device *dev, unsigned char *data, size_t length) {
    if (dev != (hid_device *)&g_mock_device || !g_mock_device.open) {
        return -1;
    }

    /* Non-blocking mode - return 0 if no data */
    if (g_mock_device.nonblocking && g_mock_device.input_queue_count == 0) {
        return 0;
    }

    /* Blocking mode - return 0 to simulate no data available */
    if (g_mock_device.input_queue_count == 0) {
        return 0;
    }

    /* Return queued input report */
    size_t idx = g_mock_device.input_queue_head;
    size_t copy_len = g_mock_device.input_queue_len[idx];
    if (copy_len > length) {
        copy_len = length;
    }

    memcpy(data, g_mock_device.input_queue[idx], copy_len);
    g_mock_device.input_queue_head = (g_mock_device.input_queue_head + 1) % 10;
    g_mock_device.input_queue_count--;

    printf("[MOCK] hid_read() -> %zu bytes (report_id=0x%02X, remaining=%zu)\n",
           copy_len, data[0], g_mock_device.input_queue_count);

    return (int)copy_len;
}

int HID_API_EXPORT hid_read_timeout(hid_device *dev, unsigned char *data,
                                     size_t length, int milliseconds) {
    printf("[MOCK] hid_read_timeout(timeout=%d)\n", milliseconds);

    if (dev != (hid_device *)&g_mock_device || !g_mock_device.open) {
        return -1;
    }

    /* Check if data available */
    if (g_mock_device.input_queue_count == 0) {
        /* Simulate timeout */
        return 0;
    }

    /* Return queued input report */
    return hid_read(dev, data, length);
}

int HID_API_EXPORT hid_send_feature_report(hid_device *dev,
                                            const unsigned char *data,
                                            size_t length) {
    if (dev != (hid_device *)&g_mock_device || !g_mock_device.open) {
        return -1;
    }

    uint8_t report_id = data[0];
    printf("[MOCK] hid_send_feature_report(report_id=0x%02X, length=%zu)\n",
           report_id, length);

    /* Store the feature report */
    if (length > sizeof(g_mock_device.feature_reports[report_id])) {
        length = sizeof(g_mock_device.feature_reports[report_id]);
    }

    memcpy(g_mock_device.feature_reports[report_id], data, length);
    g_mock_device.feature_report_len[report_id] = length;
    g_mock_device.feature_report_set[report_id] = true;

    printf("[MOCK]   Stored feature report 0x%02X (%zu bytes)\n", report_id, length);

    return (int)length;
}

int HID_API_EXPORT hid_get_feature_report(hid_device *dev,
                                           unsigned char *data,
                                           size_t length) {
    if (dev != (hid_device *)&g_mock_device || !g_mock_device.open) {
        return -1;
    }

    uint8_t report_id = data[0];
    printf("[MOCK] hid_get_feature_report(report_id=0x%02X)\n", report_id);

    /* Check if feature report was previously set */
    if (!g_mock_device.feature_report_set[report_id]) {
        /* Return default/empty report */
        memset(data, 0, length);
        data[0] = report_id;
        printf("[MOCK]   Returning default feature report (not previously set)\n");
        return (int)length;
    }

    /* Return stored feature report */
    size_t copy_len = g_mock_device.feature_report_len[report_id];
    if (copy_len > length) {
        copy_len = length;
    }

    memcpy(data, g_mock_device.feature_reports[report_id], copy_len);

    printf("[MOCK]   Returning stored feature report (%zu bytes)\n", copy_len);

    return (int)copy_len;
}

/* ============================================================================
 * Utility Functions
 * ========================================================================== */

int HID_API_EXPORT hid_set_nonblocking(hid_device *dev, int nonblock) {
    printf("[MOCK] hid_set_nonblocking(%d)\n", nonblock);

    if (dev != (hid_device *)&g_mock_device || !g_mock_device.open) {
        return -1;
    }

    g_mock_device.nonblocking = (nonblock != 0);
    return 0;
}
