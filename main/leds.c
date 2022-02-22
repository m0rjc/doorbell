
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "leds.h"

void lightBlueLed() {
    gpio_config_t ioConfig = {
        .pin_bit_mask = GPIO_SEL_2,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&ioConfig));

    printf("Blue LED on\n");
    gpio_set_level(GPIO_NUM_2, 1);
}
