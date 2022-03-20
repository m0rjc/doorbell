
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/mcpwm.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "common.h"
#include "leds.h"

#define GPIO_WIFI_NUM CONFIG_GPIO_LED_WIFI_NUM
#define GPIO_WIFI_SEL GPIO_SEL_N(GPIO_WIFI_NUM)

#define GPIO_READY_NUM CONFIG_GPIO_LED_READY_NUM
#define GPIO_READY_SEL GPIO_SEL_N(GPIO_READY_NUM)

#define GPIO_CONFIG_NUM CONFIG_GPIO_LED_CONFIG_NUM
#define GPIO_CONFIG_SEL GPIO_SEL_N(GPIO_CONFIG_NUM)

#define GPIO_RX_NUM CONFIG_GPIO_LED_RX_NUM
#define GPIO_RX_SEL GPIO_SEL_N(GPIO_RX_NUM)

#define GPIO_TX_NUM CONFIG_GPIO_LED_TX_NUM
#define GPIO_TX_SEL GPIO_SEL_N(GPIO_TX_NUM)

#define BLINK_TIME_US 200000

typedef struct {
    uint8_t leds;
    uint64_t end_time;
} blink_timeout_t; 

#define MAX_BLINK_TIMERS 3  // 2 should be enough. 3 to be sure
static uint8_t steady_state = 0;
static blink_timeout_t blink_timers[MAX_BLINK_TIMERS];
static TaskHandle_t led_task_handle = NULL;

static void led_task(void *pvParameter) {
    uint64_t now = esp_timer_get_time();
    while(true) {
        // Work out the current state
        uint64_t next_wakeup = UINT64_MAX;
        uint8_t active_leds = steady_state;
        for(int i = 0; i < MAX_BLINK_TIMERS; i++) {
            if(blink_timers[i].end_time > now) {
                active_leds |= blink_timers[i].leds;
                if(blink_timers[i].end_time < next_wakeup) next_wakeup = blink_timers[i].end_time;
            }
        }
        gpio_set_level(GPIO_CONFIG_NUM, active_leds & LED_STATUS_CONFIG);
        gpio_set_level(GPIO_READY_NUM, (active_leds & LED_STATUS_READY) >> 1);
        gpio_set_level(GPIO_WIFI_NUM, (active_leds & LED_STATUS_WIFI) >> 2);
        gpio_set_level(GPIO_RX_NUM, (active_leds & LED_RX) >> 3);
        gpio_set_level(GPIO_TX_NUM, (active_leds & LED_TX) >> 4);

        TickType_t sleep = next_wakeup < UINT64_MAX ?
            pdMS_TO_TICKS( (next_wakeup - now) / 1000) :
            portMAX_DELAY;

        uint32_t notify_value;
        uint8_t leds_to_blink;
        xTaskNotifyWaitIndexed(TASK_NOTIFY_INDEX, 0, 0xFF, &notify_value, sleep);
        leds_to_blink = (uint8_t) notify_value;

        // Set up any blinking LEDs
        now = esp_timer_get_time();
        if(leds_to_blink) {
            for(int i = 0; i < MAX_BLINK_TIMERS; i++) {
                uint8_t timer_leds = blink_timers[i].leds;
                // The timer can be used if its LED set matches or it has expired.
                if(blink_timers[i].end_time == 0 || timer_leds == leds_to_blink) {
                    blink_timers[i].end_time = now + BLINK_TIME_US;
                    blink_timers[i].leds = leds_to_blink;
                    break;
                }
            }
        }
    }
}

void led_init() {
    memset(blink_timers, 0, sizeof(blink_timers));
    gpio_config_t ioConfig = {
        .pin_bit_mask = GPIO_READY_SEL | GPIO_CONFIG_SEL | GPIO_WIFI_SEL | GPIO_RX_SEL | GPIO_TX_SEL,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&ioConfig));

    xTaskCreate(led_task, "LED Driver", 2048, NULL, 3, &led_task_handle);
}

void led_set(uint8_t leds, int state) {
    if(state) {
        steady_state |= leds;
    } else {
        steady_state &= ~leds;
    }
    if(led_task_handle != NULL) xTaskNotifyIndexed(led_task_handle, TASK_NOTIFY_INDEX, 0, eNoAction);
}

void led_blink(uint8_t leds) {
    if(led_task_handle != NULL) xTaskNotifyIndexed(led_task_handle, TASK_NOTIFY_INDEX, (int)leds, eSetBits);
}