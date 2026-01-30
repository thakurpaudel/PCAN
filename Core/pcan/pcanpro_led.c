#include "gpio.h"
#include "main.h"
#include "pcanpro_led.h"
#include "pcanpro_timestamp.h"
#include "pcanpro_variant.h"
#include <assert.h>

static struct {
  uint32_t arg;
  uint32_t delay;
  uint32_t timestamp;
  uint16_t mode;
  uint8_t state;
} led_mode_array[LED_TOTAL] = {0};

void pcan_led_init(void) {
  // GPIO initialization is handled by CubeMX in MX_GPIO_Init()
  // Just ensure LEDs are in known state
  IO_HW_INIT();

  // Set all LEDs to OFF state
  HAL_GPIO_WritePin(LED_STAT_PORT, LED_STAT_PIN, LED_OFF);
  HAL_GPIO_WritePin(LED_CH0_TX_PORT, LED_CH0_TX_PIN, LED_OFF);
  HAL_GPIO_WritePin(LED_CH0_RX_PORT, LED_CH0_RX_PIN, LED_OFF);
  HAL_GPIO_WritePin(LED_CH1_TX_PORT, LED_CH1_TX_PIN, LED_OFF);
  HAL_GPIO_WritePin(LED_CH1_RX_PORT, LED_CH1_RX_PIN, LED_OFF);
}

void pcan_led_set_mode(int led, int mode, uint32_t arg) {
  assert(led < LED_TOTAL);

  led_mode_array[led].mode = mode;
  if (!led_mode_array[led].timestamp) {
    led_mode_array[led].timestamp = pcan_timestamp_millis();
  }
  led_mode_array[led].delay = 0;

  /* set guard time */
  if (mode == LED_MODE_BLINK_FAST || mode == LED_MODE_BLINK_SLOW) {
    led_mode_array[led].delay = (mode == LED_MODE_BLINK_FAST) ? 50 : 200;
    if (arg != 0xFFFFFFFF) {
      /* update to absolute */
      arg = pcan_timestamp_millis() + arg;
    }
  }

  led_mode_array[led].arg = arg;
}

static void _led_update_state(int led, uint8_t state) {
  GPIO_PinState pin_state = state ? LED_ON : LED_OFF;

  switch (led) {
  case LED_STAT:
    HAL_GPIO_WritePin(LED_STAT_PORT, LED_STAT_PIN, pin_state);
    break;

  case LED_CH0_TX:
    HAL_GPIO_WritePin(LED_CH0_TX_PORT, LED_CH0_TX_PIN, pin_state);
    break;

  case LED_CH0_RX:
    HAL_GPIO_WritePin(LED_CH0_RX_PORT, LED_CH0_RX_PIN, pin_state);
    break;

  case LED_CH1_TX:
    HAL_GPIO_WritePin(LED_CH1_TX_PORT, LED_CH1_TX_PIN, pin_state);
    break;

  case LED_CH1_RX:
    HAL_GPIO_WritePin(LED_CH1_RX_PORT, LED_CH1_RX_PIN, pin_state);
    break;
  }
}

void pcan_led_poll(void) {
  uint32_t ts_ms = pcan_timestamp_millis();

  for (int i = 0; i < LED_TOTAL; i++) {
    if (!led_mode_array[i].timestamp)
      continue;
    if ((uint32_t)(ts_ms - led_mode_array[i].timestamp) <
        led_mode_array[i].delay)
      continue;

    switch (led_mode_array[i].mode) {
    default:
    case LED_MODE_DEVICE:
      led_mode_array[i].timestamp = 0;
      break;
    case LED_MODE_OFF:
    case LED_MODE_ON:
      led_mode_array[i].state = (led_mode_array[i].mode == LED_MODE_ON);
      led_mode_array[i].timestamp = 0;
      break;
    case LED_MODE_BLINK_FAST:
    case LED_MODE_BLINK_SLOW:
      led_mode_array[i].state ^= 1;
      led_mode_array[i].timestamp += led_mode_array[i].delay;
      if ((led_mode_array[i].arg != 0xFFFFFFFF) &&
          (led_mode_array[i].arg <= ts_ms)) {
        pcan_led_set_mode(i, LED_MODE_OFF, 0);
      }
      break;
    }

    _led_update_state(i, led_mode_array[i].state);
  }
}
