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

#include "mainQueue.h"
#include "comms.h"
#include "peers.h"

#define HEARTBEAT_INTERVAL_MS 5000
#define PACKET_BURST_COUNT 5

static const char *TAG = "comms.c";
static const char PACKET_MAGIC_NUMBER[] = {0xD0, 0x00, 0xBE, 0x11};


static uint8_t s_my_node_id[NODE_ID_LEN];
static uint8_t s_my_node_flags;

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

    for(int i = 0; i < PACKET_BURST_COUNT; i++) {
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

    xTaskCreate(heartbeat_task, "Comms Heartbeat Task", 4096, NULL, 1, NULL);
}

void onHeartbeat(packet_type_heartbeat_t *heartbeat) {
    ESP_LOGD(TAG, "Got Heartbeat from "MACSTR" uptime %llu seconds, min heap %d, heap %d, flags %x", 
        MAC2STR(heartbeat->node_id), 
        heartbeat->uptime / 1000000, 
        heartbeat->minimum_free_heap, 
        heartbeat->current_free_heap,
        (int)heartbeat->node_flags);
    
    peers_on_heartbeat(heartbeat);
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