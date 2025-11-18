/**
 * @file mds_protocol.c
 * @brief Implementation of Memfault Diagnostic Service (transport-agnostic)
 */

#include "memfault_hid/mds_protocol.h"
#include "memfault_hid/mds_backend.h"
#include "mds_backend_hid_internal.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

/* MDS Session structure */
struct mds_session {
    mds_backend_t *backend;
    uint8_t last_sequence;
    bool streaming_enabled;

    /* Chunk upload */
    mds_chunk_upload_callback_t upload_callback;
    void *upload_user_data;
};


/* ============================================================================
 * Internal Helper Functions
 * ========================================================================== */

static uint8_t mds_extract_sequence(uint8_t byte0) {
    return byte0 & MDS_SEQUENCE_MASK;
}

static bool mds_validate_sequence(uint8_t prev_seq, uint8_t new_seq) {
    /* Expected next sequence */
    uint8_t expected = (prev_seq + 1) & MDS_SEQUENCE_MASK;

    return (new_seq == expected);
}

static int mds_parse_stream_packet(const uint8_t *buffer, size_t buffer_len,
                                    mds_stream_packet_t *packet) {
    if (buffer == NULL || packet == NULL) {
        return -EINVAL;
    }

    if (buffer_len < 1) {
        return -EINVAL;  /* Need at least sequence byte */
    }

    /* Extract sequence number */
    packet->sequence = mds_extract_sequence(buffer[0]);

    /* Copy payload data */
    packet->data_len = buffer_len - 1;  /* Exclude sequence byte */
    if (packet->data_len > MDS_MAX_CHUNK_DATA_LEN) {
        packet->data_len = MDS_MAX_CHUNK_DATA_LEN;
    }

    if (packet->data_len > 0) {
        memcpy(packet->data, &buffer[1], packet->data_len);
    }

    return 0;
}


/* ============================================================================
 * MDS Session Management
 * ========================================================================== */

int mds_session_create(mds_backend_t *backend, mds_session_t **session) {
    if (session == NULL) {
        return -EINVAL;
    }

    // Note: backend can be NULL for external I/O (e.g., event-driven with mds_process_stream_from_bytes)

    mds_session_t *s = calloc(1, sizeof(mds_session_t));
    if (s == NULL) {
        return -ENOMEM;
    }

    s->backend = backend;
    s->last_sequence = MDS_SEQUENCE_MAX;  /* Initialize to max so first packet (0) is valid */
    s->streaming_enabled = false;

    *session = s;
    return 0;
}

int mds_session_create_hid(uint16_t vendor_id, uint16_t product_id,
                            const wchar_t *serial_number,
                            mds_session_t **session) {
    if (session == NULL) {
        return -EINVAL;
    }

    /* Create HID backend */
    mds_backend_t *backend = NULL;
    int ret = mds_backend_hid_create(vendor_id, product_id, serial_number, &backend);
    if (ret < 0) {
        return ret;
    }

    /* Create session with backend */
    ret = mds_session_create(backend, session);
    if (ret < 0) {
        mds_backend_destroy(backend);
        return ret;
    }

    return 0;
}

int mds_session_create_hid_path(const char *path, mds_session_t **session) {
    if (path == NULL || session == NULL) {
        return -EINVAL;
    }

    /* Create HID backend from path */
    mds_backend_t *backend = NULL;
    int ret = mds_backend_hid_create_path(path, &backend);
    if (ret < 0) {
        return ret;
    }

    /* Create session with backend */
    ret = mds_session_create(backend, session);
    if (ret < 0) {
        mds_backend_destroy(backend);
        return ret;
    }

    return 0;
}

void mds_session_destroy(mds_session_t *session) {
    if (session == NULL) {
        return;
    }

    /* Disable streaming if enabled */
    if (session->streaming_enabled) {
        mds_stream_disable(session);
    }

    /* Destroy backend (closes HID device and frees resources) */
    if (session->backend) {
        mds_backend_destroy(session->backend);
    }

    free(session);
}

/* ============================================================================
 * Device Configuration
 * ========================================================================== */

int mds_read_device_config(mds_session_t *session, mds_device_config_t *config) {
    if (session == NULL || config == NULL) {
        return -EINVAL;
    }

    int ret;

    /* Read supported features */
    ret = mds_get_supported_features(session, &config->supported_features);
    if (ret < 0) {
        return ret;
    }

    /* Read device identifier */
    ret = mds_get_device_identifier(session, config->device_identifier,
                                     sizeof(config->device_identifier));
    if (ret < 0) {
        return ret;
    }

    /* Read data URI */
    ret = mds_get_data_uri(session, config->data_uri,
                          sizeof(config->data_uri));
    if (ret < 0) {
        return ret;
    }

    /* Read authorization */
    ret = mds_get_authorization(session, config->authorization,
                               sizeof(config->authorization));
    if (ret < 0) {
        return ret;
    }

    return 0;
}

int mds_get_supported_features(mds_session_t *session, uint32_t *features) {
    if (session == NULL || features == NULL) {
        return -EINVAL;
    }

    uint8_t data[4] = {0};
    int ret = mds_backend_read(session->backend,
                                MDS_REPORT_ID_SUPPORTED_FEATURES,
                                data, sizeof(data), -1);
    if (ret < 0) {
        return ret;
    }

    if (ret < 4) {
        return -EINVAL;
    }

    /* Features is stored as little-endian 32-bit value */
    *features = (uint32_t)data[0] |
                ((uint32_t)data[1] << 8) |
                ((uint32_t)data[2] << 16) |
                ((uint32_t)data[3] << 24);

    return 0;
}

int mds_get_device_identifier(mds_session_t *session, char *device_id, size_t max_len) {
    if (session == NULL || device_id == NULL || max_len == 0) {
        return -EINVAL;
    }

    uint8_t data[MDS_MAX_DEVICE_ID_LEN];
    int ret = mds_backend_read(session->backend,
                                MDS_REPORT_ID_DEVICE_IDENTIFIER,
                                data, sizeof(data), -1);
    if (ret < 0) {
        return ret;
    }

    /* Copy string, ensuring null termination */
    size_t copy_len = (ret < max_len) ? ret : (max_len - 1);
    memcpy(device_id, data, copy_len);
    device_id[copy_len] = '\0';

    return 0;
}

int mds_get_data_uri(mds_session_t *session, char *uri, size_t max_len) {
    if (session == NULL || uri == NULL || max_len == 0) {
        return -EINVAL;
    }

    uint8_t data[MDS_MAX_URI_LEN];
    int ret = mds_backend_read(session->backend,
                                MDS_REPORT_ID_DATA_URI,
                                data, sizeof(data), -1);
    if (ret < 0) {
        return ret;
    }

    /* Copy string, ensuring null termination */
    size_t copy_len = (ret < max_len) ? ret : (max_len - 1);
    memcpy(uri, data, copy_len);
    uri[copy_len] = '\0';

    return 0;
}

int mds_get_authorization(mds_session_t *session, char *auth, size_t max_len) {
    if (session == NULL || auth == NULL || max_len == 0) {
        return -EINVAL;
    }

    uint8_t data[MDS_MAX_AUTH_LEN];
    int ret = mds_backend_read(session->backend,
                                MDS_REPORT_ID_AUTHORIZATION,
                                data, sizeof(data), -1);
    if (ret < 0) {
        return ret;
    }

    /* Copy string, ensuring null termination */
    size_t copy_len = (ret < max_len) ? ret : (max_len - 1);
    memcpy(auth, data, copy_len);
    auth[copy_len] = '\0';

    return 0;
}

/* ============================================================================
 * Stream Control
 * ========================================================================== */

int mds_stream_enable(mds_session_t *session) {
    if (session == NULL) {
        return -EINVAL;
    }

    /* Build stream control buffer */
    uint8_t buffer[1];
    buffer[0] = MDS_STREAM_MODE_ENABLED;

    /* Stream Control is a FEATURE report */
    int ret = mds_backend_write(session->backend,
                                 MDS_REPORT_ID_STREAM_CONTROL,
                                 buffer, sizeof(buffer));
    if (ret < 0) {
        return ret;
    }

    session->streaming_enabled = true;
    return 0;
}

int mds_stream_disable(mds_session_t *session) {
    if (session == NULL) {
        return -EINVAL;
    }

    /* Build stream control buffer */
    uint8_t buffer[1];
    buffer[0] = MDS_STREAM_MODE_DISABLED;

    /* Stream Control is a FEATURE report */
    int ret = mds_backend_write(session->backend,
                                 MDS_REPORT_ID_STREAM_CONTROL,
                                 buffer, sizeof(buffer));
    if (ret < 0) {
        return ret;
    }

    session->streaming_enabled = false;
    return 0;
}

/* ============================================================================
 * Stream Data Reception
 * ========================================================================== */

int mds_stream_read_packet(mds_session_t *session, mds_stream_packet_t *packet,
                           int timeout_ms) {
    if (session == NULL || packet == NULL) {
        return -EINVAL;
    }

    uint8_t data[MDS_MAX_CHUNK_DATA_LEN + 1];  /* +1 for sequence byte */

    int ret = mds_backend_read(session->backend,
                                MDS_REPORT_ID_STREAM_DATA,
                                data, sizeof(data), timeout_ms);
    if (ret < 0) {
        return ret;
    }

    /* Use the buffer-based parser */
    ret = mds_parse_stream_packet(data, ret, packet);
    if (ret < 0) {
        return ret;
    }

    /* Update last sequence */
    session->last_sequence = packet->sequence;

    return 0;
}

/* ============================================================================
 * Chunk Upload
 * ========================================================================== */

int mds_set_upload_callback(mds_session_t *session,
                             mds_chunk_upload_callback_t callback,
                             void *user_data) {
    if (session == NULL) {
        return -EINVAL;
    }

    session->upload_callback = callback;
    session->upload_user_data = user_data;

    return 0;
}

/* Common packet processing logic (validate, update sequence, upload) */
static int mds_process_packet_common(mds_session_t *session,
                                      const mds_device_config_t *config,
                                      const mds_stream_packet_t *pkt,
                                      mds_stream_packet_t *packet_out) {
    /* Validate sequence if we have a previous sequence */
    if (session->last_sequence != MDS_SEQUENCE_MAX) {
        if (!mds_validate_sequence(session->last_sequence, pkt->sequence)) {
            /* Log warning but continue - sequence validation is not critical */
            uint8_t expected = (session->last_sequence + 1) & MDS_SEQUENCE_MASK;
            fprintf(stderr, "[MDS] Sequence error: expected %u, got %u\n",
                    expected, pkt->sequence);
        }
    }

    /* Update sequence */
    session->last_sequence = pkt->sequence;

    /* Copy packet to output if requested */
    if (packet_out) {
        *packet_out = *pkt;
    }

    /* Upload chunk if callback is configured */
    if (session->upload_callback != NULL) {
        int ret = session->upload_callback(config->data_uri,
                                            config->authorization,
                                            pkt->data,
                                            pkt->data_len,
                                            session->upload_user_data);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

int mds_process_stream(mds_session_t *session,
                       const mds_device_config_t *config,
                       int timeout_ms,
                       mds_stream_packet_t *packet) {
    if (session == NULL || config == NULL) {
        return -EINVAL;
    }

    mds_stream_packet_t pkt;
    int ret = mds_stream_read_packet(session, &pkt, timeout_ms);
    if (ret < 0) {
        return ret;
    }

    return mds_process_packet_common(session, config, &pkt, packet);
}

int mds_process_stream_from_bytes(mds_session_t *session,
                                   const mds_device_config_t *config,
                                   const uint8_t *buffer,
                                   size_t buffer_len,
                                   mds_stream_packet_t *packet) {
    if (session == NULL || config == NULL || buffer == NULL) {
        return -EINVAL;
    }

    mds_stream_packet_t pkt;
    int ret = mds_parse_stream_packet(buffer, buffer_len, &pkt);
    if (ret < 0) {
        return ret;
    }

    return mds_process_packet_common(session, config, &pkt, packet);
}
