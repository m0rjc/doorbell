#include <stdint.h>

#include "driver/gpio.h"

#include "dipswitches.h"
#include "esp_log.h"

uint8_t dip_switches;

void dip_switchs_init() {
    gpio_config_t ioConfig = {
        // Four input only pins. No pullups so I have to do this myself, though I can stabilise levels before read this way.
        .pin_bit_mask = GPIO_SEL_34 | GPIO_SEL_35 | GPIO_SEL_36 | GPIO_SEL_39,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&ioConfig));

    dip_switches = 
        gpio_get_level(GPIO_NUM_36) // Top pin marked VP  Ringer
      | gpio_get_level(GPIO_NUM_39) << 1 // Next pin marked VN Button
      | gpio_get_level(GPIO_NUM_34) << 2 // Next pin marked D34 lower mode bit
      | gpio_get_level(GPIO_NUM_35) << 3; // Bottomost pin marked D35 is upper mode bit.
    
    ESP_LOGI("dipswitches.c", "Read values %x", (int) dip_switches);
}