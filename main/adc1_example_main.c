/* ADC1 Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_timer.h"
#include "esp_wifi.h"

#include "leds.h"
#include "sleep.h"
#include "mainQueue.h"
#include "wifi.h"
#include "multicast.h"


void app_main(void)
{
    esp_timer_early_init();
    main_queue_init();
    wifi_init_sta();
    initBlueLed();

    xTaskCreate(mcast_example_task, "Demo Task", 4096, NULL, 4, NULL);

    // vTaskDelay(100);
    // startSleep();
}
