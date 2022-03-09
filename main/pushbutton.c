#include <stdint.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_intr_alloc.h"
#include "driver/gpio.h"

#include "mainQueue.h"
#include "common.h"

#include "pushbutton.h"

#define DEBOUNCE_PERIOD_TICKS pdMS_TO_TICKS(100)

#define GPIO_SEL_BUTTON GPIO_SEL_N(CONFIG_GPIO_BUTTON_NUM)

static const char *TAG = "pushbutton.c";
static TaskHandle_t s_task_handle;

static void pushbutton_debounce_task(void *pvParameters) {
    bool is_debouncing = false;
    while(true) {
        TickType_t delay = is_debouncing ? DEBOUNCE_PERIOD_TICKS : portMAX_DELAY;
        BaseType_t notified = xTaskNotifyWaitIndexed(TASK_NOTIFY_INDEX, 0, 0x01, NULL, delay);
        if(notified == pdTRUE) {
            is_debouncing = true;
        } else {
            is_debouncing = false;
            if(gpio_get_level(CONFIG_GPIO_BUTTON_NUM) == 0) {
                main_queue_event_t event;
                event.id = EVENT_TYPE_BELL_BUTTON_PUSH;
                event.info.bell_button_push.ring_number = (uint32_t)(esp_timer_get_time() & 0xFFFFFFFF);           
                if(xQueueSend(main_queue, &event, QUEUE_SEND_BLOCK_TICKS) == pdFALSE) {
                    ESP_LOGE(TAG, "Failed to publish button push event");
                }
            }
        }
    }
}

static void isr_handler(void *pvParameters) {
    xTaskNotifyIndexedFromISR(s_task_handle, TASK_NOTIFY_INDEX, 0x01, eSetBits, NULL);
}

void pushbutton_init() {
    gpio_config_t ioConfig = {
        .pin_bit_mask = GPIO_SEL_BUTTON,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    ESP_ERROR_CHECK(gpio_config(&ioConfig));

    xTaskCreate(pushbutton_debounce_task, "Button Debounce", 2048, NULL, 4, &s_task_handle);

    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1));
    ESP_ERROR_CHECK(gpio_isr_handler_add(CONFIG_GPIO_BUTTON_NUM, isr_handler, NULL));
}