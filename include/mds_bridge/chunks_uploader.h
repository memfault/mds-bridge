/**
 * @file chunks_uploader.h
 * @brief HTTP uploader for Memfault diagnostic chunks
 *
 * This header provides a ready-to-use HTTP uploader built on libcurl.
 *
 * Usage:
 * 1. Create an uploader: chunks_uploader_t *uploader = chunks_uploader_create();
 * 2. Set it on the session: mds_set_upload_callback(session, chunks_uploader_callback, uploader);
 * 3. Process streams: mds_stream_process(session, &config, timeout);
 * 4. Destroy when done: chunks_uploader_destroy(uploader);
 */

#ifndef MDS_BRIDGE_CHUNKS_UPLOADER_H
#define MDS_BRIDGE_CHUNKS_UPLOADER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief Opaque handle to an HTTP uploader
 */
typedef struct chunks_uploader chunks_uploader_t;

/**
 * @brief Upload statistics
 */
typedef struct {
    /** Total chunks uploaded successfully */
    size_t chunks_uploaded;

    /** Total bytes uploaded */
    size_t bytes_uploaded;

    /** Number of upload failures */
    size_t upload_failures;

    /** Last HTTP status code */
    long last_http_status;
} chunks_upload_stats_t;

/**
 * @brief Create an HTTP uploader
 *
 * Creates an uploader instance using libcurl for HTTP POST requests.
 * The uploader can be used as a callback with mds_set_upload_callback().
 *
 * @return Uploader handle, or NULL on failure
 */
chunks_uploader_t *chunks_uploader_create(void);

/**
 * @brief Destroy an HTTP uploader
 *
 * Frees all resources associated with the uploader.
 *
 * @param uploader Uploader handle to destroy
 */
void chunks_uploader_destroy(chunks_uploader_t *uploader);

/**
 * @brief Upload callback for use with mds_set_upload_callback()
 *
 * This function can be passed directly to mds_set_upload_callback().
 * The user_data parameter should be a chunks_uploader_t* instance.
 *
 * Example:
 *   chunks_uploader_t *uploader = chunks_uploader_create();
 *   mds_set_upload_callback(session, chunks_uploader_callback, uploader);
 *
 * @param uri Data URI to POST to
 * @param auth_header Authorization header (format: "HeaderName:HeaderValue")
 * @param chunk_data Chunk data bytes
 * @param chunk_len Length of chunk data
 * @param user_data Must be a chunks_uploader_t* instance
 *
 * @return 0 on success, negative error code on failure
 */
int chunks_uploader_callback(const char *uri,
                              const char *auth_header,
                              const uint8_t *chunk_data,
                              size_t chunk_len,
                              void *user_data);

/**
 * @brief Get upload statistics
 *
 * Returns statistics about uploads performed by this uploader.
 *
 * @param uploader Uploader handle
 * @param stats Pointer to receive statistics
 *
 * @return 0 on success, negative error code otherwise
 */
int chunks_uploader_get_stats(chunks_uploader_t *uploader,
                               chunks_upload_stats_t *stats);

/**
 * @brief Reset upload statistics
 *
 * Resets the upload statistics counters to zero.
 *
 * @param uploader Uploader handle
 *
 * @return 0 on success, negative error code otherwise
 */
int chunks_uploader_reset_stats(chunks_uploader_t *uploader);

/**
 * @brief Set HTTP timeout for uploads
 *
 * Sets the timeout for HTTP requests. Default is 30 seconds.
 *
 * @param uploader Uploader handle
 * @param timeout_ms Timeout in milliseconds
 *
 * @return 0 on success, negative error code otherwise
 */
int chunks_uploader_set_timeout(chunks_uploader_t *uploader,
                                 long timeout_ms);

/**
 * @brief Enable/disable verbose output
 *
 * When enabled, prints detailed information about HTTP requests.
 *
 * @param uploader Uploader handle
 * @param verbose true to enable verbose output, false to disable
 *
 * @return 0 on success, negative error code otherwise
 */
int chunks_uploader_set_verbose(chunks_uploader_t *uploader,
                                 bool verbose);

#ifdef __cplusplus
}
#endif

#endif /* MDS_BRIDGE_CHUNKS_UPLOADER_H */
