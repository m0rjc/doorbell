#pragma once
#include "nvs_flash.h"

typedef struct  {
    char *name;
    char *ssid;
    char *password;
} m0rjc_config_t;

extern m0rjc_config_t m0rjc_config;

void initialise_nvs();

/**
 * @brief Write any non-null values into config
 * Perform a re-read so updating the m0rjc_config structure.
 * 
 * @param config complete or partial config to write.
 */
void write_config(m0rjc_config_t *config);

