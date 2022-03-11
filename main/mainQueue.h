#pragma once

#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "common.h"
#include "comms.h"

extern QueueHandle_t main_queue;
extern EventGroupHandle_t main_event_group;

#define QUEUE_SEND_BLOCK_TICKS pdMS_TO_TICKS(500)

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_RECONNECT_REQUEST_BIT BIT1

typedef enum {
    EVENT_TYPE_NETWORK_CHANGE,
    EVENT_TYPE_PEER_COUNT_CHANGE,
    EVENT_TYPE_BELL_BUTTON_PUSH,
    EVENT_TYPE_REMOTE_BELL_BUTTON_PUSH,
    EVENT_TYPE_ACKNOWLEDGE
} main_queue_event_id_t;

typedef struct {
    bool is_network_up;
} main_queue_event_network_change_t;

typedef struct {
    int max_peers;
    int peers;
    int peers_with_button;
    int peers_with_ringer;
} main_queue_event_peer_count_change_t;

typedef struct {
    ring_event_number_t event_number;
    uint8_t node_id[NODE_ID_LEN];
} main_queue_event_acknowledge_t;

typedef struct {
    // A random number different for each ring
    uint32_t ring_number;
} main_queue_event_bell_button_push_t;

typedef struct {
    // A random number different for each ring
    uint32_t ring_number;
    uint8_t node_id[NODE_ID_LEN];
    char node_name[NODE_NAME_LEN+1];
    uint8_t ring_pattern_number;
} main_queue_event_remote_bell_button_push_t;


typedef union {
    main_queue_event_network_change_t network_change;
    main_queue_event_peer_count_change_t peer_change;
    main_queue_event_acknowledge_t acknowledge;
    main_queue_event_bell_button_push_t bell_button_push;
    main_queue_event_remote_bell_button_push_t remote_button_push;
} main_queue_event_info_t;

typedef struct {
    main_queue_event_id_t id;
    main_queue_event_info_t info;
} main_queue_event_t;

/**
 * @brief Set up the main event queue.
 */
void main_queue_init();

