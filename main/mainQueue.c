#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "mainQueue.h"
#include "esp_log.h"

#define QUEUE_SIZE 5
const char* TAG = "mainQueue.c";

QueueHandle_t main_queue;
EventGroupHandle_t main_event_group;


/**
 * @brief Set up the main event queue.
 */
void main_queue_init(){
    for(int queueSize = QUEUE_SIZE; main_queue == NULL && queueSize > 0; queueSize--) {
        main_queue = xQueueCreate(queueSize, sizeof(main_queue_event_t));
    }

    if(main_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create main queue");
        abort();
    }

    main_event_group = xEventGroupCreate();
}

