
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

#include "mainQueue.h"
#include "wifi.h"

// #include "lwip/err.h"
// #include "lwip/sys.h"

static const char *TAG = "wifi.c";

#define HEARTBEAT_INTERVAL_MS 10000
#define KEEPALIVE_INITIAL_DELAY_MS 1000
#define KEEPALIVE_BACKOFF_RATE 1.5
#define KEEPALIVE_MAX_BACKOFF_MS 10000

static esp_event_handler_instance_t instance_any_id;
static esp_event_handler_instance_t instance_got_ip;

static void initialiseNVS() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
}

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
                    ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");

            xEventGroupClearBits(main_event_group, WIFI_CONNECTED_BIT);
            xEventGroupSetBits(main_event_group, WIFI_RECONNECT_REQUEST_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(main_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_keepalive_task(void *pvParameter) {
    while(true) {
        int delay = KEEPALIVE_INITIAL_DELAY_MS;
        if(xEventGroupGetBits(main_event_group) & WIFI_CONNECTED_BIT) {
            xEventGroupSetBits(main_event_group, HEARTBEAT_SEND_BIT);
        }
        EventBits_t bits = xEventGroupWaitBits(main_event_group, WIFI_RECONNECT_REQUEST_BIT, pdTRUE, pdTRUE, pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));
        // Do nothing if just a timeout.
        while((bits & WIFI_RECONNECT_REQUEST_BIT) != 0) {
            ESP_LOGI(TAG, "Keepalive reconnect triggered. Waiting %d milliseconds", delay);
            vTaskDelay(pdMS_TO_TICKS(delay));
            delay *= KEEPALIVE_BACKOFF_RATE;
            if(delay > KEEPALIVE_MAX_BACKOFF_MS) delay = KEEPALIVE_MAX_BACKOFF_MS;
            ESP_LOGI(TAG, "Keepalive reconnect calling esp_wifi_connect");
            esp_wifi_connect();
            
            // Give it up to 10s to connect.
            bits = xEventGroupWaitBits(main_event_group, WIFI_RECONNECT_REQUEST_BIT, pdTRUE, pdTRUE, pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));
        }
    }
}

void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_wifi_init(&cfg);
    if(ret == ESP_ERR_NVS_NOT_INITIALIZED) {
        initialiseNVS();
        ret = esp_wifi_init(&cfg);
    }
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

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_SSID,
            .password = CONFIG_ESP_WIFI_PASSWORD,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	        .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );

    xTaskCreate(wifi_keepalive_task, "WiFi Keepalive", 4096, NULL, 1, NULL);

    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");
}

