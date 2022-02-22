#pragma once

#include "freertos/queue.h"

void broadcast_init(QueueHandle_t queue);

void broadcast_send(const void *data, const int length);

void broadcast_teardown();
