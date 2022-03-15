#pragma once

typedef enum  {
    STATUS_LED_OFF = 0,
    STATUS_LED_READY = 1,
    STATUS_LED_RX = 2,
    STATUS_LED_READY_RX = 3
} status_led_state_t;

void initBlueLed();
void setBlueLed(uint32_t state);

void setUserLed(status_led_state_t state);