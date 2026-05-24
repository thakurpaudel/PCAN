#pragma once
#include <stdint.h>

enum {
    LED_STAT,
    LED_CH0_RX,
    LED_CH0_TX,
    LED_CH1_RX,
    LED_CH1_TX
};

#define LED_MODE_BLINK_SLOW 1
#define LED_MODE_BLINK_FAST 2

static inline void pcan_led_set_mode(int led, int mode, uint32_t arg) {
    // Stub
}
