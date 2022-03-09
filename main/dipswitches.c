#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"

#include "esp_log.h"
#include "common.h"
#include "dipswitches.h"

#define GPIO_SEL_DIP1 GPIO_SEL_N(CONFIG_GPIO_DIP1_NUM)
#define GPIO_SEL_DIP2 GPIO_SEL_N(CONFIG_GPIO_DIP2_NUM)
#define GPIO_SEL_DIP3 GPIO_SEL_N(CONFIG_GPIO_DIP3_NUM)
#define GPIO_SEL_DIP4 GPIO_SEL_N(CONFIG_GPIO_DIP4_NUM)

uint8_t dip_switches;

void dip_switchs_init() {
    gpio_config_t ioConfig = {
        .pin_bit_mask = GPIO_SEL_DIP1 | GPIO_SEL_DIP2 | GPIO_SEL_DIP3 | GPIO_SEL_DIP4,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&ioConfig));

    // Allow voltages to steady.
    vTaskDelay(pdMS_TO_TICKS(100));

    dip_switches = 
        gpio_get_level(CONFIG_GPIO_DIP1_NUM) 
      | gpio_get_level(CONFIG_GPIO_DIP2_NUM) << 1 
      | gpio_get_level(CONFIG_GPIO_DIP3_NUM) << 2 
      | gpio_get_level(CONFIG_GPIO_DIP4_NUM) << 3; 
    
    // We're using pull down when on, so invert the four bits.
    dip_switches ^= 0x0F;
    
    ESP_LOGI("dipswitches.c", "Read values %x", (int) dip_switches);
}