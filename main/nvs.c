#include "nvs.h"
#include "nvs_flash.h"

static const char *NAMESPACE = "m0rjc";

const char *NVS_KEY_SSID = "ssid";
const char *NVS_KEY_PASSWORD = "pass";
const char *NVS_KEY_NAME = "name";
const char *NVS_KEY_HAS_CONNECTED = "setup_ok";

nvs_handle_t m0rjc_nvs_handle;

void initialise_nvs() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    ESP_ERROR_CHECK(nvs_open(NAMESPACE, NVS_READWRITE, &m0rjc_nvs_handle));
}