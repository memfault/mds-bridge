/**
 * @file mock_libcurl.c
 * @brief Mock implementation of libcurl for testing MDS upload functionality
 *
 * This mock simulates HTTP POST requests and tracks upload statistics.
 */

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

/* Mock state */
typedef struct {
    char last_url[512];
    char last_headers[1024];
    uint8_t last_data[512];
    size_t last_data_len;
    long response_code;
    CURLcode error_code;
    int request_count;
    bool verbose;
} mock_curl_state_t;

static mock_curl_state_t mock_state = {0};

/* Reset mock state */
void mock_curl_reset(void) {
    memset(&mock_state, 0, sizeof(mock_state));
    mock_state.response_code = 200;  /* Default to success */
    mock_state.error_code = CURLE_OK;
}

/* Set mock response */
void mock_curl_set_response(long http_code, CURLcode error) {
    mock_state.response_code = http_code;
    mock_state.error_code = error;
}

/* Get mock statistics */
int mock_curl_get_request_count(void) {
    return mock_state.request_count;
}

const char* mock_curl_get_last_url(void) {
    return mock_state.last_url;
}

const uint8_t* mock_curl_get_last_data(size_t *len) {
    *len = mock_state.last_data_len;
    return mock_state.last_data;
}

/* ============================================================================
 * Mock libcurl API Implementation
 * ========================================================================== */

CURL *curl_easy_init(void) {
    printf("[MOCK CURL] curl_easy_init()\n");
    /* Return a non-NULL pointer (not actually used, just needs to be non-NULL) */
    static int dummy = 0;
    return (CURL *)&dummy;
}

void curl_easy_cleanup(CURL *curl) {
    printf("[MOCK CURL] curl_easy_cleanup(%p)\n", curl);
    (void)curl;
}

void curl_easy_reset(CURL *curl) {
    if (mock_state.verbose) {
        printf("[MOCK CURL] curl_easy_reset(%p)\n", curl);
    }
    (void)curl;
}

CURLcode curl_easy_setopt(CURL *curl, CURLoption option, ...) {
    va_list args;
    va_start(args, option);

    switch (option) {
        case CURLOPT_URL: {
            const char *url = va_arg(args, const char *);
            if (mock_state.verbose) {
                printf("[MOCK CURL] curl_easy_setopt(CURLOPT_URL, %s)\n", url);
            }
            strncpy(mock_state.last_url, url, sizeof(mock_state.last_url) - 1);
            break;
        }
        case CURLOPT_POST: {
            long post = va_arg(args, long);
            if (mock_state.verbose) {
                printf("[MOCK CURL] curl_easy_setopt(CURLOPT_POST, %ld)\n", post);
            }
            break;
        }
        case CURLOPT_POSTFIELDS: {
            const void *data = va_arg(args, const void *);
            if (mock_state.verbose) {
                printf("[MOCK CURL] curl_easy_setopt(CURLOPT_POSTFIELDS, %p)\n", data);
            }
            /* Store pointer but don't copy yet - wait for POSTFIELDSIZE */
            break;
        }
        case CURLOPT_POSTFIELDSIZE: {
            long size = va_arg(args, long);
            if (mock_state.verbose) {
                printf("[MOCK CURL] curl_easy_setopt(CURLOPT_POSTFIELDSIZE, %ld)\n", size);
            }
            /* Note: In real usage, POSTFIELDS is set before POSTFIELDSIZE */
            /* We'll capture the data in curl_easy_perform */
            break;
        }
        case CURLOPT_HTTPHEADER: {
            struct curl_slist *headers = va_arg(args, struct curl_slist *);
            if (mock_state.verbose) {
                printf("[MOCK CURL] curl_easy_setopt(CURLOPT_HTTPHEADER, %p)\n", headers);
            }
            /* Store headers for verification */
            mock_state.last_headers[0] = '\0';
            struct curl_slist *h = headers;
            while (h) {
                strncat(mock_state.last_headers, h->data, sizeof(mock_state.last_headers) - strlen(mock_state.last_headers) - 1);
                strncat(mock_state.last_headers, ";", sizeof(mock_state.last_headers) - strlen(mock_state.last_headers) - 1);
                h = h->next;
            }
            break;
        }
        case CURLOPT_TIMEOUT_MS: {
            long timeout = va_arg(args, long);
            if (mock_state.verbose) {
                printf("[MOCK CURL] curl_easy_setopt(CURLOPT_TIMEOUT_MS, %ld)\n", timeout);
            }
            break;
        }
        case CURLOPT_VERBOSE: {
            long verbose = va_arg(args, long);
            if (verbose) {
                printf("[MOCK CURL] curl_easy_setopt(CURLOPT_VERBOSE, %ld)\n", verbose);
            }
            mock_state.verbose = (verbose != 0);
            break;
        }
        default:
            if (mock_state.verbose) {
                printf("[MOCK CURL] curl_easy_setopt(%d, ...)\n", option);
            }
            break;
    }

    va_end(args);
    return CURLE_OK;
}

/* Note: This is a simplified mock - in reality we'd need to track the POST data pointer */
static const void *g_last_post_data = NULL;
static long g_last_post_size = 0;

CURLcode curl_easy_setopt_data(CURL *curl, const void *data, long size) {
    g_last_post_data = data;
    g_last_post_size = size;
    return CURLE_OK;
}

CURLcode curl_easy_perform(CURL *curl) {
    mock_state.request_count++;

    printf("[MOCK CURL] curl_easy_perform() - Request #%d\n", mock_state.request_count);
    printf("[MOCK CURL]   URL: %s\n", mock_state.last_url);
    printf("[MOCK CURL]   HTTP Code: %ld\n", mock_state.response_code);

    /* In a real implementation, we'd parse and execute the request */
    /* For the mock, we just return the pre-configured response */

    (void)curl;
    return mock_state.error_code;
}

CURLcode curl_easy_getinfo(CURL *curl, CURLINFO info, ...) {
    va_list args;
    va_start(args, info);

    switch (info) {
        case CURLINFO_RESPONSE_CODE: {
            long *code = va_arg(args, long *);
            *code = mock_state.response_code;
            if (mock_state.verbose) {
                printf("[MOCK CURL] curl_easy_getinfo(CURLINFO_RESPONSE_CODE) -> %ld\n", *code);
            }
            break;
        }
        default:
            if (mock_state.verbose) {
                printf("[MOCK CURL] curl_easy_getinfo(%d, ...)\n", info);
            }
            break;
    }

    va_end(args);
    (void)curl;
    return CURLE_OK;
}

const char *curl_easy_strerror(CURLcode error) {
    switch (error) {
        case CURLE_OK:
            return "No error";
        case CURLE_COULDNT_CONNECT:
            return "Couldn't connect";
        case CURLE_OPERATION_TIMEDOUT:
            return "Operation timed out";
        default:
            return "Unknown error";
    }
}

struct curl_slist *curl_slist_append(struct curl_slist *list, const char *string) {
    if (mock_state.verbose) {
        printf("[MOCK CURL] curl_slist_append(%p, \"%s\")\n", list, string);
    }

    struct curl_slist *new_item = malloc(sizeof(struct curl_slist));
    if (!new_item) {
        return list;
    }

    new_item->data = strdup(string);
    new_item->next = NULL;

    if (!list) {
        return new_item;
    }

    /* Append to end of list */
    struct curl_slist *current = list;
    while (current->next) {
        current = current->next;
    }
    current->next = new_item;

    return list;
}

void curl_slist_free_all(struct curl_slist *list) {
    if (mock_state.verbose) {
        printf("[MOCK CURL] curl_slist_free_all(%p)\n", list);
    }

    while (list) {
        struct curl_slist *next = list->next;
        free(list->data);
        free(list);
        list = next;
    }
}

/* Global init/cleanup (not typically used in our tests) */
CURLcode curl_global_init(long flags) {
    printf("[MOCK CURL] curl_global_init(%ld)\n", flags);
    (void)flags;
    return CURLE_OK;
}

void curl_global_cleanup(void) {
    printf("[MOCK CURL] curl_global_cleanup()\n");
}
