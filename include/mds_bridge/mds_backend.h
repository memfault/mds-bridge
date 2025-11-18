#ifndef MDS_BRIDGE_MDS_BACKEND_H
#define MDS_BRIDGE_MDS_BACKEND_H

#include <stddef.h>
#include <stdint.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file mds_backend.h
 * @brief MDS Backend Interface - Transport abstraction layer for MDS protocol
 *
 * This interface allows the MDS protocol to work with different transport
 * mechanisms (HID, Serial, BLE, etc.) by providing a simple READ/WRITE API.
 *
 * Backend implementers must provide:
 * - read(): Read a report from the device (handles both feature and input reports)
 * - write(): Write a report to the device (handles feature SET operations)
 * - destroy(): Clean up backend resources
 *
 * The report_id parameter determines the type of operation:
 * - For HID: report_id maps to HID report IDs (feature vs input determined by context)
 * - For Serial: report_id used as protocol framing byte
 * - For BLE: report_id maps to GATT characteristics
 */

/**
 * Opaque backend handle
 */
typedef struct mds_backend mds_backend_t;

/**
 * Backend operation vtable
 *
 * Backend implementations must provide these function pointers.
 */
typedef struct {
    /**
     * Read a report from the device
     *
     * @param impl_data Backend-specific state
     * @param report_id Report ID to read
     * @param buffer Output buffer for report data
     * @param length Maximum bytes to read
     * @param timeout_ms Timeout in milliseconds (-1 for blocking)
     * @return Number of bytes read on success, negative on error
     */
    int (*read)(void *impl_data, uint8_t report_id, uint8_t *buffer,
                size_t length, int timeout_ms);

    /**
     * Write a report to the device
     *
     * @param impl_data Backend-specific state
     * @param report_id Report ID to write
     * @param buffer Report data to write
     * @param length Number of bytes to write
     * @return Number of bytes written on success, negative on error
     */
    int (*write)(void *impl_data, uint8_t report_id, const uint8_t *buffer,
                 size_t length);

    /**
     * Destroy backend and free resources
     *
     * @param impl_data Backend-specific state to clean up
     */
    void (*destroy)(void *impl_data);
} mds_backend_ops_t;

/**
 * Backend instance
 */
struct mds_backend {
    const mds_backend_ops_t *ops;  /**< Operation vtable */
    void *impl_data;                /**< Backend-specific state */
};

/**
 * Read a report from the backend
 *
 * @param backend Backend instance
 * @param report_id Report ID to read
 * @param buffer Output buffer
 * @param length Maximum bytes to read
 * @param timeout_ms Timeout in milliseconds (-1 for blocking)
 * @return Number of bytes read on success, negative on error
 */
static inline int mds_backend_read(mds_backend_t *backend, uint8_t report_id,
                                    uint8_t *buffer, size_t length,
                                    int timeout_ms) {
    assert(backend != NULL && "backend cannot be NULL");
    assert(backend->ops != NULL && "backend->ops cannot be NULL");
    assert(backend->ops->read != NULL && "backend->ops->read cannot be NULL");
    return backend->ops->read(backend->impl_data, report_id, buffer, length, timeout_ms);
}

/**
 * Write a report to the backend
 *
 * @param backend Backend instance
 * @param report_id Report ID to write
 * @param buffer Report data
 * @param length Number of bytes to write
 * @return Number of bytes written on success, negative on error
 */
static inline int mds_backend_write(mds_backend_t *backend, uint8_t report_id,
                                     const uint8_t *buffer, size_t length) {
    assert(backend != NULL && "backend cannot be NULL");
    assert(backend->ops != NULL && "backend->ops cannot be NULL");
    assert(backend->ops->write != NULL && "backend->ops->write cannot be NULL");
    return backend->ops->write(backend->impl_data, report_id, buffer, length);
}

/**
 * Destroy backend and free resources
 *
 * @param backend Backend instance to destroy
 */
static inline void mds_backend_destroy(mds_backend_t *backend) {
    if (backend && backend->ops && backend->ops->destroy) {
        backend->ops->destroy(backend->impl_data);
    }
}

#ifdef __cplusplus
}
#endif

#endif /* MDS_BRIDGE_MDS_BACKEND_H */
