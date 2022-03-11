#include <stdint.h>>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "driver/gpio.h"

#include "common.h"
#include "ringer.h"

#define GPIO_SEL_RINGER GPIO_SEL_N(CONFIG_GPIO_RINGER_NUM)

#define RING_REST_TIME_TICKS pdMS_TO_TICKS(1000)

/**
 * @brief Ring patterns
 * Each number in the list is a time in deciseconds. Alternating on, off. Ending zero (which turns the ringer off.)
 * 
 */
static const uint8_t PATTERNS[RINGER_NUM_PATTERNS][2] = {
    {5,0}
};  

static TaskHandle_t ringer_task_handle;

static void ringer_task(void *pvParameters) {
    gpio_set_level(CONFIG_GPIO_RINGER_NUM, 0);
    while(1) {
        uint32_t patternIndex;
        xTaskNotifyWaitIndexed(TASK_NOTIFY_INDEX, 0, 0, &patternIndex, portMAX_DELAY);
        patternIndex = patternIndex % RINGER_NUM_PATTERNS;
        uint8_t *pTime = PATTERNS[patternIndex];
        while(*pTime != 0) {
            gpio_set_level(CONFIG_GPIO_RINGER_NUM, 1);
            vTaskDelay(pdMS_TO_TICKS(((uint32_t)*pTime) * 100));
            gpio_set_level(CONFIG_GPIO_RINGER_NUM, 0);
            pTime++;
            if(*pTime != 0) {
                vTaskDelay(pdMS_TO_TICKS(((uint32_t)*pTime) * 100));
                pTime++;
            }
        }
        gpio_set_level(CONFIG_GPIO_RINGER_NUM, 0);
        vTaskDelay(RING_REST_TIME_TICKS);
    }
}

void ringer_init() {
    gpio_config_t ioConfig = {
        .pin_bit_mask = GPIO_SEL_RINGER,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&ioConfig));

    xTaskCreate(ringer_task, "Ringer", 2048, NULL, 4, &ringer_task_handle);
}

void ringer_ring(int pattern) {
    xTaskNotifyIndexed(ringer_task_handle, TASK_NOTIFY_INDEX, pattern, eSetValueWithOverwrite);
}