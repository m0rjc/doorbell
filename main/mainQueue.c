#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "mainQueue.h"
#include "esp_log.h"

#define QUEUE_SIZE 5
const char* TAG = "mainQueue.c";

/**
 * @brief Set up the main event queue.
 */
QueueHandle_t main_queue_init(){
    QueueHandle_t queue;

    for(int queueSize = QUEUE_SIZE; main_queue == NULL && queueSize > 0; queueSize--) {
        queue = xQueueCreate(queueSize, sizeof(main_queue_event_t));
    }

    if(queue == NULL) {
        ESP_LOG_E(TAG, "Failed to create main queue");
        abort();
    }

    return queue;
}

/**
 * @brief Close down the main event queue.
 */
void main_queue_teardown(QueueSetHandle_t queue) {
    vQueueDelete(queue);
}
