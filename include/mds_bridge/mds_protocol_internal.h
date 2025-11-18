/**
 * @file mds_protocol_internal.h
 * @brief Internal MDS Protocol APIs for FFI Bindings
 *
 * This header contains internal APIs that are primarily used by FFI language
 * bindings (Python ctypes, Node.js ffi-napi, etc.) for event-driven I/O.
 *
 * These APIs expose low-level protocol details and should NOT be used by
 * typical C applications. Instead, use the high-level APIs in mds_protocol.h.
 *
 * Why these exist:
 * - FFI libraries (hidapi, node-hid) use non-blocking, event-driven I/O
 * - The main C API uses blocking reads via the backend interface
 * - These buffer-based parsing functions bridge the impedance mismatch
 * - They allow FFI code to parse packets received via callbacks/events
 */

#ifndef MDS_BRIDGE_MDS_PROTOCOL_INTERNAL_H
#define MDS_BRIDGE_MDS_PROTOCOL_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mds_protocol.h"

/* ============================================================================
 * Helper Functions
 * ========================================================================== */

/**
 * @brief Extract sequence number from packet byte 0
 *
 * Helper for extracting sequence from the first byte of a stream packet.
 *
 * @param byte0 First byte of stream packet
 *
 * @return Sequence number (0-31)
 */
static inline uint8_t mds_extract_sequence(uint8_t byte0) {
    return byte0 & MDS_SEQUENCE_MASK;
}

#ifdef __cplusplus
}
#endif

#endif /* MDS_BRIDGE_MDS_PROTOCOL_INTERNAL_H */
