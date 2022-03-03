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
#include "freertos/timers.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_log.h"

#include "leds.h"
#include "sleep.h"
#include "mainQueue.h"
#include "wifi.h"
#include "comms.h"
#include "comms_multicast.h"

static const char *TAG = "main.c";

void main_loop_task(void *pvParameter) {
    while(true) {
        main_queue_event_t event;
        if(xQueueReceive(main_queue, &event, portMAX_DELAY) == pdTRUE) {
            switch(event.id) {
                case EVENT_TYPE_NETWORK_CHANGE:
                    ESP_LOGI(TAG, "Network change: %s", 
                        event.info.network_change.is_network_up ? "UP" : "DOWN");
                    break;
                case EVENT_TYPE_PEER_COUNT_CHANGE:
                    ESP_LOGI(TAG, "Peer change: %d peers (%d max), %d buttons, %d ringers",
                        event.info.peer_change.peers,
                        event.info.peer_change.max_peers,
                        event.info.peer_change.peers_with_button,
                        event.info.peer_change.peers_with_ringer);
                    setBlueLed(event.info.peer_change.peers > 0 ? 1 : 0);
                    break;
                case EVENT_TYPE_ACKNOWLEDGE_COUNT_CHANGE:
                    ESP_LOGI(TAG, "ACK event: %d ringers of %d",
                        event.info.acknowledge_change.ringers_ackowledged,
                        event.info.acknowledge_change.peers_with_ringer);
                    break;
            }
        }
    }
}

void app_main(void)
{
    esp_timer_early_init();
    main_queue_init();
    wifi_init_sta();
    initBlueLed();

    comms_init(0);
    comms_multicast_init();

    xTaskCreate(main_loop_task, "Main Event Loop", 2048, NULL, 5, NULL);

    vTaskStartScheduler();

    // vTaskDelay(100);
    // startSleep();
}
