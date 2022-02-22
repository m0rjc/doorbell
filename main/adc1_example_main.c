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

#include "leds.h"
#include "sleep.h"
#include "mainQueue.h"
#include "broadcast.h"

const char *MESSAGE = "Testing de M0RJC";

void app_main(void)
{
    QueueHandle_t mainQueue = main_queue_init();
    broadcast_init(mainQueue);
    lightBlueLed();


    broadcast_send(MESSAGE, strlen(MESSAGE) * sizeof(char));
    vTaskDelay(100);
    startSleep();
}
