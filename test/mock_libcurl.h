/**
 * @file mock_libcurl.h
 * @brief Mock libcurl control interface for tests
 */

#ifndef MOCK_LIBCURL_H
#define MOCK_LIBCURL_H

#include <stddef.h>
#include <stdint.h>
#include <curl/curl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Reset mock state to defaults
 */
void mock_curl_reset(void);

/**
 * @brief Set the HTTP response code and error for next request
 *
 * @param http_code HTTP status code (e.g., 200, 404, 500)
 * @param error libcurl error code (e.g., CURLE_OK, CURLE_COULDNT_CONNECT)
 */
void mock_curl_set_response(long http_code, CURLcode error);

/**
 * @brief Get number of HTTP requests made
 *
 * @return Number of requests
 */
int mock_curl_get_request_count(void);

/**
 * @brief Get the last URL that was requested
 *
 * @return Last URL string
 */
const char* mock_curl_get_last_url(void);

/**
 * @brief Get the last POST data that was sent
 *
 * @param len Pointer to receive data length
 * @return Pointer to last POST data
 */
const uint8_t* mock_curl_get_last_data(size_t *len);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_LIBCURL_H */
