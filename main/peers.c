#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_mac.h"
#include "esp_log.h"

#include "comms.h"
#include "mainQueue.h"
#include "peers.h"
#include "common.h"

#define KEEPALIVE_LIFE_MS 20000
#define TASK_NOTIFY_INDEX 0

peer_info_t peer_infos[MAX_PEERS];
comms_status_summary_t comms_status_summary;
static StaticSemaphore_t s_peer_static_semaphore;
static SemaphoreHandle_t s_peer_semaphore;
static TaskHandle_t cleanup_task_handle;

static const char *TAG = "peers.c";

/**
 * @brief Recalculate the peer summary
 * 
 * @return int time in milliseconds until further calculation is needed, 
 *    or a large number if there is no current need.
 */
static int recalculate_peer_summary() {
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
        peer_info_t *peer = peer_infos+i;
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
            ESP_LOGE(TAG, "Failed to publish summary change event");
        }
    }

    // Add a delay above the keepalive ensures that peers are well dead before
    // cleanup and that the ticks calculation cannot be zero or less.
    int delay = (int)(500 + KEEPALIVE_LIFE_MS - (now - earliest_seen)/1000);
    return delay;
}

static void cleanup_task(void *pvParameters) {
    while(true) {
        int delay = KEEPALIVE_LIFE_MS;
        if(xSemaphoreTake(s_peer_semaphore, QUEUE_SEND_BLOCK_TICKS) == pdFALSE) {
            ESP_LOGE(TAG, "Cleanup timer failed to take semaphore");
        } else {
            recalculate_peer_summary();
            xSemaphoreGive(s_peer_semaphore);
        }

        ESP_LOGD(TAG, "Cleapup task sleeping for %d ms", delay);
        xTaskNotifyWaitIndexed(TASK_NOTIFY_INDEX, 0,0, NULL, pdMS_TO_TICKS(delay));
    }
}

void peers_init() {
    comms_status_summary.peers = 0;
    comms_status_summary.peers_with_button = 0;
    comms_status_summary.peers_with_ringer = 0;
    comms_status_summary.ringers_acknowledged_last_ring = 0;

    for(int i = 0; i < MAX_PEERS; i++) peer_infos[i].is_active = false;
    s_peer_semaphore = xSemaphoreCreateMutexStatic(&s_peer_static_semaphore);

    xTaskCreate(cleanup_task, "Peer List Maintenance", 2048, NULL, 1, &cleanup_task_handle);
}

void peers_on_heartbeat(packet_type_heartbeat_t *heartbeat) {
    if(xSemaphoreTake(s_peer_semaphore, pdMS_TO_TICKS(QUEUE_SEND_BLOCK_TICKS)) == pdFALSE) {
        ESP_LOGE(TAG, "Failed to take sempahore for peer calculation");
        return;
    }

    peer_info_t *peer_info = NULL;
    peer_info_t *first_free_slot = NULL;
    for(int i = 0; i < MAX_PEERS && peer_info == NULL; i++) {
        if(memcmp(peer_infos[i].node_id, heartbeat->node_id, NODE_ID_LEN) == 0) {
            peer_info = peer_infos + i;
        }
        if(!peer_infos[i].is_active) {
            first_free_slot = peer_infos + i;
        }
    }

    if(peer_info == NULL && first_free_slot != NULL) {
        // If we've not seen it then fake up a new slot and set that as inactive.
        ESP_LOGI(TAG, "Peer "MACSTR" is new", MAC2STR(heartbeat->node_id));
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

    xSemaphoreGive(s_peer_semaphore);
    xTaskNotifyGiveIndexed(cleanup_task_handle, TASK_NOTIFY_INDEX);
}
