#include "nvs.h"
#include "nvs_flash.h"

static const char *NAMESPACE = "m0rjc";
static const char *NVS_KEY_SSID = "ssid";
static const char *NVS_KEY_PASSWORD = "pass";
static const char *NVS_KEY_NAME = "name";
static const char *NVS_KEY_HAS_CONNECTED = "setup_ok";

static nvs_handle_t m0rjc_nvs_handle;
m0rjc_config_t m0rjc_config;
static char* config_buffer = NULL;

static void read_config() {
    size_t name_size;
    size_t ssid_size;
    size_t password_size;

    if(config_buffer != NULL) free(config_buffer);
    esp_err_t name_qry_rslt = nvs_get_str(m0rjc_nvs_handle, NVS_KEY_NAME, NULL, &name_size);
    if(name_qry_rslt != ESP_OK) name_size = 1;
    esp_err_t ssid_qry_rslt = nvs_get_str(m0rjc_nvs_handle, NVS_KEY_SSID, NULL, &ssid_size);
    if(ssid_qry_rslt != ESP_OK) ssid_size = 1;
    esp_err_t pass_qry_rslt = nvs_get_str(m0rjc_nvs_handle, NVS_KEY_PASSWORD, NULL, &password_size);
    if(pass_qry_rslt != ESP_OK) password_size = 1;

    config_buffer = malloc(name_size + ssid_size + password_size);
    assert(config_buffer != NULL);

    m0rjc_config.name = config_buffer;
    m0rjc_config.password = config_buffer + name_size;
    m0rjc_config.ssid = config_buffer + name_size + password_size;

    if(name_qry_rslt == ESP_OK) {
        nvs_get_str(m0rjc_nvs_handle, NVS_KEY_NAME, m0rjc_config.name, &name_size);
    } else {
        *m0rjc_config.name = 0;
    }

    if(ssid_qry_rslt == ESP_OK) {
        nvs_get_str(m0rjc_nvs_handle, NVS_KEY_SSID, m0rjc_config.ssid, &ssid_size);
    } else {
        *m0rjc_config.ssid = 0;
    }

    if(pass_qry_rslt == ESP_OK) {
        nvs_get_str(m0rjc_nvs_handle, NVS_KEY_PASSWORD, m0rjc_config.password, &password_size);
    } else {
        *m0rjc_config.password = 0;
    }
}

void initialise_nvs() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    ESP_ERROR_CHECK(nvs_open(NAMESPACE, NVS_READWRITE, &m0rjc_nvs_handle));

    read_config();
}

void write_config(m0rjc_config_t *config) {
    if(config->name != NULL) {
        nvs_set_str(m0rjc_nvs_handle, NVS_KEY_NAME, config->name);
    }

    if(config->ssid != NULL) {
        nvs_set_str(m0rjc_nvs_handle, NVS_KEY_SSID, config->ssid);
    }

    if(config->password != NULL) {
        nvs_set_str(m0rjc_nvs_handle, NVS_KEY_PASSWORD, config->password);
    }

    read_config();
}