#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "esp_log.h"
#include "esp_http_server.h"

#include "web/formparams.h"
#include "web/stringbuilder.h"
#include "nvs.h"
#include "webui.h"
#include "dipswitches.h"
#include "peers.h"
#include "common.h"

#define ROOT_PAGE_BUFFER_SIZE 10240

#define MIN(a,b) ((a)<(b)?(a):(b))

static const char *TAG="webui.c";
static const char *HDR_CONTENT_TYPE = "Content-Type";
static httpd_handle_t server_handle;

static const char *html_config_form =
    "<h2>WiFi</h2>"
    "<form method=\"POST\">"
    "<div class=\"config_fields\">"
    "<label for=\"fname\">Name:</label>"
    "<input type=\"text\" id=\"fname\" name=\"n\" maxlength=\"%d\" required value=\"%s\">"
    "<label for=\"fssid\">WiFi Network:</label>"
    "<input type=\"text\" id=\"fssid\" name=\"s\" maxlength=\"40\" required value=\"%s\">"
    "<label for=\"fpass\">Password:</label>"
    "<input type=\"text\" id=\"fpass\" name=\"p\" maxlength=\"40\" placeholder=\"unchaged\">"
    "</div>"
    "<input type=\"submit\" value=\"save\">"
    "</form>";

static void report_status(string_builder_t *sb) {
    sb_append(sb, "<H2>Status</H2>"
        "<table><tr><th>Node</th><th>Address</th><th>Ringer</th><th>Button</th><th>Last Seen</th></tr>");
    
    sb_append(sb, "<tr class=\"myself\"><th>");
    sb_append_htmlescape(sb, m0rjc_config.name);
    sb_appendf(sb, "</th><td>(todo)</td><td>%c</td><td>%c</td><td>This Node</td></tr>",
        DIP_HAS_RINGER ? 'Y' : '-',
        DIP_HAS_BUTTON ? 'Y' : '-');

    uint64_t now = esp_timer_get_time() + 500000; // Rounding offset when truncating
    for(int i = 0; i < MAX_PEERS; i++) {
        if(peer_infos[i].is_active) {
            sb_append(sb, "<tr><th>");
            sb_append_htmlescape(sb, peer_infos[i].name);
            sb_appendf(sb, "</th><td>"MACSTR"</td><td>%c</td><td>%c</td><td>%d seconds</td></tr>",
                MAC2STR(peer_infos[i].node_id),
                (peer_infos[i].node_flags & NODE_FLAG_HAS_RINGER) ? 'Y' : '-',
                (peer_infos[i].node_flags & NODE_FLAG_HAS_BUTTON) ? 'Y' : '-',
                (int)((now - peer_infos[i].last_seen_time)/1000000)
            );
        }
    }
    sb_append(sb, "</table>");
}

static esp_err_t get_handler(httpd_req_t *req) {
    size_t escapedNameSize = sb_predict_escaped_length(m0rjc_config.name) + 1;
    size_t escapeSSIDSize = sb_predict_escaped_length(m0rjc_config.ssid) + 1;
    string_builder_t *buffers = sb_new_multiple(3, ROOT_PAGE_BUFFER_SIZE, escapedNameSize, escapeSSIDSize);

    if(buffers == NULL) {
        httpd_resp_send_err(req, 500, "Unable to allocate memory for response");
        return ESP_FAIL;
    }

    sb_append_htmlescape(buffers+1, m0rjc_config.name);
    assert(sb_isok(buffers + 1));
    char *escaped_name = buffers[1].buffer;

    sb_append_htmlescape(buffers+2, m0rjc_config.ssid);
    assert(sb_isok(buffers + 2));
    char *escaped_ssid = buffers[2].buffer;

    sb_appendf(buffers, "<html><head>"
        "<link rel=\"stylesheet\" href=\"style.css\">"
        "<title>ESP32 Network Doorbell %s</title></head>", escaped_name);
    sb_appendf(buffers, "<body><h1>ESP32 Network Doorbell %s</h1>", escaped_name);

    if(DIP_IS_MODE_RUN) {
        report_status(buffers);
    }

    if(DIP_IS_MODE_CONFIG) {
        if(req->sess_ctx != NULL) {
            int *ctx = (int *)req->sess_ctx;
            if(*ctx == 1) {
                sb_append(buffers, "<p class=\"msg_ok\">Configuration saved.</p>");
                *ctx = 0;
            }
        }
        sb_appendf(buffers, html_config_form, NODE_NAME_LEN, escaped_name, escaped_ssid);
    }

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

esp_err_t css_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/css");
    httpd_resp_sendstr(req, 
    "body { background: white; foreground: black;}\n"
    ".msg_ok { background: aquamarine; padding: 2px; border-radius: 2px; }\n"
    ".msg_err { background: pink; padding: 2px; border-radius: 2px; }\n"
    ".config_fields { display: grid; column-gap: 5px; row-gap: 2px; grid-template-columns: max-content max-content; padding: 5px }\n"
    ".config_fields > label { font: bold; text-align: right; }\n"
    );
    return ESP_OK;
}

static httpd_uri_t uri_css_get = {
    .uri = "/style.css",
    .method = HTTP_GET,
    .handler = css_get_handler,
    .user_ctx = NULL
};

esp_err_t config_post_handler(httpd_req_t *req)
{
    if(!DIP_IS_MODE_CONFIG) {
        httpd_resp_send_err(req, 403, "Forbidden. Use the dip switches to enable configuration");
        return ESP_OK;
    }

    char content[256];
    size_t recv_size = MIN(req->content_len, sizeof(content));

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
    form_parameter_t parameters[3];
    int found_parameters = read_form_parameters(content, parameters, 3);
    if(found_parameters > 3) {
        httpd_resp_send_err(req, 413, "Too many parameters");
        return ESP_FAIL;
    }
    
    m0rjc_config_t config;
    memset(&config, 0, sizeof(m0rjc_config_t));
    for(int i = 0; i < found_parameters; i++) {
        form_parameter_t *param = parameters + i;
        // Non-empty value and single letter key
        if(strlen(param->value) > 0 && strlen(param->key) == 1) {
            switch (*param->key) {
                case 'n':
                    // If it's too long just truncate it.
                    if(strlen(param->value) > NODE_NAME_LEN) param->value[NODE_NAME_LEN] = 0;
                    config.name = param->value;
                    break;
                case 's':
                    config.ssid = param->value;
                    break;
                case 'p':
                    config.password = param->value;
                    break;
            }
        }
    }
    write_config(&config);

    // Use POST-REDIRECT-GET pattern to load the main page again.
    
    if(req->sess_ctx == NULL) {
        req->sess_ctx = malloc(sizeof(int));
        if(req->sess_ctx != NULL) {
            *((int *)req->sess_ctx) = 1;
        }
    }
    httpd_resp_set_status(req, "303 Saved");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_sendstr(req, "");
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
        httpd_register_uri_handler(server_handle, &uri_css_get);
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
