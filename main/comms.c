#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_crc.h"
#include "esp_mac.h"

#include "mainQueue.h"
#include "comms.h"

#define MAX_PEERS 10

#define HEARTBEAT_INTERVAL_MS 10000
#define KEEPALIVE_LIFE_MS 60000

static const char *TAG = "comms.c";
static const char PACKET_MAGIC_NUMBER[] = {0xD0, 0x00, 0xBE, 0x11};


typedef struct  {
    bool is_active;
    uint8_t node_id[NODE_ID_LEN];
    uint8_t node_flags;
    uint64_t last_seen_time;
} peer_info_t;

static peer_info_t s_peers[MAX_PEERS];
static uint8_t s_my_node_id[NODE_ID_LEN];
static uint8_t s_my_node_flags;
static TimerHandle_t s_cleanup_timer;

send_broadcast_function_t *comms_send_callback = NULL;
comms_status_summary_t comms_status_summary;

static void cleanup_timer_callback(TimerHandle_t timer) {

}

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
    memcpy(packet.info.heartbeat.node_id, s_my_node_id, NODE_ID_LEN * sizeof(uint8_t));

    comms_send_packet(&packet);
}

static void heartbeat_task(void *pvParameter) {
    while(true) {
        if((xEventGroupGetBits(main_event_group) & WIFI_CONNECTED_BIT) == 0) {
            ESP_LOGI(TAG, "Heartbeat Task waiting for WiFi up");
            if(xEventGroupWaitBits(main_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(60000)) == 0) {
                continue;
            }
        }

        do {
            ESP_LOGI(TAG, "Heartbeat Task sending");
            comms_send_heartbeat();
            vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));
        } while(xEventGroupGetBits(main_event_group) & WIFI_CONNECTED_BIT);
    }
}

void comms_init(uint8_t my_node_flags) {
    comms_status_summary.peers = 0;
    comms_status_summary.peers_with_button = 0;
    comms_status_summary.peers_with_ringer = 0;
    comms_status_summary.ringers_acknowledged_last_ring = 0;
    comms_status_summary.current_ring_number = 0;

    s_my_node_flags = my_node_flags;
    memset(s_my_node_id, 0, sizeof(s_my_node_id));
    esp_base_mac_addr_get(s_my_node_id);

    s_cleanup_timer = xTimerCreate(
        "Comms Cleanup Timer",
        pdMS_TO_TICKS(KEEPALIVE_LIFE_MS),
        pdFALSE,
        NULL,
        cleanup_timer_callback
    );

    xTaskCreate(heartbeat_task, "Comms Heartbeat Task", 2048, NULL, 1, NULL);
}

void onHeartbeat(packet_type_heartbeat_t *heartbeat) {
    ESP_LOGI(TAG, "Got Heartbeat from "MACSTR" uptime %llu seconds, min heap %d, heap %d, flags %x", 
        MAC2STR(heartbeat->node_id), 
        heartbeat->uptime / 1000000, 
        heartbeat->minimum_free_heap, 
        heartbeat->current_free_heap,
        (int)heartbeat->node_flags); 
}

void comms_on_packet(void *buffer, int length) {
    if(comms_verify_packet(buffer, length)) {
        packet_t *packet = (packet_t *) buffer;
        switch(packet->id) {
            case PACKET_TYPE_HEARTBEAT:
                onHeartbeat(&packet->info.heartbeat);
                break;
            default:
                ESP_LOGW(TAG, "comms_on_packet: Unexpected packet type %d", packet->id);
        }
    }
}