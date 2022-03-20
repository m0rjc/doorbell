#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"

#include "esp_log.h"
#include "common.h"
#include "dipswitches.h"

#define GPIO_SEL_CONFIG GPIO_SEL_N(CONFIG_GPIO_CONFIG_NUM)

uint8_t dip_switches;

void dip_switchs_init() {
    // This used to be a bank of dip switches to set peripherals and config.
    // The thought was that the hardware would describe itself, but in the end
    // the extra soldering meant I moved to using software instead. The exception
    // is putting it into config mode which is a pushbutton on the finished project.
    gpio_config_t ioConfig = {
        .pin_bit_mask = GPIO_SEL_CONFIG,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&ioConfig));

    // Allow voltages to steady.
    vTaskDelay(pdMS_TO_TICKS(100));

    dip_switches = 
        gpio_get_level(CONFIG_GPIO_CONFIG_NUM) << 2; 
    
    // We're using pull down when on, so invert the three bits.
    // We'll report the bottom two bits true too so the system
    // works with bell and button until the move to doing this
    // in software is complete.
    dip_switches ^= 0x07;
    
    ESP_LOGI("dipswitches.c", "Read values %x", (int) dip_switches);
}