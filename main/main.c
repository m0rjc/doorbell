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
#include "nvs.h"
#include "peers.h"
#include "webui.h"
#include "dipswitches.h"
#include "pushbutton.h"
#include "ringer.h"

#define RING_RETRY_DELAY pdMS_TO_TICKS(500)
#define RING_RETRY_COUNT 5

static const char *TAG = "main.c";

void main_loop_task(void *pvParameter) {
    uint8_t my_ring_index = 0;
    ring_event_number_t current_ring_event = 0;
    int ring_retries = 0;
    int expected_acks = 0;
    int found_acks = 0;
    while(true) {
        main_queue_event_t event;
        TickType_t delay = current_ring_event ? RING_RETRY_DELAY : portMAX_DELAY;
        if(xQueueReceive(main_queue, &event, delay) == pdTRUE) {
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
                case EVENT_TYPE_BELL_BUTTON_PUSH:
                    current_ring_event = event.info.bell_button_push.ring_number;
                    ring_retries = 0;
                    peers_clear_acknowledge_status();
                    peers_count_acknowledgements(&expected_acks, &found_acks);
                    comms_send_ring(current_ring_event, my_ring_index);
                    ringer_ring(my_ring_index);
                    break;
                case EVENT_TYPE_REMOTE_BELL_BUTTON_PUSH:
                    ESP_LOGI(TAG, "Remote button push from "MACSTR" %s number %ux", MAC2STR(event.info.remote_button_push.node_id), event.info.remote_button_push.node_name, event.info.remote_button_push.ring_number);
                    ringer_ring(0);
                    break;
                case EVENT_TYPE_ACKNOWLEDGE:
                    if(event.info.acknowledge.event_number == current_ring_event) {
                        peers_set_acknowledged(event.info.acknowledge.node_id);
                        peers_count_acknowledgements(&expected_acks, &found_acks);
                        ESP_LOGI(TAG, "ACK event: %d ringers of %d acknowledged",
                            found_acks, expected_acks);
                    }
                    break;
            }
        } else {
            // Timeout waiting, so if needed resend
            if(current_ring_event != 0) {
                if(ring_retries < RING_RETRY_COUNT && found_acks < expected_acks) {
                    ring_retries++;
                    ESP_LOGI(TAG, "Resending ring message");
                    comms_send_ring(current_ring_event, my_ring_index); 
                } else {
                    current_ring_event = 0;
                }
            }
        }
    }
}

void app_main(void)
{
    esp_timer_early_init();
    dip_switchs_init();
    initialise_nvs();
    main_queue_init();

    if(strlen(m0rjc_config.ssid) == 0) {
        // Force config mode
        dip_switches |= DIP_SWITCH_MODE_CONFIG;
    }

    wifi_init_sta();
    initBlueLed();

    peers_init();
    ringer_init();

    uint8_t node_flags = 0;
    if(DIP_HAS_BUTTON) node_flags |= NODE_FLAG_HAS_BUTTON;
    if(DIP_HAS_RINGER) node_flags |= NODE_FLAG_HAS_RINGER;
    comms_init(node_flags);

    webui_start();

    if(DIP_IS_MODE_RUN) {
        comms_multicast_init();
        if(DIP_HAS_BUTTON) pushbutton_init();
    }

    xTaskCreate(main_loop_task, "Main Event Loop", 4096, NULL, 5, NULL);
}
