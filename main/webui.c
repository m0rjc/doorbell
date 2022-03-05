#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "esp_log.h"
#include "esp_http_server.h"

#define ROOT_PAGE_BUFFER_SIZE 10240

#define MIN(a,b) ((a)<(b)?(a):(b))

/** If we know it's [0-9A-Fa-f] convert it to a hex value */
#define READHEX(c) (c&0x0F + (c&0xF0==0x30 ? 0 : 9 ))

static const char *TAG="webui.c";
static const char *HDR_CONTENT_TYPE = "Content-Type";
static httpd_handle_t server_handle;

static const char *html_header = 
    "<html><head><title>ESP32 Network Doorbell %s</title></head>"
    "<body><h1>ESP32 Network Doorbell %s</h1>";

static const char *html_config_form =
    "<h2>WiFi</h2>"
    "<form action=\"config\" method=\"POST\">"
    "<label for=\"fname\"Name (alphanumeric):</label>"
    "<input type=\"text\" id=\"fname\" name=\"n\" maxlength=\"20\" required pattern=\"[A-Za-z0-9 _]\">%s<br>"
    "<label for=\"fssid\">WiFi Network:</label>"
    "<input type=\"text\" id=\"fssid\" name=\"s\" maxlength=\"40\" required>%s<br>"
    "<label for=\"fpass\">Password:</label>"
    "<input type=\"text\" id=\"fpass\" name=\"p\" maxlength=\"40\" placeholder=\"unchaged\"><br>"
    "<input type=\"submit\" value=\"save\">"
    "</form>";

typedef struct {
    char *key;
    char *value;
} parameter_t;

/**
 * @brief Split a FORM Encoded input into parameters.
 * 
 * @param input input string null terminated. Will be modified to insert nulls
 * @param result buffer for result pointers
 * @param max_parameters number of entries in the result buffer
 * @return int the number of parameters found. Will be max_parameters+1 if more were found.
 */
static int split_parameters(char *input, parameter_t *result, int max_parameters) {
    memset(result, 0, max_parameters * sizeof(parameter_t));
    result[0].key = input;
    int current_index = 0;

    for(char *ptr = input; *ptr != 0; ptr++) {
        if(*ptr == '=') {
            *ptr = 0;
            ptr++;
            result[current_index].value = ptr;
        } else if(*ptr == '&') {
            *ptr = 0;
            ptr++;
            current_index++;
            if(current_index >= max_parameters) break;
            result[current_index].key = ptr;
        }
    }

    if(current_index < max_parameters && result[current_index].value == NULL) {
        result[current_index].key = NULL;
        current_index--;
    }

    return current_index + 1;
}

/**
 * @brief URL Decode a string by interpreting %ab values within it
 * 
 * @param output buffer for result
 * @param input input string
 * @param len  length of the buffer
 * @return int number of characters output or -1 for overflow or format error
 */
static int decode_url_encoded(char *output, const char *input, size_t len) {
    int destIndex = 0;
    char hex[3];
    for(char *ptr = input; ptr != 0 && destIndex < len; ptr++) {
        if(*ptr == '%') {
            hex[0] = *(++ptr);
            hex[1] = *(++ptr);
            hex[2] = 0;
            if(!isxdigit(hex[0]) || !isxdigit(hex[1])) return -1;
            output[destIndex++] = 16*READHEX(hex[0]) + READHEX(hex[1]);
        } else {
            output[destIndex++] = *ptr;
        }
    }
    if(destIndex < len) {
        output[destIndex++] = 0;
        return destIndex;
    }
    return -1;
}

/**
 * @brief Append src to dest if it fits within len.
 * Leave dest pointing to the null terminator.
 * 
 * @param dest destination pointer, mutable, points at the existing null terminator or start of new string
 * @param len reminaing available space
 * @param src source string null terminated
 * @return int new remaining space for appending overwriting the terminator, or -1 if failed.
 */
static int safe_append(char **dest, size_t len, const char *src) {
    int src_len = strlen(src);
    if(src_len >= len-1) return -1;
    memcpy(*dest, src, src_len+1);
    *dest += src_len;
    return len-src_len;
}

/**
 * @brief HTML escape an input string
 * 
 * @param dest destination pointer, mutable
 * @param len reminaing available space
 * @param src source string null terminated
 * @return int new remaining space for appending overwriting the terminator, or -1 if failed.
 */
static int html_escape_append(char **dest, const char *input, size_t remaining) {
    for(char *src = input; *src != 0 && remaining > 0; src++) {
        switch (*src) {
            case '&':
                remaining = safe_append(dest, remaining, "&amp;");
                break;
            case '"':
                remaining = safe_append(dest, remaining, "&quot;");
                break;
            case '<':
                remaining = safe_append(dest, remaining, "&lt;");
                break;
            case '>':
                remaining = safe_append(dest, remaining, "&gt;");
                break;
            default:
                **dest = *src;
                (*dest)++;
                remaining--;
        }
    }
    return remaining;
}

static esp_err_t get_handler(httpd_req_t *req) {
    char *buffer = malloc(ROOT_PAGE_BUFFER_SIZE);
    if(buffer == NULL) {
        httpd_resp_send_err(req, 500, "Unable to allocate memory for response");
        return ESP_FAIL;
    }

    strcpy(buffer, html_header);
    strncat(buffer, html_config_form, ROOT_PAGE_BUFFER_SIZE - strlen(buffer) - 1);

    if(strlen(buffer) >= ROOT_PAGE_BUFFER_SIZE-1) {
        free(buffer);
        httpd_resp_send_err(req, 500, "Response buffer overflow");
        return ESP_FAIL;
    }

    httpd_resp_set_hdr(req, HDR_CONTENT_TYPE, "text/html");
    httpd_resp_send(req, buffer, HTTPD_RESP_USE_STRLEN);
    free(buffer);
    return ESP_OK;
}

static httpd_uri_t uri_root_get = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = get_handler,
    .user_ctx = NULL
};

esp_err_t config_post_handler(httpd_req_t *req)
{
    char content[100];
    size_t recv_size = MIN(req->content_len, sizeof(content)-1);

    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {  /* 0 return value indicates connection closed */
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    if (ret >= sizeof(content)-1) {
        httpd_resp_send_err(req, 413, "Requst too large");
        return ESP_FAIL;
    }

    // Split the input into key value pairs
    content[ret] = 0;
    parameter_t parameters[2];
    int found_parameters = split_parameters(content, parameters, 2);
    if(found_parameters != 2) {
        httpd_resp_send_err(req, 413, "Wrong number of parameters");
        return ESP_FAIL;
    }
    
    char output[1024];
    snprintf(output, sizeof(output), "%s = %s\n%s = %s\n", parameters[0].key, parameters[0].value, parameters[1].key, parameters[1].value);
    /* Send a simple response */
    httpd_resp_set_hdr(req, HDR_CONTENT_TYPE, "text/plain");
    httpd_resp_send(req, output, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static httpd_uri_t uri_post = {
    .uri      = "/config",
    .method   = HTTP_POST,
    .handler  = config_post_handler,
    .user_ctx = NULL
};

void webui_start() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    if(httpd_start(&server_handle, &config) == ESP_OK) {
        httpd_register_uri_handler(server_handle, &uri_root_get);
        httpd_register_uri_handler(server_handle, &uri_post);
        ESP_LOGI(TAG, "Web server running");
    }
}

void webui_stop() {
    if(server_handle) {
        httpd_stop(server_handle);
        server_handle = NULL;
        ESP_LOGI(TAG, "Web server stopped");
    }
}
