#pragma once

#define LED_STATUS_CONFIG 1
#define LED_STATUS_READY 2
#define LED_STATUS_WIFI 4
#define LED_RX 8
#define LED_TX 16

void led_init();

void led_set(uint8_t leds, int state);
void led_blink(uint8_t leds);
