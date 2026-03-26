/**
 * @file chunks_uploader.c
 * @brief HTTP uploader implementation using libcurl
 */

#include "mds_bridge/chunks_uploader.h"
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdbool.h>

/* Uploader structure */
struct chunks_uploader {
    CURL *curl;
    struct curl_slist *headers;
    chunks_upload_stats_t stats;
    long timeout_ms;
    bool verbose;
};

/* ============================================================================
 * Uploader Management
 * ========================================================================== */

chunks_uploader_t *chunks_uploader_create(void) {
    chunks_uploader_t *uploader = calloc(1, sizeof(chunks_uploader_t));
    if (uploader == NULL) {
        return NULL;
    }

    /* Initialize libcurl */
    uploader->curl = curl_easy_init();
    if (uploader->curl == NULL) {
        free(uploader);
        return NULL;
    }

    /* Set default timeout (30 seconds) */
    uploader->timeout_ms = 30000;
    uploader->verbose = false;

    return uploader;
}

void chunks_uploader_destroy(chunks_uploader_t *uploader) {
    if (uploader == NULL) {
        return;
    }

    if (uploader->headers) {
        curl_slist_free_all(uploader->headers);
    }

    if (uploader->curl) {
        curl_easy_cleanup(uploader->curl);
    }

    free(uploader);
}

/* ============================================================================
 * Upload Callback
 * ========================================================================== */

int chunks_uploader_callback(const char *uri,
                              const char *auth_header,
                              const uint8_t *chunk_data,
                              size_t chunk_len,
                              void *user_data) {
    if (uri == NULL || auth_header == NULL || chunk_data == NULL || user_data == NULL) {
        return -EINVAL;
    }

    chunks_uploader_t *uploader = (chunks_uploader_t *)user_data;
    CURLcode res;

    /* Reset curl for new request */
    curl_easy_reset(uploader->curl);

    /* Set URL */
    curl_easy_setopt(uploader->curl, CURLOPT_URL, uri);

    /* Set POST method */
    curl_easy_setopt(uploader->curl, CURLOPT_POST, 1L);

    /* Set POST data */
    curl_easy_setopt(uploader->curl, CURLOPT_POSTFIELDS, chunk_data);
    curl_easy_setopt(uploader->curl, CURLOPT_POSTFIELDSIZE, (long)chunk_len);

    /* Parse authorization header (format: "HeaderName:HeaderValue") */
    const char *colon = strchr(auth_header, ':');
    if (colon == NULL) {
        fprintf(stderr, "Invalid authorization header format: %s\n", auth_header);
        uploader->stats.upload_failures++;
        return -EINVAL;
    }

    /* Extract header name and value */
    size_t header_name_len = colon - auth_header;
    char *header_name = malloc(header_name_len + 1);
    if (header_name == NULL) {
        uploader->stats.upload_failures++;
        return -ENOMEM;
    }
    memcpy(header_name, auth_header, header_name_len);
    header_name[header_name_len] = '\0';

    const char *header_value = colon + 1;

    /* Build full header string for curl */
    size_t full_header_len = strlen(header_name) + 2 + strlen(header_value) + 1; /* name + ": " + value + \0 */
    char *full_header = malloc(full_header_len);
    if (full_header == NULL) {
        free(header_name);
        uploader->stats.upload_failures++;
        return -ENOMEM;
    }
    snprintf(full_header, full_header_len, "%s: %s", header_name, header_value);

    /* Set headers */
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, full_header);
    headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
    headers = curl_slist_append(headers, "User-Agent: mds-bridge/1.0 (Memfault MDS Gateway)");

    curl_easy_setopt(uploader->curl, CURLOPT_HTTPHEADER, headers);

    /* Set timeout */
    curl_easy_setopt(uploader->curl, CURLOPT_TIMEOUT_MS, uploader->timeout_ms);

    /* Set verbose if enabled */
    if (uploader->verbose) {
        curl_easy_setopt(uploader->curl, CURLOPT_VERBOSE, 1L);

        /* Debug: show exactly what bytes we're uploading */
        printf("[UPLOAD] %zu bytes: ", chunk_len);
        for (size_t i = 0; i < chunk_len && i < 20; i++) {
            printf("%02X ", chunk_data[i]);
        }
        if (chunk_len > 20) {
            printf("...");
        }
        printf("\n");
    }

    /* Perform the request */
    res = curl_easy_perform(uploader->curl);

    /* Get HTTP status code */
    long http_code = 0;
    curl_easy_getinfo(uploader->curl, CURLINFO_RESPONSE_CODE, &http_code);
    uploader->stats.last_http_status = http_code;

    /* Clean up headers */
    curl_slist_free_all(headers);
    free(full_header);
    free(header_name);

    /* Check result */
    if (res != CURLE_OK) {
        fprintf(stderr, "Upload failed: %s\n", curl_easy_strerror(res));
        uploader->stats.upload_failures++;
        return -EIO;
    }

    /* Check HTTP status */
    if (http_code < 200 || http_code >= 300) {
        fprintf(stderr, "Upload failed with HTTP status %ld\n", http_code);
        uploader->stats.upload_failures++;
        return -EIO;
    }

    /* Success - update stats */
    uploader->stats.chunks_uploaded++;
    uploader->stats.bytes_uploaded += chunk_len;

    if (uploader->verbose) {
        printf("Uploaded chunk: %zu bytes, HTTP %ld\n", chunk_len, http_code);
    }

    return 0;
}

/* ============================================================================
 * Batch Upload (multipart/mixed)
 * ========================================================================== */

/* Helper to parse "HeaderName: HeaderValue" into a curl header string */
static char *prv_build_auth_header(const char *auth_header) {
    const char *colon = strchr(auth_header, ':');
    if (colon == NULL) {
        return NULL;
    }

    size_t name_len = colon - auth_header;
    char *name = malloc(name_len + 1);
    if (name == NULL) {
        return NULL;
    }
    memcpy(name, auth_header, name_len);
    name[name_len] = '\0';

    const char *value = colon + 1;
    size_t full_len = strlen(name) + 2 + strlen(value) + 1;
    char *full = malloc(full_len);
    if (full == NULL) {
        free(name);
        return NULL;
    }
    snprintf(full, full_len, "%s: %s", name, value);
    free(name);
    return full;
}

int chunks_uploader_batch_callback(const char *uri,
                                   const char *auth_header,
                                   const uint8_t **chunks,
                                   const size_t *chunk_lens,
                                   size_t num_chunks,
                                   void *user_data) {
    if (uri == NULL || auth_header == NULL || chunks == NULL ||
        chunk_lens == NULL || num_chunks == 0 || user_data == NULL) {
        return -EINVAL;
    }

    /* Single chunk: use simple POST */
    if (num_chunks == 1) {
        return chunks_uploader_callback(uri, auth_header,
                                        chunks[0], chunk_lens[0], user_data);
    }

    chunks_uploader_t *uploader = (chunks_uploader_t *)user_data;

    /* Build auth header */
    char *full_auth = prv_build_auth_header(auth_header);
    if (full_auth == NULL) {
        uploader->stats.upload_failures++;
        return -EINVAL;
    }

    /* Build multipart/mixed body manually (curl mime API forces form-data) */
    static const char *boundary = "mds-bridge-chunk-boundary";

    /* Calculate total body size */
    size_t body_size = 0;
    for (size_t i = 0; i < num_chunks; i++) {
        /* --boundary\r\nContent-Length: NNN\r\n\r\n<data>\r\n */
        body_size += 2 + strlen(boundary) + 2;  /* --boundary\r\n */
        body_size += 32;  /* Content-Length: NNN\r\n (generous) */
        body_size += 2;  /* \r\n */
        body_size += chunk_lens[i];  /* data */
        body_size += 2;  /* \r\n */
    }
    body_size += 2 + strlen(boundary) + 2 + 2;  /* --boundary--\r\n */

    char *body = malloc(body_size);
    if (body == NULL) {
        free(full_auth);
        uploader->stats.upload_failures++;
        return -ENOMEM;
    }

    size_t offset = 0;
    for (size_t i = 0; i < num_chunks; i++) {
        offset += snprintf(body + offset, body_size - offset,
                           "--%s\r\nContent-Length: %zu\r\n\r\n",
                           boundary, chunk_lens[i]);
        memcpy(body + offset, chunks[i], chunk_lens[i]);
        offset += chunk_lens[i];
        memcpy(body + offset, "\r\n", 2);
        offset += 2;
    }
    offset += snprintf(body + offset, body_size - offset, "--%s--\r\n", boundary);

    /* Set up the request */
    curl_easy_reset(uploader->curl);
    curl_easy_setopt(uploader->curl, CURLOPT_URL, uri);
    curl_easy_setopt(uploader->curl, CURLOPT_POST, 1L);
    curl_easy_setopt(uploader->curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(uploader->curl, CURLOPT_POSTFIELDSIZE, (long)offset);
    curl_easy_setopt(uploader->curl, CURLOPT_TIMEOUT_MS, uploader->timeout_ms);

    if (uploader->verbose) {
        curl_easy_setopt(uploader->curl, CURLOPT_VERBOSE, 1L);
    }

    char content_type[128];
    snprintf(content_type, sizeof(content_type),
             "Content-Type: multipart/mixed; boundary=%s", boundary);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, full_auth);
    headers = curl_slist_append(headers, content_type);
    headers = curl_slist_append(headers, "User-Agent: mds-bridge/1.0 (Memfault MDS Gateway)");
    curl_easy_setopt(uploader->curl, CURLOPT_HTTPHEADER, headers);

    /* Perform the upload */
    CURLcode res = curl_easy_perform(uploader->curl);

    long http_code = 0;
    curl_easy_getinfo(uploader->curl, CURLINFO_RESPONSE_CODE, &http_code);
    uploader->stats.last_http_status = http_code;

    curl_slist_free_all(headers);
    free(full_auth);
    free(body);

    if (res != CURLE_OK) {
        fprintf(stderr, "Batch upload failed: %s\n", curl_easy_strerror(res));
        uploader->stats.upload_failures++;
        return -EIO;
    }

    if (http_code < 200 || http_code >= 300) {
        fprintf(stderr, "Batch upload failed with HTTP status %ld\n", http_code);
        uploader->stats.upload_failures++;
        return -EIO;
    }

    /* Success */
    size_t total_bytes = 0;
    for (size_t i = 0; i < num_chunks; i++) {
        total_bytes += chunk_lens[i];
    }
    uploader->stats.chunks_uploaded += num_chunks;
    uploader->stats.bytes_uploaded += total_bytes;

    return 0;
}

/* ============================================================================
 * Statistics
 * ========================================================================== */

int chunks_uploader_get_stats(chunks_uploader_t *uploader,
                               chunks_upload_stats_t *stats) {
    if (uploader == NULL || stats == NULL) {
        return -EINVAL;
    }

    *stats = uploader->stats;
    return 0;
}

int chunks_uploader_reset_stats(chunks_uploader_t *uploader) {
    if (uploader == NULL) {
        return -EINVAL;
    }

    memset(&uploader->stats, 0, sizeof(uploader->stats));
    return 0;
}

/* ============================================================================
 * Configuration
 * ========================================================================== */

int chunks_uploader_set_timeout(chunks_uploader_t *uploader,
                                 long timeout_ms) {
    if (uploader == NULL) {
        return -EINVAL;
    }

    uploader->timeout_ms = timeout_ms;
    return 0;
}

int chunks_uploader_set_verbose(chunks_uploader_t *uploader,
                                 bool verbose) {
    if (uploader == NULL) {
        return -EINVAL;
    }

    uploader->verbose = verbose;
    return 0;
}
