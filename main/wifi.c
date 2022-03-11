
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "freertos/queue.h"

#include "dipswitches.h"
#include "mainQueue.h"
#include "nvs.h"
#include "wifi.h"

// #include "lwip/err.h"
// #include "lwip/sys.h"

static const char *TAG = "wifi.c";

#define KEEPALIVE_INITIAL_DELAY_MS 1000
#define KEEPALIVE_BACKOFF_RATE 1.5
#define KEEPALIVE_MAX_BACKOFF_MS 10000

static esp_event_handler_instance_t instance_any_id;
static esp_event_handler_instance_t instance_got_ip;


static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
                    ESP_LOGD(TAG, "WIFI_EVENT_STA_DISCONNECTED");

        xEventGroupClearBits(main_event_group, WIFI_CONNECTED_BIT);
        xEventGroupSetBits(main_event_group, WIFI_RECONNECT_REQUEST_BIT);

        main_queue_event_t mqe = {
            .id = EVENT_TYPE_NETWORK_CHANGE,
            .info.network_change.is_network_up = false
        };
        xQueueSend(main_queue, &mqe, QUEUE_SEND_BLOCK_TICKS);

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(main_event_group, WIFI_CONNECTED_BIT);

        main_queue_event_t mqe = {
            .id = EVENT_TYPE_NETWORK_CHANGE,
            .info.network_change.is_network_up = true
        };
        xQueueSend(main_queue, &mqe, QUEUE_SEND_BLOCK_TICKS);
    }
}

void wifi_keepalive_task(void *pvParameter) {
    while(true) {
        int delay = KEEPALIVE_INITIAL_DELAY_MS;
        EventBits_t bits = xEventGroupWaitBits(main_event_group, WIFI_RECONNECT_REQUEST_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
        // Do nothing if just a timeout.
        while((bits & WIFI_RECONNECT_REQUEST_BIT) != 0) {
            ESP_LOGD(TAG, "Keepalive reconnect triggered. Waiting %d milliseconds", delay);
            vTaskDelay(pdMS_TO_TICKS(delay));
            delay *= KEEPALIVE_BACKOFF_RATE;
            if(delay > KEEPALIVE_MAX_BACKOFF_MS) delay = KEEPALIVE_MAX_BACKOFF_MS;
            ESP_LOGD(TAG, "Keepalive reconnect calling esp_wifi_connect");
            xEventGroupClearBits(main_event_group, WIFI_RECONNECT_REQUEST_BIT);
            esp_wifi_connect();
            
            // Give it up to 10s to connect.
            bits = xEventGroupWaitBits(main_event_group, WIFI_RECONNECT_REQUEST_BIT | WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
        }
    }
}

void wifi_init_sta(void)
{
    int hasStationConfig = strlen(m0rjc_config.ssid) > 0;
    int allowAP = !hasStationConfig || DIP_IS_MODE_CONFIG;

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    if(hasStationConfig) {
        esp_netif_create_default_wifi_sta();
    }
    if(allowAP) {
        esp_netif_create_default_wifi_ap();
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_wifi_init(&cfg);
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_sta_config = {
        .sta = {
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        }
    };

    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = CONFIG_SETUP_AP_SSID,
            .ssid_len = strlen(CONFIG_SETUP_AP_SSID),
            .channel = CONFIG_SETUP_AP_CHANNEL,
            .password = CONFIG_SETUP_AP_PASS,
            .max_connection = CONFIG_SETUP_AP_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        }
    };

    strncpy((char *)wifi_sta_config.sta.ssid, m0rjc_config.ssid, sizeof(wifi_sta_config.sta.ssid));
    strncpy((char *)wifi_sta_config.sta.password, m0rjc_config.password, sizeof(wifi_sta_config.sta.password));

    if(strlen(m0rjc_config.password) > 0) {
        wifi_sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }
    if(strlen(CONFIG_SETUP_AP_PASS) == 0) {
        wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(
        hasStationConfig ? 
            (allowAP ? WIFI_MODE_APSTA : WIFI_MODE_STA) :
            (allowAP ? WIFI_MODE_AP : WIFI_MODE_NULL) ));
    if(hasStationConfig) {       
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config) );
        xTaskCreate(wifi_keepalive_task, "WiFi Keepalive", 4096, NULL, 1, NULL);
    }
    if(allowAP) {
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));
    }

    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGD(TAG, "wifi_init_sta finished.");
}

