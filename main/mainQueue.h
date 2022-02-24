#pragma once

#include "freertos/queue.h"
#include "esp_now.h"

typedef enum {
    EVENT_TYPE_BEACON_RECEIVED,
    EVENT_TYPE_BEACON_TX_FINISHED
} main_queue_event_id_t;

typedef struct {
    esp_now_send_status_t status;
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
} main_queue_event_beacon_tx_finished_t;

typedef struct {
    void *data;
    int length;
} main_queue_event_beacon_received_t;

typedef union {
    main_queue_event_beacon_received_t beacon_received;
    main_queue_event_beacon_tx_finished_t tx_finished;
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
