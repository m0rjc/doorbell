#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_now.h"
#include "esp_crc.h"

#include "broadcast.h"
#include "mainQueue.h"

#define ESPNOW_MAXDELAY 512
#define PACKET_SIZE ESP_NOW_MAX_DATA_LEN
#define MAX_PEERS 10

static const char *TAG = "broadcast.c";

static uint8_t s_example_broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

static QueueHandle_t main_queue;

const char *MAGIC = "M0RJC";

typedef enum {
    PACKET_TYPE_BEACON,
    PACKET_TYPE_PAYLOAD
} packet_type_t;

typedef struct {
    char magic[5];
    packet_type_t type;
    uint8_t len;
    uint16_t crc;
    uint8_t payload[0];
} on_air_packet_t;

typedef struct {
    char filler[20];
} beacon_payload_t;

static beacon_payload_t s_my_beacon_payload;

typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
} peer_info_t;

static peer_info_t peers[MAX_PEERS];
static int peerCount = 0;

static void initialiseNVS() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
}

/* WiFi should start before using ESPNOW */
static void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    esp_err_t ret = esp_wifi_init(&cfg);
    if(ret == ESP_ERR_NVS_NOT_INITIALIZED) {
        initialiseNVS();
        ret = esp_wifi_init(&cfg);
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start());
    ESP_ERROR_CHECK( esp_wifi_set_channel(CONFIG_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

#if CONFIG_ESPNOW_ENABLE_LONG_RANGE
            ESP_LOGI(TAG, "Setting long range mode");
    ESP_ERROR_CHECK( esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_LR) );
#endif
}

/**
 * @brief Recieve send complete callback from ESPNow and publish on the main event channel.
 * 
 * ESPNOW sending or receiving callback function is called in WiFi task.
 * Users should not do lengthy operations from this task. Instead, post 
 * necessary data to a queue and handle it from a lower priority task.
 */
static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    main_queue_event_t event;
    main_queue_event_beacon_tx_finished_t *eventPayload = &event.info.tx_finished;

    if (mac_addr == NULL) {
        ESP_LOGE(TAG, "Send cb arg error");
    }

    event.id = EVENT_TYPE_BEACON_TX_FINISHED;
    memcpy(eventPayload->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    eventPayload->status = status;
    if (xQueueSend(main_queue, &event, ESPNOW_MAXDELAY) != pdTRUE) {
        ESP_LOGW(TAG, "Send send queue fail");
    }
}


void onReceivedPayload(on_air_packet_t *packet) {
    main_queue_event_t event;
    main_queue_event_beacon_received_t *eventPayload = &event.info.beacon_received;

    event.id = EVENT_TYPE_BEACON_RECEIVED;
    eventPayload->data = malloc(packet->len);
    eventPayload->length = packet->len;
    memcpy(eventPayload->data, packet->payload, packet->len);
    if (xQueueSend(main_queue, &event, ESPNOW_MAXDELAY) != pdTRUE) {
        ESP_LOGW(TAG, "Send queue fail for receive event");
        free(eventPayload->data);
    }
}

bool hasPeer(const uint8_t *mac_addr) {
    for(int i = 0; i < peerCount; i++) {
        if(memcmp(peers[i].mac_addr, mac_addr, ESP_NOW_ETH_ALEN) == 0) return true;
    }
    return false;
}

void onReceivedBeacon(const uint8_t *mac_addr, on_air_packet_t *packet) {
    if (esp_now_is_peer_exist(mac_addr) == true && !hasPeer(mac_addr)) {
        ESP_LOGW(TAG, "ESP Peers must be persistent");
        if(peerCount < MAX_PEERS) {
            memcpy(peers[peerCount++].mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
        }
    }
    if (esp_now_is_peer_exist(mac_addr) == false && peerCount < MAX_PEERS) {
        esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
        if (peer == NULL) {
            ESP_LOGE(TAG, "Malloc peer information fail. Beacon dropped.");
            return;
        }
        memset(peer, 0, sizeof(esp_now_peer_info_t));
        peer->channel = CONFIG_ESPNOW_CHANNEL;
        peer->ifidx = WIFI_IF_STA;
        peer->encrypt = false;
        memcpy(peer->lmk, CONFIG_ESPNOW_LMK, ESP_NOW_KEY_LEN);
        memcpy(peer->peer_addr, mac_addr, ESP_NOW_ETH_ALEN);
        ESP_ERROR_CHECK( esp_now_add_peer(peer) );
        free(peer);
        memcpy(peers[peerCount++].mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
        ESP_LOGI(TAG, "Registered peer %d "MACSTR, peerCount, MAC2STR(mac_addr));
    }
}

/**
 * @brief Handle receive callback from ESPNow and publish onto the main event channel.
 * 
 * @param mac_addr 
 * @param data 
 * @param len 
 */
static void espnow_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int len)
{
    uint16_t receivedCrc;

    if (mac_addr == NULL || data == NULL || len <= 0) {
        ESP_LOGE(TAG, "Receive cb arg error");
        return;
    }

    ESP_LOGI(TAG, "Receive callback from espnow. Length=%d addr="MACSTR"\n", len, MAC2STR(mac_addr));
    on_air_packet_t *packet = (on_air_packet_t *) data;
    if(memcmp(MAGIC, packet->magic, strlen(MAGIC)) != 0) {
        ESP_LOGI(TAG, "Packet does not start with my magic number");
        return;
    }

    receivedCrc = packet->crc;
    packet->crc = 0;
    packet->crc = esp_crc16_le(UINT16_MAX, (const uint8_t *)packet, len);

    if(receivedCrc != packet->crc) {
        ESP_LOGW(TAG, "Packet has correct magic but bad CRC");
        return;
    }

    if(packet->len > PACKET_SIZE) {
        ESP_LOGW(TAG, "Packet looks well formed but has invalid length");
        return;
    }

    switch(packet->type) {
        case PACKET_TYPE_PAYLOAD:
            onReceivedPayload(packet);
            break;
        case PACKET_TYPE_BEACON:
            onReceivedBeacon(mac_addr, packet);
            break;
    }
}

static void espnow_init() {
    ESP_ERROR_CHECK( esp_now_init() );
    ESP_ERROR_CHECK( esp_now_register_send_cb(espnow_send_cb) );
    ESP_ERROR_CHECK( esp_now_register_recv_cb(espnow_recv_cb) );

    /* Set primary master key. */
    ESP_ERROR_CHECK( esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK) );

    /* Add broadcast peer information to peer list. */
    esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL) {
        ESP_LOGE(TAG, "Malloc peer information fail");
        esp_now_deinit();
        abort();
    }
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = CONFIG_ESPNOW_CHANNEL;
    peer->ifidx = WIFI_IF_STA;
    peer->encrypt = false;
    memcpy(peer->peer_addr, s_example_broadcast_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK( esp_now_add_peer(peer) );
    free(peer);

    strcpy(s_my_beacon_payload.filler, "M0RJC");
}

void broadcast_init(QueueHandle_t queue) {
    main_queue = queue;
    wifi_init();
    espnow_init();
}

static void send(const uint8_t *macAddr, const packet_type_t packetType, const void *data, const int length) {
    int totalLength = length + sizeof(on_air_packet_t);
    assert(totalLength <= PACKET_SIZE);

    on_air_packet_t *packet = malloc(PACKET_SIZE);
    if(packet == NULL) {
        ESP_LOGE(TAG, "Cannot allocate space for ESPNow packet");
        espnow_send_cb(macAddr, ESP_NOW_SEND_FAIL);
        return;
    }

    memcpy(packet->magic, MAGIC, strlen(MAGIC));
    packet->type = packetType;
    packet->len = length;
    packet->crc = 0;
    memcpy(packet->payload, data, length);
    // I assume this is to act as nonce for any crypto the platform applies to the packet.
    if(totalLength < PACKET_SIZE)
        esp_fill_random(packet->payload + length, PACKET_SIZE - totalLength);

    packet->crc = esp_crc16_le(UINT16_MAX, (const uint8_t *)packet, PACKET_SIZE);

    if(esp_now_send(macAddr, (const uint8_t *) packet, PACKET_SIZE) != ESP_OK) {
        ESP_LOGE(TAG, "Send error");
        // Do I need to do this, or does the framework?
        espnow_send_cb(macAddr, ESP_NOW_SEND_FAIL);
    }

    free(packet);
}

/**
 * @brief Broadcast the given data.
 * Result is given through the main queue EVENT_TYPE_BEACON_TX_FINISHED event.
 * 
 * @param data data to send.
 * @param length length of this data.
 */
void broadcast_send(const void *data, const int length) {
    for(int i = 0; i < peerCount; i++) {
        send(peers[i].mac_addr, PACKET_TYPE_PAYLOAD, data, length);
    }
}

void broadcast_beacon_send() {
    send(s_example_broadcast_mac, PACKET_TYPE_BEACON, &s_my_beacon_payload, sizeof(beacon_payload_t));
}

void broadcast_teardown() {

}