#pragma once

#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_now.h"

extern QueueHandle_t main_queue;
extern EventGroupHandle_t main_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define HEARTBEAT_SEND_BIT BIT1
#define WIFI_RECONNECT_REQUEST_BIT BIT2

typedef enum {
    EVENT_TYPE_NETWORK_UP,
    EVENT_TYPE_NETWORK_DOWN,
    EVENT_TYPE_BEACON_RECEIVED,
} main_queue_event_id_t;

typedef struct {
    void *data;
    int length;
} main_queue_event_beacon_received_t;

typedef union {
    main_queue_event_beacon_received_t beacon_received;
} main_queue_event_info_t;

typedef struct {
    main_queue_event_id_t id;
    main_queue_event_info_t info;
} main_queue_event_t;

/**
 * @brief Set up the main event queue.
 */
void main_queue_init();

