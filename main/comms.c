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

#define MAX_PEERS 10

#define HEARTBEAT_INTERVAL_MS 10000
#define KEEPALIVE_LIFE_MS 60000
#define PEER_SEMAPHORE_BLOCK_TIME 1000
#define QUEUE_SEND_BLOCK_TICKS pdMS_TO_TICKS(500)

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
static StaticSemaphore_t s_peer_static_semaphore;
static SemaphoreHandle_t s_peer_semaphore;

send_broadcast_function_t *comms_send_callback = NULL;
comms_status_summary_t comms_status_summary;


static void recalculate_peer_summary() {
    int peers = 0;
    int buttons = 0;
    int ringers = 0;
    bool hasChange = false;

    uint64_t now = esp_timer_get_time();
    uint64_t earliest_seen = now;
    uint64_t oldest_valid =
        now > KEEPALIVE_LIFE_MS * 1000 ?
        now - KEEPALIVE_LIFE_MS * 1000 :
        0;

    for(int i = 0; i < MAX_PEERS; i++) {
        peer_info_t *peer = s_peers+i;
        if(peer->is_active) {
            if(peer->last_seen_time < oldest_valid) {
                ESP_LOGI(TAG, "Peer "MACSTR" disappeared", MAC2STR(peer->node_id));
                peer->is_active = false;
            } else {
                peers++;
                if(peer->node_flags & NODE_FLAG_HAS_BUTTON) buttons++;
                if(peer->node_flags & NODE_FLAG_HAS_RINGER) ringers++;
                if(peer->last_seen_time < earliest_seen) earliest_seen = peer->last_seen_time;
            }
        }
    }

    hasChange = comms_status_summary.peers != peers ||
       comms_status_summary.peers_with_button != buttons ||
       comms_status_summary.peers_with_ringer != ringers ||
       comms_status_summary.max_peers < peers;

    comms_status_summary.peers = peers;
    comms_status_summary.peers_with_button = buttons;
    comms_status_summary.peers_with_ringer = ringers; 
    if(comms_status_summary.max_peers < peers) comms_status_summary.max_peers = peers;

    if(hasChange) {
        main_queue_event_t change_evt = {
            .id = EVENT_TYPE_PEER_COUNT_CHANGE,
            .info.peer_change = {
                .max_peers = comms_status_summary.max_peers,
                .peers = peers,
                .peers_with_button = buttons,
                .peers_with_ringer = ringers
            }
        };

        if(xQueueSend(main_queue, &change_evt, QUEUE_SEND_BLOCK_TICKS) == pdFALSE) {
            ESP_LOGW(TAG, "Failed to publish summary change event");
        }
    }

    if(comms_status_summary.peers > 0) {
        // Add a delay above the keepalive ensures that peers are well dead before
        // cleanup and that the ticks calculation cannot be zero or less.
        uint64_t delay = 500 + KEEPALIVE_LIFE_MS - (now - earliest_seen)/1000;
        ESP_LOGI(TAG, "Scheduling cleanup timer for %lld ms", delay);
        xTimerChangePeriod(s_cleanup_timer,  
            pdMS_TO_TICKS(delay),
            QUEUE_SEND_BLOCK_TICKS );
        xTimerStart(s_cleanup_timer, QUEUE_SEND_BLOCK_TICKS);
    } else {
        xTimerStop(s_cleanup_timer, QUEUE_SEND_BLOCK_TICKS);
    }
}

static void cleanup_timer_callback(TimerHandle_t timer) {
    if(xSemaphoreTake(s_peer_semaphore, QUEUE_SEND_BLOCK_TICKS) == pdFALSE) {
        ESP_LOGE(TAG, "Cleanup timer failed to take semaphore");
        xTimerStart(s_cleanup_timer, QUEUE_SEND_BLOCK_TICKS);
        return;
    }

    recalculate_peer_summary();

    xSemaphoreGive(s_peer_semaphore);
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
    memcpy(packet.info.heartbeat.node_id, s_my_node_id, NODE_ID_LEN);

    comms_send_packet(&packet);
}

static void heartbeat_task(void *pvParameter) {
    while(true) {
        EventBits_t bits = xEventGroupWaitBits(main_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));
        if((bits & WIFI_CONNECTED_BIT) == 0) {
            ESP_LOGI(TAG, "Heartbeat Task waiting for WiFi up");
            continue;
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

    for(int i = 0; i < MAX_PEERS; i++) s_peers[i].is_active = false;

    s_peer_semaphore = xSemaphoreCreateMutexStatic(&s_peer_static_semaphore);

    s_cleanup_timer = xTimerCreate(
        "Comms Cleanup Timer",
        pdMS_TO_TICKS(KEEPALIVE_LIFE_MS),
        pdFALSE,
        NULL,
        cleanup_timer_callback
    );

    xTaskCreate(heartbeat_task, "Comms Heartbeat Task", 4096, NULL, 1, NULL);
}

void onHeartbeat(packet_type_heartbeat_t *heartbeat) {
    ESP_LOGI(TAG, "Got Heartbeat from "MACSTR" uptime %llu seconds, min heap %d, heap %d, flags %x", 
        MAC2STR(heartbeat->node_id), 
        heartbeat->uptime / 1000000, 
        heartbeat->minimum_free_heap, 
        heartbeat->current_free_heap,
        (int)heartbeat->node_flags);
    
    if(xSemaphoreTake(s_peer_semaphore, pdMS_TO_TICKS(PEER_SEMAPHORE_BLOCK_TIME)) == pdFALSE) {
        ESP_LOGE(TAG, "Failed to take sempahore for peer calculation");
        return;
    }

    peer_info_t *peer_info = NULL;
    peer_info_t *first_free_slot = NULL;
    for(int i = 0; i < MAX_PEERS && peer_info == NULL; i++) {
        if(memcmp(s_peers[i].node_id, heartbeat->node_id, NODE_ID_LEN) == 0) {
            peer_info = s_peers + i;
        }
        if(!s_peers[i].is_active) {
            first_free_slot = s_peers + i;
        }
    }

    if(peer_info == NULL && first_free_slot != NULL) {
        // If we've not seen it then fake up a new slot and set that as inactive.
        ESP_LOGD(TAG, "Peer is new");
        peer_info = first_free_slot;
        peer_info->is_active = false;
        peer_info->node_flags = heartbeat->node_flags;
        peer_info->last_seen_time = 0;
        memcpy(peer_info->node_id, heartbeat->node_id, NODE_ID_LEN);
    }

    if(peer_info == NULL) {
        ESP_LOGW(TAG, "No free slots for new peer");
        xSemaphoreGive(s_peer_semaphore);
        return;
    }

    peer_info->is_active = true;
    peer_info->node_flags = heartbeat->node_flags;
    peer_info->last_seen_time = esp_timer_get_time();

    recalculate_peer_summary();
    xSemaphoreGive(s_peer_semaphore);
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