#pragma once
#include "nvs_flash.h"

extern const char *NVS_KEY_SSID;
extern const char *NVS_KEY_PASSWORD;
extern const char *NVS_KEY_NAME;
extern const char *NVS_KEY_HAS_CONNECTED;

extern nvs_handle_t m0rjc_nvs_handle;

void initialise_nvs();