
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/mcpwm.h"

#include "common.h"
#include "leds.h"

/**
 * @brief set motor moves speed and direction with duty cycle = duty %
 */
void brushed_motor_set_duty(float duty_cycle)
{
    /* motor moves in forward direction, with duty cycle = duty % */
    if (duty_cycle > 0) {
        mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A);
        mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, duty_cycle);
        mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, MCPWM_DUTY_MODE_0);  //call this each time, if operator was previously in low/high state
    }
    /* motor moves in backward direction, with duty cycle = -duty % */
    else {
        mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B);
        mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, -duty_cycle);
        mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, MCPWM_DUTY_MODE_0); //call this each time, if operator was previously in low/high state
    }
}

void initBlueLed() {
    gpio_config_t ioConfig = {
        .pin_bit_mask = GPIO_SEL_2 | GPIO_SEL_N(CONFIG_GPIO_STATUS_LED_READY_PIN) | GPIO_SEL_N(CONFIG_GPIO_STATUS_LED_RX_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&ioConfig));

/*
    mcpwm_config_t pwmconf = {
        .frequency = 1000,
        .cmpr_a = 0.0,
        .cmpr_b = 0.0,
        .duty_mode = MCPWM_DUTY_MODE_0,
        .counter_mode = MCPWM_UP_COUNTER
    };
    ESP_ERROR_CHECK(mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, CONFIG_GPIO_STATUS_LED_READY_PIN));
    ESP_ERROR_CHECK(mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, CONFIG_GPIO_STATUS_LED_RX_PIN));
    ESP_ERROR_CHECK(mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwmconf));
    brushed_motor_set_duty(-100.0);
*/
}

void setBlueLed(uint32_t state) {
   gpio_set_level(GPIO_NUM_2, state);
}

void setUserLed(status_led_state_t state) {
    gpio_set_level(CONFIG_GPIO_STATUS_LED_READY_PIN, state & 1);
    gpio_set_level(CONFIG_GPIO_STATUS_LED_RX_PIN, (state & 2) >> 1);   
}