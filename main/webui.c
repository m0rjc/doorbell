#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "esp_log.h"
#include "esp_http_server.h"

#include "web/formparams.h"
#include "web/stringbuilder.h"
#include "webui.h"

#define ROOT_PAGE_BUFFER_SIZE 10240

#define MIN(a,b) ((a)<(b)?(a):(b))

static const char *TAG="webui.c";
static const char *HDR_CONTENT_TYPE = "Content-Type";
static httpd_handle_t server_handle;


static const char *html_config_form =
    "<h2>WiFi</h2>"
    "<form method=\"POST\">"
    "<label for=\"fname\">Name (alphanumeric):</label>"
    "<input type=\"text\" id=\"fname\" name=\"n\" maxlength=\"20\" required pattern=\"[A-Za-z0-9 _]+\" value=\"%s\"><br>"
    "<label for=\"fssid\">WiFi Network:</label>"
    "<input type=\"text\" id=\"fssid\" name=\"s\" maxlength=\"40\" required value=\"%s\"><br>"
    "<label for=\"fpass\">Password:</label>"
    "<input type=\"text\" id=\"fpass\" name=\"p\" maxlength=\"40\" placeholder=\"unchaged\"><br>"
    "<input type=\"submit\" value=\"save\">"
    "</form>";


static esp_err_t get_handler(httpd_req_t *req) {
    char *myName = "Sample Name";
    char *ssid = "My SSID";

    size_t escapedNameSize = sb_predict_escaped_length(myName) + 1;
    size_t escapeSSIDSize = sb_predict_escaped_length(ssid) + 1;
    string_builder_t *buffers = sb_new_multiple(3, ROOT_PAGE_BUFFER_SIZE, escapedNameSize, escapeSSIDSize);

    if(buffers == NULL) {
        httpd_resp_send_err(req, 500, "Unable to allocate memory for response");
        return ESP_FAIL;
    }

    sb_append_htmlescape(buffers+1, myName);
    assert(sb_isok(buffers + 1));
    char *escaped_name = buffers[1].buffer;

    sb_append_htmlescape(buffers+2, ssid);
    assert(sb_isok(buffers + 2));
    char *escaped_ssid = buffers[2].buffer;

    sb_appendf(buffers, "<html><head><title>ESP32 Network Doorbell %s</title></head>", escaped_name);
    sb_appendf(buffers, "<body><h1>ESP32 Network Doorbell %s</h1>", escaped_name);

    // If config
    sb_appendf(buffers, html_config_form, escaped_name, escaped_ssid);

    sb_append(buffers, "</body></html>");

    if(!sb_isok(buffers)) {
        free(buffers);
        httpd_resp_send_err(req, 500, "Response buffer overflow");
        return ESP_FAIL;
    }

    httpd_resp_set_hdr(req, HDR_CONTENT_TYPE, "text/html");
    httpd_resp_send(req, buffers[0].buffer, HTTPD_RESP_USE_STRLEN);
    free(buffers);
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
    string_builder_t *buffer = sb_new_multiple(1, 2048);
    if(buffer == NULL) {
        httpd_resp_send_err(req, 500, "Cannot allocate space for response");
        return ESP_FAIL;
    }

    char content[256];
    size_t recv_size = MIN(req->content_len, sizeof(content)-1);

    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {  /* 0 return value indicates connection closed */
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    if (ret >= sizeof(content)-1) {
        httpd_resp_send_err(req, 413, "Request too large");
        return ESP_FAIL;
    }

    // Split the input into key value pairs
    content[ret] = 0;
    sb_append(buffer, "RAW: ");
    sb_append(buffer, content);
    sb_append(buffer, "\n\n");

    form_parameter_t parameters[3];
    int found_parameters = split_parameters(content, parameters, 3);
    if(found_parameters != 3) {
        httpd_resp_send_err(req, 413, "Wrong number of parameters");
        return ESP_FAIL;
    }
    
    for(int i = 0; i < 3; i++) {
        char decoded[100];
        decode_url_encoded(decoded, parameters[i].value, sizeof(decoded));
        decoded[99] = 0;
        sb_append(buffer, parameters[i].key);
        sb_append(buffer, " => ");
        sb_append(buffer, decoded);
        sb_append(buffer, "\n");
    }
    /* Send a simple response */
    httpd_resp_set_hdr(req, HDR_CONTENT_TYPE, "text/plain");
    httpd_resp_send(req, buffer->buffer, HTTPD_RESP_USE_STRLEN);
    free(buffer);
    return ESP_OK;
}

static httpd_uri_t uri_post = {
    .uri      = "/",
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
