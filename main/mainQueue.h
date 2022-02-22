#pragma once

#include "freertos/queue.h"

typedef enum {
    EVENT_TYPE_BEACON_RECEIVED
} main_queue_event_id_t;

typedef struct {
    void *data;
    int length;
} main_queue_event_beacon_received_t;

typedef union {
    main_queue_event_beacon_received_t;
} main_queue_event_info_t;

typedef struct {
    main_queue_event_id_t id;
    main_queue_event_info_t info;
} main_queue_event_t;

/**
 * @brief Set up the main event queue.
 */
QueueHandle_t main_queue_init();

/**
 * @brief Close down the main event queue.
 */
void main_queue_teardown(QueueSetHandle_t queue);
