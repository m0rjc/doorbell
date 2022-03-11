#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_crc.h"
#include "esp_mac.h"

#include "nvs.h"
#include "mainQueue.h"
#include "comms.h"
#include "peers.h"

#define HEARTBEAT_INTERVAL_MS 2000
#define HEARTBEAT_BURST_COUNT 2
#define RING_BURST_COUNT 2
#define ACK_BURST_COUNT 2

// Remember rings we've received.
// Needs to be at least the size of the number of buttons in the system that we expect
// to be pressable within the debounce and send retry period.
#define REPLAY_BUFFER_LENGTH 5

static const char *TAG = "comms.c";
static const char PACKET_MAGIC_NUMBER[] = {0xD0, 0x00, 0xBE, 0x11};


static uint8_t s_my_node_id[NODE_ID_LEN];
static uint8_t s_my_node_flags;
static ring_event_number_t s_replay_buffer[REPLAY_BUFFER_LENGTH];

send_broadcast_function_t *comms_send_callback = NULL;


static bool comms_verify_packet(void *buffer, int len) {
    if(len != sizeof(packet_t)){
        ESP_LOGW(TAG, "Incoming packet wrong length. Got %d, need %d", len, sizeof(packet_t));
        return false;
    } 

    packet_t *packet = (packet_t *)buffer;
    uint16_t crc = packet->crc;
    packet->crc = 0;
    uint16_t crccalc =  esp_crc16_le(UINT16_MAX, buffer, len);
    if(crc != crccalc) {
        ESP_LOGW(TAG, "Incoming packet wrong CRC");
        return false;
    }

    return true;
}

static void comms_send_packet(packet_t *packet) {
    if(comms_send_callback == NULL) return;
    packet->crc = 0;
    memcpy(packet->magic, PACKET_MAGIC_NUMBER, sizeof(PACKET_MAGIC_NUMBER));
    packet->crc = esp_crc16_le(UINT16_MAX, (uint8_t const *)packet, sizeof(packet_t));
    comms_send_callback(packet, sizeof(packet_t));
}

static void comms_send_heartbeat() {
    packet_t packet;
    esp_fill_random(&packet, sizeof(packet_t));
    
    packet.id = PACKET_TYPE_HEARTBEAT;
    packet.info.heartbeat.node_flags = s_my_node_flags;
    packet.info.heartbeat.minimum_free_heap = esp_get_minimum_free_heap_size();
    packet.info.heartbeat.current_free_heap = esp_get_free_heap_size();
    packet.info.heartbeat.uptime = esp_timer_get_time();
    memcpy(packet.info.heartbeat.node_id, s_my_node_id, NODE_ID_LEN);
    strncpy(packet.info.heartbeat.node_name, m0rjc_config.name, NODE_NAME_LEN);

    for(int i = 0; i < HEARTBEAT_BURST_COUNT; i++) {
        comms_send_packet(&packet);
    }
}

void comms_send_ring(ring_event_number_t ring_number, uint8_t ring_pattern_number) {
    ESP_LOGI(TAG, "Send ring %ux", ring_number);
    packet_t packet;
    esp_fill_random(&packet, sizeof(packet_t));
    packet.id = PACKET_TYPE_RING_EVENT;
    packet.info.ring.event_number = ring_number;
    memcpy(packet.info.ring.node_id, s_my_node_id, NODE_ID_LEN);
    strncpy(packet.info.ring.node_name, m0rjc_config.name, NODE_NAME_LEN);
    packet.info.ring.ring_pattern_number = ring_pattern_number;

    for(int i = 0; i < RING_BURST_COUNT; i++) {
        comms_send_packet(&packet);
    }
}

static void comms_send_ack(ring_event_number_t ring_number) {
    packet_t packet;
    esp_fill_random(&packet, sizeof(packet_t));
    packet.id = PACKET_TYPE_RING_ACKNOWLEDGE;
    packet.info.ring_ack.event_number = ring_number;
    memcpy(packet.info.ring_ack.ack_node_id, s_my_node_id, NODE_ID_LEN);

    for(int i = 0; i < ACK_BURST_COUNT; i++) {
        comms_send_packet(&packet);
    }
}

static void heartbeat_task(void *pvParameter) {
    while(true) {
        EventBits_t bits = xEventGroupWaitBits(main_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));
        if((bits & WIFI_CONNECTED_BIT) == 0) {
            ESP_LOGI(TAG, "Heartbeat Task waiting for WiFi up");
            continue;
        }

        ESP_LOGI(TAG, "Heartbeat Task entering send loop");
        do {
            ESP_LOGD(TAG, "Heartbeat Task sending");
            comms_send_heartbeat();
            vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));
        } while(xEventGroupGetBits(main_event_group) & WIFI_CONNECTED_BIT);
    }
}

void comms_init(uint8_t my_node_flags) {
    s_my_node_flags = my_node_flags;
    memset(s_my_node_id, 0, sizeof(s_my_node_id));
    esp_base_mac_addr_get(s_my_node_id);
    memset(s_replay_buffer, 0, sizeof(s_replay_buffer));

    xTaskCreate(heartbeat_task, "Comms Heartbeat Task", 4096, NULL, 1, NULL);
}

static void onHeartbeat(packet_type_heartbeat_t *heartbeat) {
    ESP_LOGD(TAG, "Got Heartbeat from "MACSTR" uptime %llu seconds, min heap %d, heap %d, flags %x", 
        MAC2STR(heartbeat->node_id), 
        heartbeat->uptime / 1000000, 
        heartbeat->minimum_free_heap, 
        heartbeat->current_free_heap,
        (int)heartbeat->node_flags);
    
    peers_on_heartbeat(heartbeat);
}

static void onRingPacket(packet_type_ring_event_t *packetinfo) {
    ring_event_number_t event_number = packetinfo->event_number;
    int found = 0;
    for(int i = 0; i < REPLAY_BUFFER_LENGTH; i++) {
        if(s_replay_buffer[i] == event_number) {
            found++;
            break;
        }
    }
    bool can_ack = true;
    if(found == 0) {
        ESP_LOGD(TAG, "RING from "MACSTR" with number %ux", MAC2STR(packetinfo->node_id), packetinfo->event_number);
        memmove(s_replay_buffer+1, s_replay_buffer, (REPLAY_BUFFER_LENGTH-1) * sizeof(ring_event_number_t));
        s_replay_buffer[0] = event_number;

        main_queue_event_t event;
        event.id = EVENT_TYPE_REMOTE_BELL_BUTTON_PUSH;
        event.info.remote_button_push.ring_number = event_number;
        memcpy(event.info.remote_button_push.node_id, packetinfo->node_id, NODE_ID_LEN);
        strncpy(event.info.remote_button_push.node_name, packetinfo->node_name, NODE_NAME_LEN);
        event.info.remote_button_push.node_name[NODE_NAME_LEN] = 0;
        event.info.remote_button_push.ring_pattern_number = packetinfo->ring_pattern_number;

        if(xQueueSend(main_queue, &event, QUEUE_SEND_BLOCK_TICKS) == pdFALSE) {
            // It would be nice not to have populated the replay buffer, but the blocking here
            // means time has passed during which more packets may have arrived. I don't want to
            // lock the replay buffer for this time. If this is a problem I could make it bigger
            // and clear the entry here. Null entries will then propagate through it.
            ESP_LOGE(TAG, "Failed to publish button push event");
            can_ack = false;
        }
    }

    if(can_ack) {
        comms_send_ack(packetinfo->event_number);
    }
}

static void onRingAcknowledge(packet_type_ring_acknowledge_t *packet) {
    main_queue_event_t event;
    event.id = EVENT_TYPE_ACKNOWLEDGE;
    event.info.acknowledge.event_number = packet->event_number;
    memcpy(event.info.acknowledge.node_id, packet->ack_node_id, NODE_ID_LEN);
    if(xQueueSend(main_queue, &event, QUEUE_SEND_BLOCK_TICKS) == pdFALSE) {
        ESP_LOGE(TAG, "Failed to publish acknowledge event");
    }    
}

void comms_on_packet(void *buffer, int length) {
    if(comms_verify_packet(buffer, length)) {
        packet_t *packet = (packet_t *) buffer;
        switch(packet->id) {
            case PACKET_TYPE_HEARTBEAT:
                onHeartbeat(&packet->info.heartbeat);
                break;
            case PACKET_TYPE_RING_EVENT:
                onRingPacket(&packet->info.ring);
                break;
            case PACKET_TYPE_RING_ACKNOWLEDGE:
                onRingAcknowledge(&packet->info.ring_ack);
                break;
            default:
                ESP_LOGW(TAG, "comms_on_packet: Unexpected packet type %d", packet->id);
        }
    }
}