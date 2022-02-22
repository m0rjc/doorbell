#include <stdio.h>
#include <stdlib.h>
#include "esp_sleep.h"

#include "sleep.h"

#define POLL_INTERVAL_SECONDS 5

void startSleep() {
    printf("Calling sleep\n");
    ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF));
    ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF));
    ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF));
    ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_OFF));
    ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_RTC8M, ESP_PD_OPTION_OFF));
    ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_VDDSDIO, ESP_PD_OPTION_OFF));
    esp_sleep_enable_timer_wakeup(POLL_INTERVAL_SECONDS * 1000000);
    esp_deep_sleep_start();
}