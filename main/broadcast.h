#pragma once

#include "freertos/queue.h"

void broadcast_init(QueueHandle_t queue);

/**
 * @brief Send data to all recipients. Publish a main_queue_event_beacon_tx_finished_t event when done.
 * 
 * @param data 
 * @param length 
 */
void broadcast_send(const void *data, const int length);

void broadcast_teardown();
