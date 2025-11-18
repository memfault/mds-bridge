#include <node_api.h>
#include <memfault_hid/mds_protocol.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// These functions are implemented in JavaScript instead
// since they don't require C parsing

// JavaScript: parseStreamPacket(buffer)
// Parse a stream data packet
static napi_value ParseStreamPacket(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value argv[1];
    napi_value result, value;

    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);

    if (argc < 1) {
        napi_throw_error(env, NULL, "Expected buffer argument");
        return NULL;
    }

    void *data;
    size_t length;
    napi_get_buffer_info(env, argv[0], &data, &length);

    mds_stream_packet_t packet;
    int ret = mds_parse_stream_packet(data, length, &packet);

    if (ret < 0) {
        char error_msg[64];
        snprintf(error_msg, sizeof(error_msg), "Failed to parse packet: %d", ret);
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

// JavaScript: validateSequence(lastSeq, currentSeq)
// Validate stream packet sequence number
static napi_value ValidateSequence(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value argv[2];
    napi_value result;
    uint32_t last_seq, current_seq;

    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);

    if (argc < 2) {
        napi_throw_error(env, NULL, "Expected 2 arguments");
        return NULL;
    }

    napi_get_value_uint32(env, argv[0], &last_seq);
    napi_get_value_uint32(env, argv[1], &current_seq);

    bool valid = mds_validate_sequence((uint8_t)last_seq, (uint8_t)current_seq);

    napi_get_boolean(env, valid, &result);
    return result;
}

// Module initialization
static napi_value Init(napi_env env, napi_value exports) {
    napi_value fn;

    napi_create_function(env, NULL, 0, ParseStreamPacket, NULL, &fn);
    napi_set_named_property(env, exports, "parseStreamPacket", fn);

    napi_create_function(env, NULL, 0, ValidateSequence, NULL, &fn);
    napi_set_named_property(env, exports, "validateSequence", fn);

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
