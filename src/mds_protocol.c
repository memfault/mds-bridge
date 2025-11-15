/**
 * @file mds_protocol.c
 * @brief Implementation of Memfault Diagnostic Service over HID
 */

#include "memfault_hid/mds_protocol.h"
#include "memfault_hid/memfault_hid.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* MDS Session structure */
struct mds_session {
    memfault_hid_device_t *device;
    uint8_t last_sequence;
    bool streaming_enabled;

    /* Async streaming (future enhancement) */
    bool async_active;
    mds_stream_callback_t stream_callback;
    void *stream_user_data;

    /* Chunk upload */
    mds_chunk_upload_callback_t upload_callback;
    void *upload_user_data;
};

/* ============================================================================
 * MDS Session Management
 * ========================================================================== */

int mds_session_create(memfault_hid_device_t *device, mds_session_t **session) {
    if (device == NULL || session == NULL) {
        return -EINVAL;
    }

    mds_session_t *s = calloc(1, sizeof(mds_session_t));
    if (s == NULL) {
        return -ENOMEM;
    }

    s->device = device;
    s->last_sequence = MDS_SEQUENCE_MAX;  /* Initialize to max so first packet (0) is valid */
    s->streaming_enabled = false;
    s->async_active = false;

    *session = s;
    return 0;
}

void mds_session_destroy(mds_session_t *session) {
    if (session == NULL) {
        return;
    }

    /* Stop async streaming if active */
    if (session->async_active) {
        mds_stream_stop_async(session);
    }

    /* Disable streaming if enabled */
    if (session->streaming_enabled) {
        mds_stream_disable(session);
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
    int ret = memfault_hid_get_feature_report(session->device,
                                               MDS_REPORT_ID_SUPPORTED_FEATURES,
                                               data, sizeof(data));
    if (ret < 0) {
        return ret;
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
    int ret = memfault_hid_get_feature_report(session->device,
                                               MDS_REPORT_ID_DEVICE_IDENTIFIER,
                                               data, sizeof(data));
    if (ret < 0) {
        return ret;
    }

    /* Copy string, ensuring null termination */
    size_t copy_len = (ret < (int)max_len) ? ret : (max_len - 1);
    memcpy(device_id, data, copy_len);
    device_id[copy_len] = '\0';

    return 0;
}

int mds_get_data_uri(mds_session_t *session, char *uri, size_t max_len) {
    if (session == NULL || uri == NULL || max_len == 0) {
        return -EINVAL;
    }

    uint8_t data[MDS_MAX_URI_LEN];
    int ret = memfault_hid_get_feature_report(session->device,
                                               MDS_REPORT_ID_DATA_URI,
                                               data, sizeof(data));
    if (ret < 0) {
        return ret;
    }

    /* Copy string, ensuring null termination */
    size_t copy_len = (ret < (int)max_len) ? ret : (max_len - 1);
    memcpy(uri, data, copy_len);
    uri[copy_len] = '\0';

    return 0;
}

int mds_get_authorization(mds_session_t *session, char *auth, size_t max_len) {
    if (session == NULL || auth == NULL || max_len == 0) {
        return -EINVAL;
    }

    uint8_t data[MDS_MAX_AUTH_LEN];
    int ret = memfault_hid_get_feature_report(session->device,
                                               MDS_REPORT_ID_AUTHORIZATION,
                                               data, sizeof(data));
    if (ret < 0) {
        return ret;
    }

    /* Copy string, ensuring null termination */
    size_t copy_len = (ret < (int)max_len) ? ret : (max_len - 1);
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

    uint8_t mode = MDS_STREAM_MODE_ENABLED;
    int ret = memfault_hid_write_report(session->device,
                                         MDS_REPORT_ID_STREAM_CONTROL,
                                         &mode, 1, 1000);
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

    uint8_t mode = MDS_STREAM_MODE_DISABLED;
    int ret = memfault_hid_write_report(session->device,
                                         MDS_REPORT_ID_STREAM_CONTROL,
                                         &mode, 1, 1000);
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

    uint8_t report_id;
    uint8_t data[MDS_MAX_CHUNK_DATA_LEN + 1];  /* +1 for sequence byte */

    int ret = memfault_hid_read_report(session->device, &report_id,
                                        data, sizeof(data), timeout_ms);
    if (ret < 0) {
        return ret;
    }

    /* Verify this is a stream data report */
    if (report_id != MDS_REPORT_ID_STREAM_DATA) {
        return -EINVAL;  /* Wrong report type */
    }

    if (ret < 1) {
        return -EINVAL;  /* Need at least sequence byte */
    }

    /* Extract sequence number */
    packet->sequence = mds_extract_sequence(data[0]);

    /* Copy payload data */
    packet->data_len = ret - 1;  /* Exclude sequence byte */
    if (packet->data_len > 0) {
        memcpy(packet->data, &data[1], packet->data_len);
    }

    /* Update last sequence */
    session->last_sequence = packet->sequence;

    return 0;
}

int mds_stream_start_async(mds_session_t *session,
                           mds_stream_callback_t callback,
                           void *user_data) {
    if (session == NULL || callback == NULL) {
        return -EINVAL;
    }

    if (session->async_active) {
        return -EBUSY;  /* Already running */
    }

    /* TODO: Implement async reading with threads
     * For now, return not implemented */
    (void)user_data;
    return -ENOSYS;
}

int mds_stream_stop_async(mds_session_t *session) {
    if (session == NULL) {
        return -EINVAL;
    }

    if (!session->async_active) {
        return 0;  /* Not running */
    }

    /* TODO: Implement async stop */
    return -ENOSYS;
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

int mds_stream_process(mds_session_t *session,
                        const mds_device_config_t *config,
                        int timeout_ms) {
    if (session == NULL || config == NULL) {
        return -EINVAL;
    }

    mds_stream_packet_t packet;
    int ret = mds_stream_read_packet(session, &packet, timeout_ms);
    if (ret < 0) {
        return ret;
    }

    /* Validate sequence if we have a previous sequence */
    if (session->last_sequence != MDS_SEQUENCE_MAX) {
        if (!mds_validate_sequence(session->last_sequence, packet.sequence)) {
            /* Log warning but continue - sequence validation is not critical */
            /* Note: Actual logging would require a logging callback or printf */
        }
    }

    /* Upload chunk if callback is configured */
    if (session->upload_callback != NULL) {
        ret = session->upload_callback(config->data_uri,
                                        config->authorization,
                                        packet.data,
                                        packet.data_len,
                                        session->upload_user_data);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

/* ============================================================================
 * Utility Functions
 * ========================================================================== */

bool mds_validate_sequence(uint8_t prev_seq, uint8_t new_seq) {
    /* Expected next sequence */
    uint8_t expected = (prev_seq + 1) & MDS_SEQUENCE_MASK;

    return (new_seq == expected);
}
