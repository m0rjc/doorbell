/* ADC1 Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_wifi.h"

#include "leds.h"
#include "sleep.h"
#include "mainQueue.h"
#include "broadcast.h"

const char *MESSAGE = "Testing de M0RJC";
static QueueHandle_t mainQueue;

static void onBeaconReceive(main_queue_event_t *evt) {
    ;
    printf("RC receive event: len=%d\n", (int)evt->info.beacon_received.length);
    free(evt->info.beacon_received.data);
}

static void demo_task(void *pvParameter) {
    main_queue_event_t evt;
    int64_t lastSendTime = 0;
    int64_t ledOnTime = 0;
    int64_t now;
    int leftToSend = CONFIG_ESPNOW_SEND_COUNT;
    int canSend = 1;

    broadcast_send(MESSAGE, strlen(MESSAGE));
    while(true) {
        now = esp_timer_get_time();
        
        if(canSend && leftToSend > 0) {
            leftToSend--;
            canSend = 0;
            broadcast_beacon_send();
            broadcast_send(MESSAGE, strlen(MESSAGE) + 1);
        }

        BaseType_t readResult = xQueueReceive(mainQueue, &evt, pdMS_TO_TICKS(100));
        if(readResult == pdTRUE) {
            switch(evt.id) {
                case EVENT_TYPE_BEACON_TX_FINISHED:
                    printf("TX finished "MACSTR" status=%s\n", 
                        MAC2STR(evt.info.tx_finished.mac_addr),
                        (evt.info.tx_finished.status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL"));
                    canSend = 1;
                    break;
                case EVENT_TYPE_BEACON_RECEIVED:
                    onBeaconReceive(&evt);
                    setBlueLed(1);
                    ledOnTime = now;
                    break;
                default:
                    printf("Unknown event ID on main queue.\n");
            }
        } else {
            if(now >= (ledOnTime + 200000)) {
                setBlueLed(0);
            }
            if(now >= (lastSendTime + CONFIG_ESPNOW_SEND_DELAY*1000) || now < lastSendTime) {
                lastSendTime = now;
                canSend = 1;
                leftToSend = CONFIG_ESPNOW_SEND_COUNT;
            }
        }
    }
}

void app_main(void)
{
    esp_timer_early_init();
    mainQueue = main_queue_init();
    broadcast_init(mainQueue);
    initBlueLed();

    xTaskCreate(demo_task, "Demo Task", 2048, NULL, 4, NULL);

    // vTaskDelay(100);
    // startSleep();
}
