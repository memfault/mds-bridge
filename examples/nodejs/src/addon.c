#include <node_api.h>
#include <mds_bridge/mds_protocol.h>
#include <mds_bridge/mds_backend.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

// Backend wrapper for N-API - bridges node-hid to C library
typedef struct {
    mds_backend_t backend;
    mds_backend_ops_t ops;
    napi_env env;
    napi_threadsafe_function read_tsfn;
    napi_threadsafe_function write_tsfn;
} backend_wrapper_t;

// Session wrapper for N-API
typedef struct {
    mds_session_t *session;
    backend_wrapper_t *backend_wrapper;
} session_wrapper_t;

// Read/write call data structures
typedef struct {
    uint8_t report_id;
    uint8_t *buffer;
    size_t length;
    int timeout_ms;
    int result;
    bool completed;
} read_call_data_t;

typedef struct {
    uint8_t report_id;
    const uint8_t *buffer;
    size_t length;
    int result;
    bool completed;
} write_call_data_t;

// Backend read callback - called by C library when it needs to read HID data
static int backend_read(void *impl_data, uint8_t report_id, uint8_t *buffer, size_t length, int timeout_ms) {
    backend_wrapper_t *backend = (backend_wrapper_t *)impl_data;

    // For now, return error - JavaScript will handle HID I/O via events
    // This backend is for session management only
    (void)backend;
    (void)report_id;
    (void)buffer;
    (void)length;
    (void)timeout_ms;
    return -ENOSYS;
}

// Backend write callback - called by C library when it needs to write HID data
static int backend_write(void *impl_data, uint8_t report_id, const uint8_t *buffer, size_t length) {
    backend_wrapper_t *backend = (backend_wrapper_t *)impl_data;

    // For now, return error - JavaScript will handle HID I/O
    (void)backend;
    (void)report_id;
    (void)buffer;
    (void)length;
    return -ENOSYS;
}

// Backend destroy callback
static void backend_destroy(void *impl_data) {
    // No-op, cleanup happens in finalize_session
    (void)impl_data;
}

// Finalize callback for session cleanup
static void finalize_session(napi_env env, void *finalize_data, void *finalize_hint) {
    (void)env;
    (void)finalize_hint;

    session_wrapper_t *wrapper = (session_wrapper_t *)finalize_data;
    if (wrapper) {
        if (wrapper->session) {
            mds_session_destroy(wrapper->session);
        }
        if (wrapper->backend_wrapper) {
            free(wrapper->backend_wrapper);
        }
        free(wrapper);
    }
}

// Create an MDS session with a custom backend
static napi_value CreateSession(napi_env env, napi_callback_info info) {
    napi_value result;

    // Create backend wrapper
    backend_wrapper_t *backend_wrapper = (backend_wrapper_t *)calloc(1, sizeof(backend_wrapper_t));
    if (!backend_wrapper) {
        napi_throw_error(env, NULL, "Failed to allocate backend wrapper");
        return NULL;
    }

    // Initialize backend ops
    backend_wrapper->ops.read = backend_read;
    backend_wrapper->ops.write = backend_write;
    backend_wrapper->ops.destroy = backend_destroy;

    // Initialize backend structure
    backend_wrapper->backend.ops = &backend_wrapper->ops;
    backend_wrapper->backend.impl_data = backend_wrapper;
    backend_wrapper->env = env;

    // Create MDS session with our backend
    mds_session_t *session = NULL;
    int ret = mds_session_create(&backend_wrapper->backend, &session);

    if (ret != 0 || session == NULL) {
        free(backend_wrapper);
        napi_throw_error(env, NULL, "Failed to create MDS session");
        return NULL;
    }

    // Create session wrapper
    session_wrapper_t *wrapper = (session_wrapper_t *)malloc(sizeof(session_wrapper_t));
    if (!wrapper) {
        mds_session_destroy(session);
        free(backend_wrapper);
        napi_throw_error(env, NULL, "Failed to allocate session wrapper");
        return NULL;
    }

    wrapper->session = session;
    wrapper->backend_wrapper = backend_wrapper;

    // Wrap as external
    napi_create_external(env, wrapper, finalize_session, NULL, &result);

    return result;
}

// Process stream packet from bytes (for node-hid event-driven I/O)
static napi_value ProcessStreamFromBytes(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value argv[3];
    napi_value result, value;

    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);

    if (argc < 3) {
        napi_throw_error(env, NULL, "Expected 3 arguments: session, config, buffer");
        return NULL;
    }

    // Get session wrapper
    session_wrapper_t *wrapper;
    napi_get_value_external(env, argv[0], (void **)&wrapper);

    if (!wrapper || !wrapper->session) {
        napi_throw_error(env, NULL, "Invalid session");
        return NULL;
    }

    // Get config object and extract fields
    mds_device_config_t config = {0};

    napi_value device_id, data_uri, auth;
    napi_get_named_property(env, argv[1], "deviceIdentifier", &device_id);
    napi_get_named_property(env, argv[1], "dataUri", &data_uri);
    napi_get_named_property(env, argv[1], "authorization", &auth);

    size_t str_len;
    napi_get_value_string_utf8(env, device_id, config.device_identifier, sizeof(config.device_identifier), &str_len);
    napi_get_value_string_utf8(env, data_uri, config.data_uri, sizeof(config.data_uri), &str_len);
    napi_get_value_string_utf8(env, auth, config.authorization, sizeof(config.authorization), &str_len);

    // Get buffer
    void *data;
    size_t length;
    napi_get_buffer_info(env, argv[2], &data, &length);

    // Process packet
    mds_stream_packet_t packet;
    int ret = mds_process_stream_from_bytes(
        wrapper->session,
        &config,
        (const uint8_t *)data,
        length,
        &packet
    );

    if (ret < 0) {
        char error_msg[64];
        snprintf(error_msg, sizeof(error_msg), "Failed to process packet: %d", ret);
        napi_throw_error(env, NULL, error_msg);
        return NULL;
    }

    // Create result object
    napi_create_object(env, &result);

    napi_create_uint32(env, packet.sequence, &value);
    napi_set_named_property(env, result, "sequence", value);

    napi_create_buffer_copy(env, packet.data_len, packet.data, NULL, &value);
    napi_set_named_property(env, result, "data", value);

    napi_create_uint32(env, packet.data_len, &value);
    napi_set_named_property(env, result, "length", value);

    return result;
}

// Module initialization
static napi_value Init(napi_env env, napi_value exports) {
    napi_value fn;

    // Export functions
    napi_create_function(env, NULL, 0, CreateSession, NULL, &fn);
    napi_set_named_property(env, exports, "createSession", fn);

    napi_create_function(env, NULL, 0, ProcessStreamFromBytes, NULL, &fn);
    napi_set_named_property(env, exports, "processStreamFromBytes", fn);

    // Export constants
    napi_value constants;
    napi_create_object(env, &constants);

    napi_create_uint32(env, MDS_REPORT_ID_SUPPORTED_FEATURES, &fn);
    napi_set_named_property(env, constants, "SUPPORTED_FEATURES", fn);

    napi_create_uint32(env, MDS_REPORT_ID_DEVICE_IDENTIFIER, &fn);
    napi_set_named_property(env, constants, "DEVICE_IDENTIFIER", fn);

    napi_create_uint32(env, MDS_REPORT_ID_DATA_URI, &fn);
    napi_set_named_property(env, constants, "DATA_URI", fn);

    napi_create_uint32(env, MDS_REPORT_ID_AUTHORIZATION, &fn);
    napi_set_named_property(env, constants, "AUTHORIZATION", fn);

    napi_create_uint32(env, MDS_REPORT_ID_STREAM_CONTROL, &fn);
    napi_set_named_property(env, constants, "STREAM_CONTROL", fn);

    napi_create_uint32(env, MDS_REPORT_ID_STREAM_DATA, &fn);
    napi_set_named_property(env, constants, "STREAM_DATA", fn);

    napi_set_named_property(env, exports, "MDS_REPORT_ID", constants);

    return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
